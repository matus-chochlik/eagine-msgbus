/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/local/stream_protocol.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <cassert>

module eagine.msgbus.core;
import eagine.core.debug;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.string;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.serialization;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.main_ctx;
import std;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
template <connection_addr_kind, connection_protocol>
struct asio_types;

template <connection_addr_kind Kind, connection_protocol Proto>
using asio_socket_type = typename asio_types<Kind, Proto>::socket_type;

template <connection_addr_kind Kind, connection_protocol Proto>
using asio_endpoint_type =
  typename asio_socket_type<Kind, Proto>::endpoint_type;

template <typename Base, connection_addr_kind, connection_protocol>
class asio_connection_info;

template <connection_addr_kind, connection_protocol>
class asio_connection;

template <connection_addr_kind, connection_protocol>
class asio_connector;

template <connection_addr_kind, connection_protocol>
class asio_acceptor;
//------------------------------------------------------------------------------
template <typename Socket>
class asio_flushing_sockets {
public:
    void adopt(Socket& sckt) noexcept {
        _waiting.emplace_back(std::chrono::seconds(10), std::move(sckt));
    }

    auto empty() const noexcept -> bool {
        return _waiting.empty();
    }

    void update() noexcept {
        erase_if(
          _waiting, [](auto& waiting) { return bool(std::get<0>(waiting)); });
    }

private:
    std::vector<std::tuple<timeout, Socket>> _waiting;
};
//------------------------------------------------------------------------------
struct asio_common_state {
    asio::io_context context;

    asio_common_state() = default;
    asio_common_state(asio_common_state&&) = delete;
    asio_common_state(const asio_common_state&) = delete;
    auto operator=(asio_common_state&&) = delete;
    auto operator=(const asio_common_state&) = delete;

    ~asio_common_state() noexcept {
        while(has_flushing()) {
            update();
            std::this_thread::yield();
        }
    }

    template <typename Socket>
    void adopt_flushing(Socket& sckt) noexcept {
        std::get<asio_flushing_sockets<Socket>>(_flushing).adopt(sckt);
    }

    void update() noexcept {
        _update_flushing(_flushing);
    }

    auto has_flushing() const noexcept -> bool {
        return _has_flushing(_flushing);
    }

private:
    template <typename Tup, std::size_t... I>
    static void _do_update_flushing(
      Tup& flushing,
      const std::index_sequence<I...>) noexcept {
        (..., std::get<I>(flushing).update());
    }

    template <typename Tup>
    static void _update_flushing(Tup& flushing) noexcept {
        _do_update_flushing(
          flushing, std::make_index_sequence<std::tuple_size_v<Tup>>());
    }

    template <typename Tup, std::size_t... I>
    static auto _does_have_flushing(
      Tup& flushing,
      const std::index_sequence<I...>) noexcept -> bool {
        return (false or ... or not std::get<I>(flushing).empty());
    }

    template <typename Tup>
    static auto _has_flushing(Tup& flushing) noexcept -> bool {
        return _does_have_flushing(
          flushing, std::make_index_sequence<std::tuple_size_v<Tup>>());
    }

    std::tuple<
#if defined(ASIO_HAS_LOCAL_SOCKETS)
      asio_flushing_sockets<asio::local::stream_protocol::socket>,
#endif
      asio_flushing_sockets<asio::ip::tcp::socket>,
      asio_flushing_sockets<asio::ip::udp::socket>>
      _flushing;
};
//------------------------------------------------------------------------------
template <connection_addr_kind Kind, connection_protocol Proto>
struct asio_connection_group : interface<asio_connection_group<Kind, Proto>> {

    using endpoint_type = asio_endpoint_type<Kind, Proto>;

    virtual auto pack_into(endpoint_type&, memory::block) noexcept
      -> message_pack_info = 0;

    virtual void on_sent(
      const endpoint_type&,
      const message_pack_info& to_be_removed) noexcept = 0;

    virtual void on_received(
      const endpoint_type&,
      const memory::const_block) noexcept = 0;

    virtual auto has_received() noexcept -> bool = 0;
};
//------------------------------------------------------------------------------
struct asio_connection_state_base
  : std::enable_shared_from_this<asio_connection_state_base>
  , main_ctx_object {
    using clock_type = std::chrono::steady_clock;
    using clock_time = typename clock_type::time_point;

    const std::shared_ptr<asio_common_state> common;
    const memory::buffer push_buffer{};
    const memory::buffer read_buffer{};
    const memory::buffer write_buffer{};
    span_size_t total_used_size{0};
    span_size_t total_sent_size{0};
    clock_time send_start_time{clock_type::now()};
    std::int32_t total_sent_messages{0};
    std::int32_t total_sent_blocks{0};
    float usage_ratio{-1.F};
    float used_per_sec{-1.F};
    bool is_sending{false};
    bool is_recving{false};

    asio_connection_state_base(
      main_ctx_parent parent,
      std::shared_ptr<asio_common_state> asio_state,
      const span_size_t block_size) noexcept
      : main_ctx_object{"AsioConnSt", parent}
      , common{std::move(asio_state)}
      , push_buffer{block_size, max_span_align()}
      , read_buffer{block_size, max_span_align()}
      , write_buffer{block_size, max_span_align()} {
        assert(common);
        common->update();

        assert(block_size >= min_connection_data_size);
        zero(cover(push_buffer));
        zero(cover(read_buffer));
        zero(cover(write_buffer));

        log_debug("allocating write buffer of ${size}")
          .arg("size", "ByteSize", write_buffer.size());
        log_debug("allocating read buffer of ${size}")
          .arg("size", "ByteSize", read_buffer.size());
    }

    auto self_ref() noexcept {
        return this->shared_from_this();
    }
};
//------------------------------------------------------------------------------
template <connection_addr_kind Kind, connection_protocol Proto>
struct asio_connection_state : asio_connection_state_base {
    using endpoint_type = asio_endpoint_type<Kind, Proto>;
    using clock_type = std::chrono::steady_clock;
    using clock_time = typename clock_type::time_point;

    asio_socket_type<Kind, Proto> socket;
    endpoint_type conn_endpoint{};

    asio_connection_state(
      main_ctx_parent parent,
      std::shared_ptr<asio_common_state> asio_state,
      asio_socket_type<Kind, Proto> sock,
      const span_size_t block_size) noexcept
      : asio_connection_state_base{parent, std::move(asio_state), block_size}
      , socket{std::move(sock)} {}

    asio_connection_state(
      main_ctx_parent parent,
      const std::shared_ptr<asio_common_state>& asio_state,
      const span_size_t block_size) noexcept
      : asio_connection_state{
          parent,
          asio_state,
          asio_socket_type<Kind, Proto>{asio_state->context},
          block_size} {}

    auto is_usable() const noexcept -> bool {
        if(common) [[likely]] {
            return socket.is_open();
        }
        return false;
    }

    auto log_usage_stats(const span_size_t threshold = 0) noexcept -> bool {
        if(total_sent_size >= threshold) [[unlikely]] {
            usage_ratio = float(total_used_size) / float(total_sent_size);
            const auto slack = 1.F - usage_ratio;
            const auto msgs_per_block =
              total_sent_blocks
                ? float(total_sent_messages) / float(total_sent_blocks)
                : 0.F;
            used_per_sec =
              float(total_used_size) /
              std::chrono::duration<float>(clock_type::now() - send_start_time)
                .count();
            const auto sent_per_sec =
              float(total_sent_size) /
              std::chrono::duration<float>(clock_type::now() - send_start_time)
                .count();

            log_stat("message slack ratio: ${slack}")
              .tag("msgSlack")
              .arg("usedSize", "ByteSize", total_used_size)
              .arg("sentSize", "ByteSize", total_sent_size)
              .arg("msgsPerBlk", msgs_per_block)
              .arg("usedPerSec", "ByteSize", used_per_sec)
              .arg("sentPerSec", "ByteSize", sent_per_sec)
              .arg("addrKind", Kind)
              .arg("protocol", Proto)
              .arg("slack", "Ratio", slack);
            return true;
        }
        return false;
    }

    template <typename Handler>
    void do_start_send(
      const stream_protocol_tag,
      const endpoint_type&,
      const memory::const_block blk,
      const Handler handler) noexcept {
        asio::async_write(
          socket, asio::buffer(blk.data(), blk.size()), handler);
    }

    template <typename Handler>
    void do_start_send(
      const datagram_protocol_tag,
      const endpoint_type& target_endpoint,
      const memory::const_block blk,
      const Handler handler) noexcept {
        socket.async_send_to(
          asio::buffer(blk.data(), blk.size()), target_endpoint, handler);
    }

    void handle_send_error(const std::error_code error) noexcept {
        log_error("failed to send data: ${error}").arg("error", error.message());
        this->is_sending = false;
        this->socket.close();
    }

    void do_start_send(asio_connection_group<Kind, Proto>& group) noexcept {
        using std::get;

        endpoint_type target_endpoint{conn_endpoint};
        const auto packed =
          group.pack_into(target_endpoint, cover(write_buffer));
        if(not packed.is_empty()) {
            is_sending = true;

            do_start_send(
              connection_protocol_tag<Proto>{},
              target_endpoint,
              view(write_buffer),
              [this, &group, target_endpoint, packed, self{self_ref()}](
                const std::error_code error,
                [[maybe_unused]] const std::size_t length) {
                  if(not error) [[likely]] {
                      assert(span_size(length) == packed.total());
                      log_trace("sent data")
                        .arg("usedSize", "ByteSize", packed.used())
                        .arg("sentSize", "ByteSize", packed.total());

                      total_used_size += packed.used();
                      total_sent_size += packed.total();
                      total_sent_messages += packed.count();
                      total_sent_blocks += 1;

                      if(this->log_usage_stats(span_size(2U << 27U)))
                        [[unlikely]] {
                          total_used_size = 0;
                          total_sent_size = 0;
                          send_start_time = clock_type::now();
                      }

                      this->handle_sent(group, target_endpoint, packed);
                  } else {
                      this->handle_send_error(error);
                  }
              });
        } else {
            is_sending = false;
        }
    }

    auto start_send(asio_connection_group<Kind, Proto>& group) noexcept
      -> bool {
        if(not is_sending) {
            do_start_send(group);
        }
        return is_sending;
    }

    void handle_sent(
      asio_connection_group<Kind, Proto>& group,
      const endpoint_type& target_endpoint,
      const message_pack_info& to_be_removed) noexcept {
        group.on_sent(target_endpoint, to_be_removed);
        do_start_send(group);
    }

    template <typename Handler>
    void do_start_receive(
      const stream_protocol_tag,
      memory::block blk,
      const Handler handler) noexcept {
        asio::async_read(socket, asio::buffer(blk.data(), blk.size()), handler);
    }

    template <typename Handler>
    void do_start_receive(
      const datagram_protocol_tag,
      memory::block blk,
      const Handler handler) noexcept {
        socket.async_receive_from(
          asio::buffer(blk.data(), blk.size()), conn_endpoint, handler);
    }

    void handle_receive_error(
      memory::const_block rcvd,
      asio_connection_group<Kind, Proto>& group,
      const std::error_code error) noexcept {
        if(rcvd) {
            log_warning("failed receiving data: ${error}")
              .arg("error", error.message());
            this->handle_received(rcvd, group);
        } else {
            if(error == asio::error::eof) {
                log_debug("received end-of-file");
            } else if(error == asio::error::connection_reset) {
                log_debug("connection reset by peer");
            } else {
                log_error("failed to receive data: ${error}")
                  .arg("error", error.message());
            }
        }
        this->is_recving = false;
        this->socket.close();
    }

    void do_start_receive(asio_connection_group<Kind, Proto>& group) noexcept {
        auto blk = cover(read_buffer);

        is_recving = true;
        do_start_receive(
          connection_protocol_tag<Proto>{},
          blk,
          [this, &group, selfref{self_ref()}, blk](
            const std::error_code error, const std::size_t length) {
              memory::const_block rcvd{head(blk, span_size(length))};
              if(not error) [[likely]] {
                  log_trace("received data (size: ${size})")
                    .arg("size", "ByteSize", length);

                  this->handle_received(rcvd, group);
              } else {
                  this->handle_receive_error(rcvd, group, error);
              }
          });
    }

    auto start_receive(asio_connection_group<Kind, Proto>& group) noexcept
      -> bool {
        if(not is_recving) {
            do_start_receive(group);
        }
        return group.has_received();
    }

    void handle_received(
      const memory::const_block data,
      asio_connection_group<Kind, Proto>& group) noexcept {
        group.on_received(conn_endpoint, data);
        do_start_receive(group);
    }

    auto update() noexcept -> work_done {
        some_true something_done{};
        if(const auto count{common->context.poll()}) {
            something_done();
        } else {
            common->context.reset();
        }
        return something_done;
    }

    void cleanup(asio_connection_group<Kind, Proto>& group) noexcept {
        log_usage_stats();
        while(is_usable() and start_send(group)) {
            log_debug("flushing connection outbox");
            update();
        }
        if(is_usable()) {
            common->adopt_flushing(socket);
        }
        common->update();
    }
};
//------------------------------------------------------------------------------
template <connection_addr_kind Kind, connection_protocol Proto>
class asio_connection_base
  : public asio_connection_info<connection, Kind, Proto>
  , public main_ctx_object {
public:
    asio_connection_base(
      main_ctx_parent parent,
      std::shared_ptr<asio_common_state> asio_state,
      const span_size_t block_size) noexcept
      : main_ctx_object{"AsioConnBs", parent}
      , _state{std::make_shared<asio_connection_state<Kind, Proto>>(
          *this,
          std::move(asio_state),
          block_size)} {
        assert(_state);
    }

    asio_connection_base(
      main_ctx_parent parent,
      std::shared_ptr<asio_common_state> asio_state,
      asio_socket_type<Kind, Proto> socket,
      const span_size_t block_size) noexcept
      : main_ctx_object{"AsioConnBs", parent}
      , _state{std::make_shared<asio_connection_state<Kind, Proto>>(
          *this,
          std::move(asio_state),
          std::move(socket),
          block_size)} {
        assert(_state);
    }

    inline auto conn_state() noexcept -> auto& {
        assert(_state);
        return *_state;
    }

    auto max_data_size() noexcept -> valid_if_positive<span_size_t> final {
        return {conn_state().write_buffer.size()};
    }

    auto is_usable() noexcept -> bool final {
        return conn_state().is_usable();
    }

protected:
    const std::shared_ptr<asio_connection_state<Kind, Proto>> _state;

    asio_connection_base(
      main_ctx_parent parent,
      std::shared_ptr<asio_connection_state<Kind, Proto>> state)
      : main_ctx_object{"AsioConnBs", parent}
      , _state{std::move(state)} {}
};
//------------------------------------------------------------------------------
template <connection_addr_kind Kind, connection_protocol Proto>
class asio_connection
  : public asio_connection_base<Kind, Proto>
  , public asio_connection_group<Kind, Proto> {

    using base = asio_connection_base<Kind, Proto>;
    using endpoint_type = asio_endpoint_type<Kind, Proto>;

public:
    using base::base;
    using base::conn_state;

    auto update() noexcept -> work_done override {
        some_true something_done{};
        if(conn_state().socket.is_open()) [[likely]] {
            something_done(conn_state().start_receive(*this));
            something_done(conn_state().start_send(*this));
        }
        something_done(conn_state().update());
        return something_done;
    }

    auto pack_into(endpoint_type&, memory::block data) noexcept
      -> message_pack_info final {
        return _outgoing.pack_into(data);
    }

    void on_sent(
      const endpoint_type&,
      const message_pack_info& to_be_removed) noexcept final {
        return _outgoing.cleanup(to_be_removed);
    }

    void on_received(const endpoint_type&, memory::const_block data) noexcept
      final {
        return _incoming.push(data);
    }

    auto has_received() noexcept -> bool final {
        return not _incoming.empty();
    }

    auto send(const message_id msg_id, const message_view& message) noexcept
      -> bool final {
        return _outgoing.enqueue(
          *this, msg_id, message, cover(conn_state().push_buffer));
    }

    auto fetch_messages(const connection::fetch_handler handler) noexcept
      -> work_done final {
        return _incoming.fetch_messages(*this, handler);
    }

    auto query_statistics(connection_statistics& stats) noexcept -> bool final {
        auto& state = conn_state();
        stats.block_usage_ratio = state.usage_ratio;
        stats.bytes_per_second = state.used_per_sec;
        return true;
    }

    void cleanup() noexcept final {
        const timeout too_long{std::chrono::seconds{5}};
        while(not _outgoing.empty() and not too_long) {
            if(conn_state().socket.is_open()) {
                if(not conn_state().start_send(*this)) {
                    break;
                }
            }
            conn_state().update();
        }
        conn_state().cleanup(*this);
        _outgoing.log_stats(*this);
        _incoming.log_stats(*this);
    }

private:
    connection_outgoing_messages _outgoing{};
    connection_incoming_messages _incoming{};
};
//------------------------------------------------------------------------------
template <connection_addr_kind Kind>
class asio_datagram_client_connection
  : public asio_connection_base<Kind, connection_protocol::datagram> {

    using base = asio_connection_base<Kind, connection_protocol::datagram>;

public:
    using base::conn_state;

    asio_datagram_client_connection(
      main_ctx_parent parent,
      std::shared_ptr<asio_connection_state<Kind, connection_protocol::datagram>>
        state,
      std::shared_ptr<connection_outgoing_messages> outgoing,
      std::shared_ptr<connection_incoming_messages> incoming) noexcept
      : base(parent, std::move(state))
      , _outgoing{std::move(outgoing)}
      , _incoming{std::move(incoming)} {}

    auto pack_into(memory::block data) noexcept -> message_pack_info {
        assert(_outgoing);
        return _outgoing->pack_into(data);
    }

    void on_sent(const message_pack_info& to_be_removed) noexcept {
        assert(_outgoing);
        return _outgoing->cleanup(to_be_removed);
    }

    void on_received(const memory::const_block data) noexcept {
        assert(_incoming);
        _incoming->push(data);
    }

    auto send(const message_id msg_id, const message_view& message) noexcept
      -> bool final {
        assert(_outgoing);
        return _outgoing->enqueue(
          *this, msg_id, message, cover(conn_state().push_buffer));
    }

    auto fetch_messages(const connection::fetch_handler handler) noexcept
      -> work_done final {
        assert(_incoming);
        return _incoming->fetch_messages(*this, handler);
    }

    auto query_statistics(connection_statistics& stats) noexcept -> bool final {
        auto& state = conn_state();
        stats.block_usage_ratio = state.usage_ratio;
        stats.bytes_per_second = state.used_per_sec;
        return true;
    }

    auto update() noexcept -> work_done final {
        some_true something_done{};
        something_done(conn_state().update());
        return something_done;
    }

    void cleanup() noexcept override {
        _outgoing->log_stats(*this);
        _incoming->log_stats(*this);
    }

private:
    std::shared_ptr<connection_outgoing_messages> _outgoing;
    std::shared_ptr<connection_incoming_messages> _incoming;
};
//------------------------------------------------------------------------------
template <connection_addr_kind Kind>
class asio_datagram_server_connection
  : public asio_connection_base<Kind, connection_protocol::datagram>
  , public asio_connection_group<Kind, connection_protocol::datagram> {

    using base = asio_connection_base<Kind, connection_protocol::datagram>;
    using endpoint_type =
      asio_endpoint_type<Kind, connection_protocol::datagram>;

public:
    using base::base;
    using base::conn_state;

    auto pack_into(endpoint_type& target, memory::block dest) noexcept
      -> message_pack_info final {
        assert(_index >= 0);
        const auto prev_idx{_index};
        do {
            if(_index < span_size(_current.size())) {
                auto pos = _current.begin();
                std::advance(pos, _index);
                ++_index;
                auto& [ep, out_in] = *pos;
                auto& outgoing = std::get<0>(out_in);
                assert(outgoing);
                const auto packed = outgoing->pack_into(dest);
                if(not packed.is_empty()) {
                    target = ep;
                    return packed;
                }
            } else {
                _index = 0;
            }
        } while(_index != prev_idx);
        return {0};
    }

    void on_sent(
      const endpoint_type& ep,
      const message_pack_info& to_be_removed) noexcept final {
        _outgoing(ep).cleanup(to_be_removed);
    }

    void on_received(
      const endpoint_type& ep,
      const memory::const_block data) noexcept final {
        _incoming(ep).push(data);
    }

    auto has_received() noexcept -> bool final {
        for(auto m : {&_current, &_pending}) {
            for(const auto& p : *m) {
                const auto& incoming = std::get<1>(std::get<1>(p));
                assert(incoming);
                if(not incoming->empty()) {
                    return true;
                }
            }
        }
        return false;
    }

    auto send(const message_id, const message_view&) noexcept -> bool final {
        unreachable();
        return false;
    }

    auto fetch_messages(const connection::fetch_handler) noexcept
      -> work_done final {
        unreachable();
        return {};
    }

    auto query_statistics(connection_statistics&) noexcept -> bool final {
        return false;
    }

    auto process_accepted(const acceptor::accept_handler handler) noexcept
      -> work_done {
        some_true something_done;
        for(auto& p : _pending) {
            handler(std::make_unique<asio_datagram_client_connection<Kind>>(
              *this,
              this->_state,
              std::get<0>(std::get<1>(p)),
              std::get<1>(std::get<1>(p))));
            _current.insert(p);
            something_done();
        }
        _pending.clear();
        if(something_done) {
            this->log_debug("accepted datagram endpoints")
              .arg("current", _current.size());
        }
        return something_done;
    }

    auto update() noexcept -> work_done final {
        some_true something_done{};
        if(conn_state().socket.is_open()) [[likely]] {
            something_done(conn_state().start_receive(*this));
            something_done(conn_state().start_send(*this));
        } else {
            this->log_warning("datagram socket is not open");
        }
        something_done(conn_state().update());
        return something_done;
    }

    void cleanup() noexcept final {
        base::cleanup();
        conn_state().cleanup(*this);
    }

private:
    auto _get(const endpoint_type& ep) noexcept -> auto& {
        auto pos = _current.find(ep);
        if(pos == _current.end()) {
            pos = _pending.find(ep);
            if(pos == _pending.end()) {
                pos = _pending
                        .try_emplace(
                          ep,
                          std::make_shared<connection_outgoing_messages>(),
                          std::make_shared<connection_incoming_messages>())
                        .first;
                this->log_debug("added pending datagram endpoint")
                  .arg("pending", _pending.size())
                  .arg("current", _current.size());
            }
        }
        return std::get<1>(*pos);
    }

    auto _outgoing(const endpoint_type& ep) noexcept
      -> connection_outgoing_messages& {
        auto& outgoing = std::get<0>(_get(ep));
        assert(outgoing);
        return *outgoing;
    }

    auto _incoming(const endpoint_type& ep) noexcept
      -> connection_incoming_messages& {
        auto& incoming = std::get<1>(_get(ep));
        assert(incoming);
        return *incoming;
    }

    flat_map<
      endpoint_type,
      std::tuple<
        std::shared_ptr<connection_outgoing_messages>,
        std::shared_ptr<connection_incoming_messages>>>
      _current{}, _pending{};
    span_size_t _index{0};
};
//------------------------------------------------------------------------------
// TCP/IPv4
//------------------------------------------------------------------------------
template <typename Base>
class asio_connection_info<
  Base,
  connection_addr_kind::ipv4,
  connection_protocol::stream> : public Base {
public:
    auto kind() noexcept -> connection_kind final {
        return connection_kind::remote_interprocess;
    }

    auto addr_kind() noexcept -> connection_addr_kind final {
        return connection_addr_kind::ipv4;
    }

    auto type_id() noexcept -> identifier final {
        return "AsioTcpIp4";
    }
};
//------------------------------------------------------------------------------
template <>
struct asio_types<connection_addr_kind::ipv4, connection_protocol::stream> {
    using socket_type = asio::ip::tcp::socket;
};
//------------------------------------------------------------------------------
template <>
class asio_connector<connection_addr_kind::ipv4, connection_protocol::stream>
  : public asio_connection<
      connection_addr_kind::ipv4,
      connection_protocol::stream> {

    using base =
      asio_connection<connection_addr_kind::ipv4, connection_protocol::stream>;
    using base::conn_state;

public:
    asio_connector(
      main_ctx_parent parent,
      const std::shared_ptr<asio_common_state>& asio_state,
      const string_view addr_str,
      const span_size_t block_size) noexcept
      : base{parent, asio_state, block_size}
      , _resolver{asio_state->context}
      , _addr{parse_ipv4_addr(addr_str)} {}

    auto update() noexcept -> work_done final {
        some_true something_done{};
        if(conn_state().socket.is_open()) [[likely]] {
            something_done(conn_state().start_receive(*this));
            something_done(conn_state().start_send(*this));
        } else if(not _connecting) {
            if(_should_reconnect) {
                _should_reconnect.reset();
                _start_resolve();
                something_done();
            }
        }
        something_done(conn_state().update());
        return something_done;
    }

private:
    asio::ip::tcp::resolver _resolver;
    std::tuple<std::string, ipv4_port> _addr;
    timeout _should_reconnect{
      adjusted_duration(std::chrono::seconds{1}),
      nothing};
    bool _connecting{false};

    void _start_connect(
      asio::ip::tcp::resolver::iterator resolved,
      const ipv4_port port) noexcept {
        auto& ep = conn_state().conn_endpoint = *resolved;
        ep.port(port);

        this->log_debug("connecting to ${host}:${port}")
          .arg("host", "IpV4Host", std::get<0>(_addr))
          .arg("port", "IpV4Port", std::get<1>(_addr));

        conn_state().socket.async_connect(
          ep, [this, resolved, port](const std::error_code error) mutable {
              if(not error) {
                  this->log_debug("connected on address ${host}:${port}")
                    .arg("host", "IpV4Host", std::get<0>(_addr))
                    .arg("port", "IpV4Port", std::get<1>(_addr));
                  this->_connecting = false;
              } else {
                  if(++resolved != asio::ip::tcp::resolver::iterator{}) {
                      this->_start_connect(resolved, port);
                  } else {
                      this
                        ->log_error(
                          "failed to connect on address "
                          "${address}:${port}: "
                          "${error}")
                        .arg("error", error.message())
                        .arg("host", "IpV4Host", std::get<0>(_addr))
                        .arg("port", "IpV4Port", std::get<1>(_addr));
                      this->_connecting = false;
                  }
              }
          });
    }

    void _start_resolve() noexcept {
        _connecting = true;
        auto& [host, port] = _addr;
        _resolver.async_resolve(
          asio::string_view(host.data(), integer(host.size())),
          {},
          [this, port{port}](const std::error_code error, auto resolved) {
              if(not error) {
                  this->_start_connect(resolved, port);
              } else {
                  this->log_error("failed to resolve address: ${error}")
                    .arg("error", error.message());
                  this->_connecting = false;
              }
          });
    }
};
//------------------------------------------------------------------------------
template <>
class asio_acceptor<connection_addr_kind::ipv4, connection_protocol::stream>
  : public asio_connection_info<
      acceptor,
      connection_addr_kind::ipv4,
      connection_protocol::stream>
  , public main_ctx_object {
public:
    asio_acceptor(
      main_ctx_parent parent,
      std::shared_ptr<asio_common_state> asio_state,
      const string_view addr_str,
      const span_size_t block_size) noexcept
      : main_ctx_object{"AsioAccptr", parent}
      , _asio_state{std::move(asio_state)}
      , _addr{parse_ipv4_addr(addr_str)}
      , _acceptor{_asio_state->context}
      , _socket{_asio_state->context}
      , _block_size{block_size} {}

    auto update() noexcept -> work_done final {
        assert(this->_asio_state);
        some_true something_done{};
        if(not _acceptor.is_open()) [[unlikely]] {
            asio::ip::tcp::endpoint endpoint(
              asio::ip::tcp::v4(), std::get<1>(_addr));
            _acceptor.open(endpoint.protocol());
            _acceptor.bind(endpoint);
            _acceptor.listen();
            _start_accept();
            something_done();
        }
        if(this->_asio_state->context.poll()) {
            something_done();
        } else {
            this->_asio_state->context.reset();
        }
        return something_done;
    }

    auto process_accepted(const accept_handler handler) noexcept
      -> work_done final {
        some_true something_done{};
        for(auto& socket : _accepted) {
            auto conn = std::make_unique<asio_connection<
              connection_addr_kind::ipv4,
              connection_protocol::stream>>(
              *this, _asio_state, std::move(socket), _block_size);
            handler(std::move(conn));
            something_done();
        }
        _accepted.clear();
        return something_done;
    }

private:
    const std::shared_ptr<asio_common_state> _asio_state;
    const std::tuple<std::string, ipv4_port> _addr;
    asio::ip::tcp::acceptor _acceptor;
    asio::ip::tcp::socket _socket;
    const span_size_t _block_size;

    std::vector<asio::ip::tcp::socket> _accepted;

    void _start_accept() noexcept {
        log_debug("accepting connection on address ${host}:${port}")
          .arg("host", "IpV4Host", std::get<0>(_addr))
          .arg("port", "IpV4Port", std::get<1>(_addr));

        _socket = asio::ip::tcp::socket(this->_asio_state->context);
        _acceptor.async_accept(_socket, [this](const std::error_code error) {
            if(not error) {
                log_debug("accepted connection on address ${host}:${port}")
                  .arg("host", "IpV4Host", std::get<0>(_addr))
                  .arg("port", "IpV4Port", std::get<1>(_addr));
                this->_accepted.emplace_back(std::move(this->_socket));
            } else {
                log_error(
                  "failed to accept connection on address "
                  "${host}:${port}: "
                  "${error}")
                  .arg("error", error.message())
                  .arg("host", "IpV4Host", std::get<0>(_addr))
                  .arg("port", "IpV4Port", std::get<1>(_addr));
            }
            _start_accept();
        });
    }
};
//------------------------------------------------------------------------------
// UDP/IPv4
//------------------------------------------------------------------------------
template <typename Base>
class asio_connection_info<
  Base,
  connection_addr_kind::ipv4,
  connection_protocol::datagram> : public Base {
public:
    auto kind() noexcept -> connection_kind final {
        return connection_kind::remote_interprocess;
    }

    auto addr_kind() noexcept -> connection_addr_kind final {
        return connection_addr_kind::ipv4;
    }

    auto type_id() noexcept -> identifier final {
        return "AsioUdpIp4";
    }
};
//------------------------------------------------------------------------------
template <>
struct asio_types<connection_addr_kind::ipv4, connection_protocol::datagram> {
    using socket_type = asio::ip::udp::socket;
};
//------------------------------------------------------------------------------
template <>
class asio_connector<connection_addr_kind::ipv4, connection_protocol::datagram>
  : public asio_connection<
      connection_addr_kind::ipv4,
      connection_protocol::datagram> {

    using base =
      asio_connection<connection_addr_kind::ipv4, connection_protocol::datagram>;
    using base::conn_state;

public:
    asio_connector(
      main_ctx_parent parent,
      const std::shared_ptr<asio_common_state>& asio_state,
      const string_view addr_str,
      const span_size_t block_size) noexcept
      : base{parent, asio_state, block_size}
      , _resolver{asio_state->context}
      , _addr{parse_ipv4_addr(addr_str)} {}

    auto update() noexcept -> work_done final {
        some_true something_done{};
        if(conn_state().socket.is_open()) [[likely]] {
            something_done(conn_state().start_receive(*this));
            something_done(conn_state().start_send(*this));
        } else if(not _establishing) {
            if(_should_reconnect) {
                _should_reconnect.reset();
                _start_resolve();
                something_done();
            }
        }
        something_done(conn_state().update());
        return something_done;
    }

private:
    asio::ip::udp::resolver _resolver;
    std::tuple<std::string, ipv4_port> _addr;
    timeout _should_reconnect{
      adjusted_duration(std::chrono::seconds{1}, memory_access_rate::low),
      nothing};
    bool _establishing{false};

    void _on_resolve(
      const asio::ip::udp::resolver::iterator& resolved,
      const ipv4_port port) noexcept {
        auto& ep = conn_state().conn_endpoint = *resolved;
        ep.port(port);
        conn_state().socket.open(ep.protocol());
        this->_establishing = false;

        this->log_debug("resolved address ${host}:${port}")
          .arg("host", "IpV4Host", std::get<0>(_addr))
          .arg("port", "IpV4Port", std::get<1>(_addr));
    }

    void _start_resolve() noexcept {
        _establishing = true;
        const auto& [host, port] = _addr;
        _resolver.async_resolve(
          host, {}, [this, port{port}](std::error_code error, auto resolved) {
              if(not error) {
                  this->_on_resolve(resolved, port);
              } else {
                  this->log_error("failed to resolve address: ${error}")
                    .arg("error", error.message());
                  this->_establishing = false;
              }
          });
    }
};
//------------------------------------------------------------------------------
template <>
class asio_acceptor<connection_addr_kind::ipv4, connection_protocol::datagram>
  : public asio_connection_info<
      acceptor,
      connection_addr_kind::ipv4,
      connection_protocol::datagram>
  , public main_ctx_object {

public:
    asio_acceptor(
      main_ctx_parent parent,
      std::shared_ptr<asio_common_state> asio_state,
      const string_view addr_str,
      const span_size_t block_size) noexcept
      : main_ctx_object{"AsioAccptr", parent}
      , _asio_state{std::move(asio_state)}
      , _addr{parse_ipv4_addr(addr_str)}
      , _conn{
          *this,
          _asio_state,
          asio::ip::udp::socket{
            _asio_state->context,
            asio::ip::udp::endpoint{asio::ip::udp::v4(), std::get<1>(_addr)}},
          block_size} {}

    auto update() noexcept -> work_done final {
        return _conn.update();
    }

    auto process_accepted(const accept_handler handler) noexcept
      -> work_done final {
        return _conn.process_accepted(handler);
    }

private:
    const std::shared_ptr<asio_common_state> _asio_state;
    const std::tuple<std::string, ipv4_port> _addr;

    asio_datagram_server_connection<connection_addr_kind::ipv4> _conn;
};
//------------------------------------------------------------------------------
// Local/Stream
#if ASIO_HAS_LOCAL_SOCKETS
//------------------------------------------------------------------------------
template <typename Base>
class asio_connection_info<
  Base,
  connection_addr_kind::filepath,
  connection_protocol::stream> : public Base {
public:
    auto kind() noexcept -> connection_kind final {
        return connection_kind::local_interprocess;
    }

    auto addr_kind() noexcept -> connection_addr_kind final {
        return connection_addr_kind::filepath;
    }

    auto type_id() noexcept -> identifier final {
        return "AsioLclStr";
    }
};
//------------------------------------------------------------------------------
template <>
struct asio_types<connection_addr_kind::filepath, connection_protocol::stream> {
    using socket_type = asio::local::stream_protocol::socket;
};
//------------------------------------------------------------------------------
template <>
class asio_connector<connection_addr_kind::filepath, connection_protocol::stream>
  : public asio_connection<
      connection_addr_kind::filepath,
      connection_protocol::stream> {
    using base = asio_connection<
      connection_addr_kind::filepath,
      connection_protocol::stream>;

public:
    asio_connector(
      main_ctx_parent parent,
      const std::shared_ptr<asio_common_state>& asio_state,
      const string_view addr_str,
      const span_size_t block_size) noexcept
      : base{parent, asio_state, block_size}
      , _addr_str{_fix_addr(addr_str)} {
        conn_state().conn_endpoint = {_addr_str.c_str()};
    }

    auto update() noexcept -> work_done final {
        some_true something_done{};
        if(conn_state().socket.is_open()) [[likely]] {
            something_done(conn_state().start_receive(*this));
            something_done(conn_state().start_send(*this));
        } else if(not _connecting) {
            if(_should_reconnect) {
                _should_reconnect.reset();
                _start_connect();
                something_done();
            }
        }
        something_done(conn_state().update());
        return something_done;
    }

private:
    std::string _addr_str;
    timeout _should_reconnect{
      adjusted_duration(std::chrono::seconds{1}, memory_access_rate::low),
      nothing};
    bool _connecting{false};

    void _start_connect() noexcept {
        _connecting = true;
        this->log_debug("connecting to ${address}")
          .arg("address", "FsPath", this->_addr_str);

        conn_state().socket.async_connect(
          conn_state().conn_endpoint,
          [this](const std::error_code error) mutable {
              if(not error) {
                  this->log_debug("connected on address ${address}")
                    .arg("address", "FsPath", _addr_str);
                  _connecting = false;
              } else {
                  this->log_error("failed to connect: ${error}")
                    .arg("error", error.message());
                  _connecting = false;
              }
          });
    }

    static auto _fix_addr(const string_view addr_str) noexcept -> string_view {
        return addr_str ? addr_str : string_view{"/tmp/eagine-msgbus.socket"};
    }
};
//------------------------------------------------------------------------------
template <>
class asio_acceptor<connection_addr_kind::filepath, connection_protocol::stream>
  : public asio_connection_info<
      acceptor,
      connection_addr_kind::filepath,
      connection_protocol::stream>
  , public main_ctx_object {
public:
    asio_acceptor(
      main_ctx_parent parent,
      std::shared_ptr<asio_common_state> asio_state,
      const string_view addr_str, const span_size_t block_size) noexcept
      : main_ctx_object{"AsioAccptr", parent}
      , _asio_state{_prepare(std::move(asio_state), _fix_addr(addr_str))}
      , _addr_str{to_string(_fix_addr(addr_str))}
      , _acceptor{
          _asio_state->context,
          asio::local::stream_protocol::endpoint(_addr_str.c_str())}
	  , _block_size{block_size} {
    }

    ~asio_acceptor() noexcept override {
        try {
            [[maybe_unused]] const auto unused{std::remove(_addr_str.c_str())};
        } catch(...) {
        }
    }

    asio_acceptor(asio_acceptor&&) = delete;
    asio_acceptor(const asio_acceptor&) = delete;
    auto operator=(asio_acceptor&&) = delete;
    auto operator=(const asio_acceptor&) = delete;

    auto update() noexcept -> work_done final {
        assert(this->_asio_state);
        some_true something_done{};
        if(not _acceptor.is_open()) [[unlikely]] {
            _acceptor = {
              _asio_state->context,
              asio::local::stream_protocol::endpoint(_addr_str.c_str())};
            something_done();
        }
        if(_acceptor.is_open() and not _accepting) {
            _start_accept();
            something_done();
        }
        if(this->_asio_state->context.poll()) {
            something_done();
        } else {
            this->_asio_state->context.reset();
        }
        return something_done;
    }

    auto process_accepted(const accept_handler handler) noexcept
      -> work_done final {
        some_true something_done{};
        for(auto& socket : _accepted) {
            auto conn = std::make_unique<asio_connection<
              connection_addr_kind::filepath,
              connection_protocol::stream>>(
              *this, _asio_state, std::move(socket), _block_size);
            handler(std::move(conn));
            something_done();
        }
        _accepted.clear();
        return something_done;
    }

private:
    const std::shared_ptr<asio_common_state> _asio_state;
    std::string _addr_str;
    asio::local::stream_protocol::acceptor _acceptor;
    const span_size_t _block_size;
    bool _accepting{false};

    std::vector<asio::local::stream_protocol::socket> _accepted;

    void _start_accept() noexcept {
        log_debug("accepting connection on address ${address}")
          .arg("address", "FsPath", _addr_str);

        _accepting = true;
        _acceptor.async_accept([this](
                                 const std::error_code error,
                                 asio::local::stream_protocol::socket socket) {
            if(error) {
                this->_accepting = false;
                this
                  ->log_error(
                    "failed to accept connection on address ${address}: "
                    "${error}")
                  .arg("error", error.message())
                  .arg("address", "FsPath", _addr_str);
            } else {
                this->log_debug("accepted connection on address ${address}")
                  .arg("address", "FsPath", _addr_str);
                this->_accepted.emplace_back(std::move(socket));
            }
            _start_accept();
        });
    }

    static auto _fix_addr(const string_view addr_str) noexcept -> string_view {
        return addr_str ? addr_str : string_view{"/tmp/eagine-msgbus.socket"};
    }

    static auto _prepare(
      std::shared_ptr<asio_common_state> asio_state,
      string_view addr_str) noexcept -> std::shared_ptr<asio_common_state> {
        [[maybe_unused]] const auto unused{std::remove(c_str(addr_str))};
        return asio_state;
    }
};
//------------------------------------------------------------------------------
#endif // ASIO_HAS_LOCAL_SOCKETS
//------------------------------------------------------------------------------
// Factory
//------------------------------------------------------------------------------
template <connection_addr_kind Kind, connection_protocol Proto>
class asio_connection_factory
  : public asio_connection_info<connection_factory, Kind, Proto>
  , public main_ctx_object {
public:
    using connection_factory::make_acceptor;
    using connection_factory::make_connector;

    static constexpr auto default_block_size() noexcept -> span_size_t {
        return _default_block_size(
          connection_addr_kind_tag<Kind>{}, connection_protocol_tag<Proto>{});
    }

    asio_connection_factory(
      main_ctx_parent parent,
      std::shared_ptr<asio_common_state> asio_state,
      const span_size_t block_size) noexcept
      : main_ctx_object{"AsioConnFc", parent}
      , _asio_state{std::move(asio_state)}
      , _block_size{block_size} {}

    asio_connection_factory(
      main_ctx_parent parent,
      const span_size_t block_size) noexcept
      : asio_connection_factory{
          parent,
          std::make_shared<asio_common_state>(),
          block_size} {}

    asio_connection_factory(main_ctx_parent parent) noexcept
      : asio_connection_factory{parent, default_block_size()} {}

    auto make_acceptor(const string_view addr_str) noexcept
      -> std::unique_ptr<acceptor> final {
        return std::make_unique<asio_acceptor<Kind, Proto>>(
          *this, _asio_state, addr_str, _block_size);
    }

    auto make_connector(const string_view addr_str) noexcept
      -> std::unique_ptr<connection> final {
        return std::make_unique<asio_connector<Kind, Proto>>(
          *this, _asio_state, addr_str, _block_size);
    }

private:
    const std::shared_ptr<asio_common_state> _asio_state;

    template <connection_addr_kind K, connection_protocol P>
    static constexpr auto _default_block_size(
      const connection_addr_kind_tag<K>,
      const connection_protocol_tag<P>) noexcept -> span_size_t {
        return 4 * 1024;
    }

    template <connection_addr_kind K>
    static constexpr auto _default_block_size(
      const connection_addr_kind_tag<K>,
      const datagram_protocol_tag) noexcept -> span_size_t {
        return min_connection_data_size;
    }

    const span_size_t _block_size{default_block_size()};
};
//------------------------------------------------------------------------------
auto make_asio_tcp_ipv4_connection_factory(main_ctx_parent parent)
  -> std::unique_ptr<connection_factory> {
    return std::make_unique<asio_connection_factory<
      connection_addr_kind::ipv4,
      connection_protocol::stream>>(parent);
}

auto make_asio_udp_ipv4_connection_factory(main_ctx_parent parent)
  -> std::unique_ptr<connection_factory> {
    return std::make_unique<asio_connection_factory<
      connection_addr_kind::ipv4,
      connection_protocol::datagram>>(parent);
}

auto make_asio_local_stream_connection_factory(
  [[maybe_unused]] main_ctx_parent parent)
  -> std::unique_ptr<connection_factory> {
#if defined(ASIO_HAS_LOCAL_SOCKETS)
    return std::make_unique<asio_connection_factory<
      connection_addr_kind::filepath,
      connection_protocol::stream>>(parent);
#else
    return {};
#endif
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

