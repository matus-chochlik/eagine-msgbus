/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#if __has_include(<fcntl.h>) && \
	__has_include(<mqueue.h>) && \
	__has_include(<sys/resource.h>)
#include <cassert>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/resource.h>
#define EAGINE_POSIX 1
#else
#define EAGINE_POSIX 0
#endif

module eagine.msgbus.core;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.serialization;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.main_ctx;
import <cerrno>;
import <cstring>;
import <mutex>;
import <random>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
#if EAGINE_POSIX
/// @brief Class wrapping a POSIX message queue
/// @ingroup msgbus
class posix_mqueue : public main_ctx_object {
public:
    posix_mqueue(main_ctx_parent parent) noexcept
      : main_ctx_object{"PosixMQue", parent} {}

    /// @brief Move constructible.
    posix_mqueue(posix_mqueue&& temp) noexcept
      : main_ctx_object{static_cast<main_ctx_object&&>(temp)} {
        using std::swap;
        swap(_s2cname, temp._s2cname);
        swap(_c2sname, temp._c2sname);
        swap(_ihandle, temp._ihandle);
        swap(_ohandle, temp._ohandle);
    }

    /// @brief Not copy constructible.
    posix_mqueue(const posix_mqueue&) = delete;

    /// @brief Not move assignable.
    auto operator=(posix_mqueue&& temp) = delete;

    /// @brief Not copy assignable.
    auto operator=(const posix_mqueue&) = delete;

    ~posix_mqueue() noexcept {
        try {
            this->close();
        } catch(...) {
        }
    }

    /// @brief Returns the unique name of this queue.
    /// @see set_name
    auto get_name() const noexcept -> string_view {
        return _s2cname.empty()
                 ? string_view{}
                 : string_view{
                     _s2cname.data(), span_size(_s2cname.size() - 1U)};
    }

    /// @brief Sets the unique name of the queue.
    /// @see get_name
    auto set_name(std::string name) noexcept -> auto& {
        _s2cname = std::move(name);
        if(_s2cname.empty()) {
            _s2cname = "/eagine-msgbus";
        } else {
            if(_s2cname.front() != '/') {
                _s2cname.insert(_s2cname.begin(), '/');
            }
        }
        log_info("assigned message queue name ${name}").arg("name", _s2cname);
        _c2sname = _s2cname;
        _s2cname.push_back('s');
        _c2sname.push_back('c');
        return *this;
    }

    static auto name_from(const identifier id) noexcept -> std::string {
        std::string result;
        result.reserve(integer(identifier::max_size() + 1));
        id.name().str(result);
        return result;
    }

    /// @brief Sets the unique name of the queue.
    /// @see get_name
    auto set_name(const identifier id) noexcept -> auto& {
        return set_name(name_from(id));
    }

    /// @brief Constructs the queue and sets the specified name.
    /// @see set_name
    posix_mqueue(main_ctx_parent parent, std::string name) noexcept
      : main_ctx_object{"PosixMQue", parent} {
        set_name(std::move(name));
    }

    auto error_message(const int error_number) const noexcept -> std::string {
        if(error_number) {
            char buf[128] = {};
            [[maybe_unused]] auto unused{
              ::strerror_r(error_number, static_cast<char*>(buf), sizeof(buf))};
            return {static_cast<const char*>(buf)};
        }
        return {};
    }

    /// @brief Returns the error message of the last failed operation.
    /// @see had_error
    auto error_message() const noexcept -> std::string {
        return error_message(_last_errno);
    }

    /// @brief Indicates if there a previous operation finished with an error.
    /// @see error_message
    /// @see needs_retry
    auto had_error() const noexcept -> bool {
        return _last_errno != 0;
    }

    /// @brief Indicates if a previous operation on the queue needs to be retried.
    /// @see had_error
    auto needs_retry() const noexcept -> bool {
        return (_last_errno == EAGAIN) or (_last_errno == ETIMEDOUT);
    }

    /// @brief Indicates if this message queue is open.
    /// @see is_usable
    constexpr auto is_open() const noexcept -> bool {
        return (_ihandle >= 0) and (_ohandle >= 0);
    }

    /// @brief Indicates if this message queue can be used.
    /// @see is_open
    /// @see had_error
    constexpr auto is_usable() const noexcept -> bool {
        return is_open() and not(had_error() and not needs_retry());
    }

    /// @brief Unlinks the OS queue objects.
    /// @see create
    /// @see open
    /// @see close
    auto unlink() noexcept -> auto& {
        if(get_name()) {
            log_debug("unlinking message queue ${name}").arg("name", get_name());

            errno = 0;
            ::mq_unlink(_s2cname.c_str());
            ::mq_unlink(_c2sname.c_str());
            _last_errno = errno;
        }
        return *this;
    }

    /// @brief Creates new OS queue objects.
    /// @see unlink
    /// @see open
    /// @see close
    auto create() noexcept -> auto& {
        log_debug("creating new message queue ${name}").arg("name", get_name());

        struct ::mq_attr attr {};
        zero(as_bytes(cover_one(attr)));
        attr.mq_maxmsg = limit_cast<long>(8);
        attr.mq_msgsize = limit_cast<long>(default_data_size());
        errno = 0;
        // NOLINTNEXTLINE(hicpp-vararg)
        _ihandle = ::mq_open(
          _c2sname.c_str(),
          // NOLINTNEXTLINE(hicpp-signed-bitwise)
          O_RDONLY | O_CREAT | O_EXCL | O_NONBLOCK,
          // NOLINTNEXTLINE(hicpp-signed-bitwise)
          S_IRUSR | S_IWUSR,
          &attr);
        _last_errno = errno;
        if(_last_errno == 0) {
            // NOLINTNEXTLINE(hicpp-vararg)
            _ohandle = ::mq_open(
              _s2cname.c_str(),
              // NOLINTNEXTLINE(hicpp-signed-bitwise)
              O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK,
              // NOLINTNEXTLINE(hicpp-signed-bitwise)
              S_IRUSR | S_IWUSR,
              &attr);
            _last_errno = errno;
        }
        if(_last_errno) {
            log_error("failed to create message queue ${name}")
              .arg("name", get_name())
              .arg("errno", _last_errno)
              .arg("message", error_message(_last_errno));
        }
        return *this;
    }

    /// @brief Opens existing OS queue objects.
    /// @see create
    /// @see unlink
    /// @see close
    auto open() noexcept -> auto& {
        log_debug("opening existing message queue ${name}")
          .arg("name", get_name());

        errno = 0;
        // NOLINTNEXTLINE(hicpp-vararg)
        _ihandle = ::mq_open(
          _s2cname.c_str(),
          // NOLINTNEXTLINE(hicpp-signed-bitwise)
          O_RDONLY | O_NONBLOCK,
          // NOLINTNEXTLINE(hicpp-signed-bitwise)
          S_IRUSR | S_IWUSR,
          nullptr);
        _last_errno = errno;
        if(_last_errno == 0) {
            // NOLINTNEXTLINE(hicpp-vararg)
            _ohandle = ::mq_open(
              _c2sname.c_str(),
              // NOLINTNEXTLINE(hicpp-signed-bitwise)
              O_WRONLY | O_NONBLOCK,
              // NOLINTNEXTLINE(hicpp-signed-bitwise)
              S_IRUSR | S_IWUSR,
              nullptr);
            _last_errno = errno;
        }
        if(_last_errno) {
            log_error("failed to open message queue ${name}")
              .arg("name", get_name())
              .arg("errno", _last_errno)
              .arg("message", error_message(_last_errno));
        }
        return *this;
    }

    /// @brief Closes the OS queue objects.
    /// @see create
    /// @see open
    /// @see unlink
    auto close() noexcept -> posix_mqueue& {
        if(is_open()) {
            log_debug("closing message queue ${name}").arg("name", get_name());

            ::mq_close(_ihandle);
            ::mq_close(_ohandle);
            _ihandle = _invalid_handle();
            _ohandle = _invalid_handle();
            _last_errno = errno;
        }
        return *this;
    }

    constexpr static auto default_data_size() noexcept -> span_size_t {
        return 2 * 1024;
    }

    /// @brief Returns the absolute maximum block size that can be sent in a message.
    /// @see data_size
    auto max_data_size() noexcept -> valid_if_positive<span_size_t> {
        if(is_open()) [[likely]] {
            struct ::mq_attr attr {};
            errno = 0;
            ::mq_getattr(_ohandle, &attr);
            _last_errno = errno;
            return {span_size(attr.mq_msgsize)};
        }
        return {0};
    }

    /// @brief Returns the maximum block size that can be sent in a message.
    auto data_size() noexcept -> span_size_t {
        return extract_or(max_data_size(), default_data_size());
    }

    /// @brief Sends a block of data with the specified priority.
    auto send(const unsigned priority, const span<const char> blk) noexcept
      -> auto& {
        if(is_open()) [[likely]] {
            errno = 0;
            ::mq_send(_ohandle, blk.data(), integer(blk.size()), priority);
            _last_errno = errno;
            if((_last_errno != 0) and (_last_errno != EAGAIN)) [[unlikely]] {
                log_error("failed to send message")
                  .arg("name", get_name())
                  .arg("errno", _last_errno)
                  .arg("data", as_bytes(blk));
            }
        }
        return *this;
    }

    /// @brief Alias for received message handler.
    /// @see receive
    using receive_handler =
      callable_ref<void(unsigned, span<const char>) noexcept>;

    /// @brief Receives messages and calls the specified handler on them.
    auto receive(memory::span<char> blk, const receive_handler handler) noexcept
      -> bool {
        if(is_open()) [[likely]] {
            unsigned priority{0U};
            errno = 0;
            const auto received =
              ::mq_receive(_ihandle, blk.data(), blk.size(), &priority);
            _last_errno = errno;
            if(received > 0) [[likely]] {
                handler(priority, head(blk, received));
                return true;
            } else if(
              (_last_errno != 0) and (_last_errno != EAGAIN) and
              (_last_errno != ETIMEDOUT)) [[unlikely]] {
                log_error("failed to receive message")
                  .arg("name", get_name())
                  .arg("errno", _last_errno);
            }
        }
        return false;
    }

private:
    std::string _s2cname{};
    std::string _c2sname{};

    static constexpr auto _invalid_handle() noexcept -> ::mqd_t {
        return ::mqd_t(-1);
    }

    ::mqd_t _ihandle{_invalid_handle()};
    ::mqd_t _ohandle{_invalid_handle()};
    int _last_errno{0};
};
//------------------------------------------------------------------------------
struct posix_mqueue_shared_state {
    auto make_id() const noexcept {
        return random_identifier();
    }
};
//------------------------------------------------------------------------------
/// @brief Implementation of the connection_info interface for POSIX queue connection.
/// @ingroup msgbus
/// @see connection_info
template <typename Base>
class posix_mqueue_connection_info : public Base {
public:
    using Base::Base;

    auto kind() noexcept -> connection_kind final {
        return connection_kind::local_interprocess;
    }

    auto addr_kind() noexcept -> connection_addr_kind final {
        return connection_addr_kind::filepath;
    }

    auto type_id() noexcept -> identifier final {
        return "PosixMQue";
    }
};
//------------------------------------------------------------------------------
/// @brief Implementation of connection on top of POSIX message queues.
/// @ingroup msgbus
/// @see posix_mqueue
/// @see posix_mqueue_connector
/// @see posix_mqueue_acceptor
class posix_mqueue_connection
  : public posix_mqueue_connection_info<connection>
  , public main_ctx_object {

public:
    /// @brief Alias for received message fetch handler callable.
    using fetch_handler = connection::fetch_handler;

    /// @brief Construction from parent main context object.
    posix_mqueue_connection(
      main_ctx_parent parent,
      std::shared_ptr<posix_mqueue_shared_state> shared_state) noexcept
      : main_ctx_object{"MQueConn", parent}
      , _shared_state{std::move(shared_state)} {
        _buffer.resize(_data_queue.data_size());
    }

    /// @brief Opens the connection.
    auto open(std::string name) noexcept -> bool {
        return not _data_queue.set_name(std::move(name)).open().had_error();
    }

    auto is_usable() noexcept -> bool final {
        return _data_queue.is_usable();
    }

    auto max_data_size() noexcept -> valid_if_positive<span_size_t> final {
        return {_buffer.size()};
    }

    auto update() noexcept -> work_done override {
        const std::unique_lock lock{_mutex};
        some_true something_done{};
        something_done(_receive());
        something_done(_send());
        return something_done;
    }

    auto send(const message_id msg_id, const message_view& message) noexcept
      -> bool final {
        if(is_usable()) [[likely]] {
            block_data_sink sink(cover(_buffer));
            default_serializer_backend backend(sink);
            if(serialize_message(msg_id, message, backend)) [[likely]] {
                const std::unique_lock lock{_mutex};
                _outgoing.push(sink.done());
                return true;
            } else {
                log_error("failed to serialize message");
            }
        }
        return false;
    }

    auto fetch_messages(const fetch_handler handler) noexcept
      -> work_done final {
        const std::unique_lock lock{_mutex};
        return _incoming.fetch_all(handler);
    }

    auto query_statistics(connection_statistics&) noexcept -> bool final {
        return false;
    }

protected:
    auto _checkup(posix_mqueue& connect_queue) noexcept -> work_done {
        some_true something_done{};
        if(not _data_queue.is_usable()) [[unlikely]] {
            if(connect_queue.is_usable()) [[likely]] {
                if(_reconnect_timeout) {
                    _data_queue.close();
                    _data_queue.unlink();
                    assert(_shared_state);

                    log_debug("connecting to ${name}")
                      .arg("name", connect_queue.get_name());

                    if(not _data_queue.set_name(_shared_state->make_id())
                             .create()
                             .had_error()) {

                        _buffer.resize(connect_queue.data_size());

                        block_data_sink sink(cover(_buffer));
                        default_serializer_backend backend(sink);
                        if(serialize_message(
                             msgbus_id{"pmqConnect"},
                             message_view(_data_queue.get_name()),
                             backend)) [[likely]] {
                            connect_queue.send(1, as_chars(sink.done()));
                            _buffer.resize(_data_queue.data_size());
                            something_done();
                        } else {
                            log_error("failed to serialize connection name")
                              .arg("client", _data_queue.get_name())
                              .arg("server", connect_queue.get_name());
                        }
                    } else {
                        log_warning("failed to connect to ${server}")
                          .arg("server", connect_queue.get_name());
                    }
                    _reconnect_timeout.reset();
                }
            }
        }
        return something_done;
    }

    auto _receive() noexcept -> work_done {
        some_true something_done{};
        if(_data_queue.is_usable()) [[likely]] {
            while(_data_queue.receive(
              as_chars(cover(_buffer)),
              posix_mqueue::receive_handler(
                make_callable_ref<&posix_mqueue_connection::_handle_receive>(
                  this)))) {
                something_done();
            }
        }
        return something_done;
    }

    auto _send() noexcept -> bool {
        if(_data_queue.is_usable()) [[likely]] {
            return _outgoing.fetch_all(
              make_callable_ref<&posix_mqueue_connection::_handle_send>(this));
        }
        return false;
    }

protected:
    auto _handle_send(
      const message_timestamp,
      const memory::const_block data) noexcept -> bool {
        return not _data_queue.send(1, as_chars(data)).had_error();
    }

    void _handle_receive(
      const unsigned,
      const memory::span<const char> data) noexcept {
        _incoming.push_if(
          [data](
            message_id& msg_id, message_timestamp&, stored_message& message) {
              block_data_source source(as_bytes(data));
              default_deserializer_backend backend(source);
              return bool(deserialize_message(msg_id, message, backend));
          });
    }

    std::mutex _mutex;
    memory::buffer _buffer;
    message_storage _incoming;
    serialized_message_storage _outgoing;
    posix_mqueue _data_queue{*this};
    timeout _reconnect_timeout{std::chrono::seconds{2}, nothing};
    std::shared_ptr<posix_mqueue_shared_state> _shared_state;
};
//------------------------------------------------------------------------------
/// @brief Implementation of connection on top of POSIX message queues.
/// @ingroup msgbus
/// @see posix_mqueue
/// @see posix_mqueue_acceptor
class posix_mqueue_connector final : public posix_mqueue_connection {
    using base = posix_mqueue_connection;

public:
    /// @brief Alias for received message fetch handler callable.
    using fetch_handler = connection::fetch_handler;

    /// @brief Construction from parent main context object and queue name.
    posix_mqueue_connector(
      main_ctx_parent parent,
      std::string name,
      std::shared_ptr<posix_mqueue_shared_state> shared_state) noexcept
      : base{parent, std::move(shared_state)}
      , _connect_queue{*this, std::move(name)} {}

    /// @brief Construction from parent main context object and queue identifier.
    posix_mqueue_connector(
      main_ctx_parent parent,
      const identifier id,
      std::shared_ptr<posix_mqueue_shared_state> shared_state) noexcept
      : base{parent, std::move(shared_state)}
      , _connect_queue{*this, posix_mqueue::name_from(id)} {}

    posix_mqueue_connector(posix_mqueue_connector&&) = delete;
    posix_mqueue_connector(const posix_mqueue_connector&) = delete;
    auto operator=(posix_mqueue_connector&&) = delete;
    auto operator=(const posix_mqueue_connector&) = delete;

    ~posix_mqueue_connector() noexcept final {
        _data_queue.unlink();
    }

    auto update() noexcept -> work_done final {
        const std::unique_lock lock{_mutex};
        some_true something_done{};
        something_done(_checkup());
        something_done(_receive());
        something_done(_send());
        return something_done;
    }

private:
    auto _checkup() noexcept -> work_done {
        some_true something_done{};
        if(not _connect_queue.is_usable()) [[unlikely]] {
            if(_reconnect_timeout) {
                _connect_queue.close();
                if(not _connect_queue.open().had_error()) {
                    something_done();
                }
                _reconnect_timeout.reset();
            }
        }
        something_done(posix_mqueue_connection::_checkup(_connect_queue));
        return something_done;
    }

    posix_mqueue _connect_queue{*this};
};
//------------------------------------------------------------------------------
/// @brief Implementation of acceptor on top of POSIX message queues.
/// @ingroup msgbus
/// @see posix_mqueue
/// @see posix_mqueue_connector
class posix_mqueue_acceptor final
  : public posix_mqueue_connection_info<acceptor>
  , public main_ctx_object {

public:
    /// @brief Alias for accepted connection handler callable.
    using accept_handler = acceptor::accept_handler;

    /// @brief Construction from parent main context object and queue name.
    posix_mqueue_acceptor(
      main_ctx_parent parent,
      std::string name,
      std::shared_ptr<posix_mqueue_shared_state> shared_state) noexcept
      : main_ctx_object{"MQueConnAc", parent}
      , _accept_queue{*this, std::move(name)}
      , _shared_state{std::move(shared_state)} {
        _buffer.resize(_accept_queue.data_size());
    }

    /// @brief Construction from parent main context object and queue identifier.
    posix_mqueue_acceptor(
      main_ctx_parent parent,
      const identifier id,
      std::shared_ptr<posix_mqueue_shared_state> shared_state) noexcept
      : posix_mqueue_acceptor{
          parent,
          posix_mqueue::name_from(id),
          std::move(shared_state)} {}

    posix_mqueue_acceptor(posix_mqueue_acceptor&&) noexcept = default;
    posix_mqueue_acceptor(const posix_mqueue_acceptor&) = delete;
    auto operator=(posix_mqueue_acceptor&&) = delete;
    auto operator=(const posix_mqueue_acceptor&) = delete;

    ~posix_mqueue_acceptor() noexcept final {
        _accept_queue.unlink();
    }

    auto update() noexcept -> work_done final {
        some_true something_done{};
        something_done(_checkup());
        something_done(_receive());
        return something_done;
    }

    auto process_accepted(const accept_handler handler) noexcept
      -> work_done final {
        auto fetch_handler = [this, &handler](
                               [[maybe_unused]] const message_id msg_id,
                               const message_age,
                               const message_view& message) -> bool {
            assert((msg_id == msgbus_id{"pmqConnect"}));

            log_debug("accepting connection from ${name}")
              .arg("name", message.text_content());

            if(auto conn{std::make_unique<posix_mqueue_connection>(
                 *this, _shared_state)}) {
                if(extract(conn).open(to_string(message.text_content()))) {
                    handler(std::move(conn));
                }
            }
            return true;
        };
        return _requests.fetch_all({construct_from, fetch_handler});
    }

private:
    auto _checkup() noexcept -> work_done {
        some_true something_done{};
        if(not _accept_queue.is_usable()) [[unlikely]] {
            if(_reconnect_timeout) {
                _accept_queue.close();
                _accept_queue.unlink();
                if(not _accept_queue.create().had_error()) {
                    _buffer.resize(_accept_queue.data_size());
                    something_done();
                }
                _reconnect_timeout.reset();
            }
        }
        return something_done;
    }

    auto _receive() noexcept -> work_done {
        some_true something_done{};
        if(_accept_queue.is_usable()) [[likely]] {
            while(_accept_queue.receive(
              as_chars(cover(_buffer)),
              make_callable_ref<&posix_mqueue_acceptor::_handle_receive>(
                this))) {
                something_done();
            }
        }
        return something_done;
    }

    void _handle_receive(
      const unsigned,
      const memory::span<const char> data) noexcept {
        _requests.push_if(
          [data](
            message_id& msg_id, message_timestamp&, stored_message& message) {
              block_data_source source(as_bytes(data));
              default_deserializer_backend backend(source);
              const auto deserialized{
                deserialize_message(msg_id, message, backend)};
              if(is_special_message(msg_id)) [[likely]] {
                  if(msg_id.has_method("pmqConnect")) [[likely]] {
                      return bool(deserialized);
                  }
              }
              return false;
          });
    }

    memory::buffer _buffer;
    message_storage _requests;
    posix_mqueue _accept_queue;
    timeout _reconnect_timeout{std::chrono::seconds{2}, nothing};
    std::shared_ptr<posix_mqueue_shared_state> _shared_state;
};
//------------------------------------------------------------------------------
/// @brief Implementation of connection_factory for POSIX message queue connections.
/// @ingroup msgbus
/// @see posix_mqueue_connector
/// @see posix_mqueue_acceptor
class posix_mqueue_connection_factory
  : public posix_mqueue_connection_info<connection_factory>
  , public main_ctx_object {
public:
    /// @brief Construction from parent main context object.
    posix_mqueue_connection_factory(main_ctx_parent parent) noexcept
      : main_ctx_object{"MQueConnFc", parent} {
        _increase_res_limit();
    }

    using connection_factory::make_acceptor;
    using connection_factory::make_connector;

    /// @brief Makes an connection acceptor listening at queue with the specified name.
    auto make_acceptor(const string_view address) noexcept
      -> std::unique_ptr<acceptor> final {
        return std::make_unique<posix_mqueue_acceptor>(
          *this, to_string(address), _shared_state);
    }

    /// @brief Makes a connector connecting to queue with the specified name.
    auto make_connector(const string_view address) noexcept
      -> std::unique_ptr<connection> final {
        return std::make_unique<posix_mqueue_connector>(
          *this, to_string(address), _shared_state);
    }

private:
    std::shared_ptr<posix_mqueue_shared_state> _shared_state{
      std::make_shared<posix_mqueue_shared_state>()};

    void _increase_res_limit() noexcept {
        struct rlimit rlim {};
        zero(as_bytes(cover_one(rlim)));
        rlim.rlim_cur = RLIM_INFINITY;
        rlim.rlim_max = RLIM_INFINITY;
        errno = 0;
        ::setrlimit(RLIMIT_MSGQUEUE, &rlim);
    }
};
#endif // EAGINE_POSIX
//------------------------------------------------------------------------------
auto make_posix_mqueue_connection_factory(
  [[maybe_unused]] main_ctx_parent parent)
  -> std::unique_ptr<connection_factory> {
#if EAGINE_POSIX
    return std::make_unique<posix_mqueue_connection_factory>(parent);
#else
    return {};
#endif
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

