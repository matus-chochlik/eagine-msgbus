/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

module eagine.msgbus.utility;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.valid_if;
import eagine.core.identifier;
import eagine.core.reflection;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.logging;
import eagine.core.main_ctx;
import eagine.core.resource;
import eagine.msgbus.core;
import eagine.msgbus.services;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// resource_data_server_node
//------------------------------------------------------------------------------
void resource_data_server_node::_init() {
    connect<&resource_data_server_node::_handle_shutdown>(
      this, shutdown_requested);
    auto& info = provided_endpoint_info();
    info.display_name = "resource server node";
    info.description = "message bus resource server";

    main_context()
      .config()
      .get<std::string>("msgbus.resource_server.root_path")
      .and_then(
        [this](const auto& fs_root_path) { set_file_root(fs_root_path); });
}
//------------------------------------------------------------------------------
void resource_data_server_node::_handle_shutdown(
  const result_context&,
  const shutdown_request& req) noexcept {
    log_info("received shutdown request from ${source}")
      .tag("shutdwnReq")
      .arg("age", req.age)
      .arg("source", req.source_id)
      .arg("verified", req.verified);

    _done = true;
}
//------------------------------------------------------------------------------
auto resource_data_server_node::update_message_age()
  -> resource_data_server_node& {
    average_message_age(bus_node().flow_average_message_age());
    return *this;
}
//------------------------------------------------------------------------------
// resource_data_consumer_node_config
//------------------------------------------------------------------------------
resource_data_consumer_node_config::resource_data_consumer_node_config(
  application_config& c)
  : server_check_interval{c, "resource.consumer.server_check_interval", std::chrono::seconds{3}}
  , server_response_timeout{c, "resource.consumer.server_response_timeout", std::chrono::seconds{60}}
  , resource_search_interval{c, "resource.consumer.search_interval", std::chrono::seconds{3}}
  , resource_stream_timeout{c, "resource.consumer.stream_timeout", std::chrono::seconds{3600}}
  , _dummy{0} {}
//------------------------------------------------------------------------------
// resource_data_consumer_node
//------------------------------------------------------------------------------
resource_data_consumer_node::_embedded_resource_info::_embedded_resource_info(
  resource_data_consumer_node& parent,
  identifier_t request_id,
  url locator,
  const embedded_resource& resource)
  : _parent{parent}
  , _request_id{request_id}
  , _locator{std::move(locator)}
  , _unpacker{resource.make_unpacker(
      _parent._buffers,
      make_callable_ref<&_embedded_resource_info::_unpack_data>(this))} {
    _binfo.source_id = _parent.bus_node().get_id();
    _binfo.target_id = _binfo.source_id;
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::_embedded_resource_info::_unpack_data(
  memory::const_block data) noexcept -> bool {
    if(_is_all_in_one) {
        if(not _unpacker.is_working() and _unpacker.has_succeeded()) {
            _parent.blob_stream_data_appended(
              {.request_id = _request_id,
               .offset = _unpack_offset,
               .data = view_one(data),
               .info = _binfo});
        } else {
            auto chunk = _parent.buffers().get(data.size());
            memory::copy_into(data, chunk);
            _chunks.emplace_back(std::move(chunk));
        }
    } else {
        _parent.blob_stream_data_appended(
          {.request_id = _request_id,
           .offset = _unpack_offset,
           .data = view_one(data),
           .info = _binfo});
        _unpack_offset += data.size();
    }
    return true;
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::_embedded_resource_info::unpack_next() noexcept
  -> bool {
    if(not _unpacker.next().is_working()) {
        if(_unpacker.has_succeeded()) {
            if(_chunks.size() == 1U) {
                const auto data{view(_chunks.back())};
                _parent.blob_stream_data_appended(
                  {.request_id = _request_id,
                   .offset = 0,
                   .data = view_one(data),
                   .info = _binfo});
                _parent.buffers().eat(std::move(_chunks.back()));
                _chunks.clear();
            } else if(not _chunks.empty()) {
                std::vector<memory::const_block> data;
                data.reserve(_chunks.size());
                for(const auto& chunk : _chunks) {
                    data.emplace_back(view(chunk));
                }
                _parent.blob_stream_data_appended(
                  {.request_id = _request_id,
                   .offset = 0,
                   .data = view(data),
                   .info = _binfo});
                for(auto& chunk : _chunks) {
                    _parent.buffers().eat(std::move(chunk));
                }
                _chunks.clear();
            }
            _parent.blob_stream_finished(_request_id);
        } else {
            _parent.blob_stream_cancelled(_request_id);
        }
        return false;
    }
    return true;
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_init() {
    connect<&resource_data_consumer_node::_handle_server_appeared>(
      this, resource_server_appeared);
    connect<&resource_data_consumer_node::_handle_server_lost>(
      this, resource_server_lost);
    connect<&resource_data_consumer_node::_handle_resource_found>(
      this, server_has_resource);
    connect<&resource_data_consumer_node::_handle_missing>(
      this, server_has_not_resource);
    connect<&resource_data_consumer_node::_handle_stream_done>(
      this, blob_stream_finished);
    connect<&resource_data_consumer_node::_handle_stream_done>(
      this, blob_stream_cancelled);
    connect<&resource_data_consumer_node::_handle_stream_data>(
      this, blob_stream_data_appended);
    connect<&resource_data_consumer_node::_handle_ping_response>(
      this, ping_responded);
    connect<&resource_data_consumer_node::_handle_ping_timeout>(
      this, ping_timeouted);
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::embedded_resource_locator(
  const string_view scheme,
  identifier res_id) noexcept -> url {
    std::string url_str;
    url_str.reserve(std_size(scheme.size() + 4 + 10));
    append_to(scheme, url_str);
    append_to(string_view{":///"}, url_str);
    append_to(res_id.name().view(), url_str);
    return url{std::move(url_str)};
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::update_and_process_all() noexcept
  -> work_done {
    some_true something_done;

    for(auto& [server_id, sinfo] : _current_servers) {
        if(ping_if(server_id, sinfo.should_check)) {
            something_done();
        }
    }

    for(auto& [request_id, info] : _streamed_resources) {
        if(not is_valid_endpoint_id(info.source_server_id)) {
            if(info.should_search) {
                for(auto& [server_id, sinfo] : _current_servers) {
                    if(not sinfo.not_responding) {
                        search_resource(server_id, info.locator);
                    }
                }
                info.should_search.reset();
                log_debug("searching resource: ${locator}")
                  .tag("resrceSrch")
                  .arg("streamId", request_id)
                  .arg("locator", info.locator.str());
                something_done();
            }
        }
    }

    if(not _embedded_resources.empty()) {
        assert(_embedded_resources.front());
        if(not _embedded_resources.front()->unpack_next()) {
            _embedded_resources.erase(_embedded_resources.begin());
        }

        something_done();
    }

    something_done(base::update_and_process_all());

    return something_done;
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::has_pending_resource(
  identifier_t request_id) const noexcept -> bool {
    return _streamed_resources.contains(request_id) or
           (std::find_if(
              _embedded_resources.begin(),
              _embedded_resources.end(),
              _embedded_resource_info::request_id_equal(request_id)) !=
            _embedded_resources.end());
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::has_pending_resources() const noexcept
  -> bool {
    return not _streamed_resources.empty() or not _embedded_resources.empty();
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::get_request_id() noexcept -> identifier_t {
    do {
        ++_res_id_seq;
    } while((_res_id_seq == 0) or has_pending_resource(_res_id_seq));
    return _res_id_seq;
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::_query_resource(
  identifier_t request_id,
  url locator,
  shared_holder<target_blob_io> io,
  const message_priority priority,
  const std::chrono::seconds max_time,
  const bool all_in_one) -> std::pair<identifier_t, const url&> {
    assert(request_id != 0);

    if(const auto resource_id{locator.path_identifier()}) {
        if(const auto res{_embedded_loader.search(resource_id)}) {
            _embedded_resources.emplace_back();
            auto& info = *_embedded_resources.back().emplace(
              *this, request_id, std::move(locator), res);

            info._is_all_in_one = all_in_one;

            log_info("fetching embedded resource ${locator}")
              .tag("embResCont")
              .arg("locator", info._locator.str());
            return {info._request_id, info._locator};
        }
    }
    auto& info = _streamed_resources[request_id];
    info.locator = std::move(locator);
    info.resource_io = std::move(io);
    info.source_server_id = {};
    info.should_search.reset(_config.resource_search_interval.value(), nothing);
    info.blob_timeout.reset(max_time);
    info.blob_priority = priority;
    return {request_id, info.locator};
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::stream_resource(
  url locator,
  const message_priority priority,
  const std::chrono::seconds max_time) -> std::pair<identifier_t, const url&> {
    const auto request_id{get_request_id()};
    return _query_resource(
      request_id,
      std::move(locator),
      make_target_blob_stream_io(request_id, *this, _buffers),
      priority,
      max_time,
      false);
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::fetch_resource_chunks(
  url locator,
  const span_size_t chunk_size,
  const message_priority priority,
  const std::chrono::seconds max_time) -> std::pair<identifier_t, const url&> {
    const auto request_id{get_request_id()};
    return _query_resource(
      request_id,
      std::move(locator),
      make_target_blob_chunk_io(request_id, chunk_size, *this, _buffers),
      priority,
      max_time,
      true);
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::cancel_resource_stream(
  identifier_t request_id) noexcept -> bool {
    if(const auto found{find(_streamed_resources, request_id)}) {
        const auto locator{found->locator.release_string()};
        _streamed_resources.erase(found.position());
        log_info("resource request id ${reqId} (${locator}) canceled")
          .tag("streamDone")
          .arg("reqId", request_id)
          .arg("locator", locator)
          .arg("remaining", _streamed_resources.size());
        return true;
    }

    return std::erase_if(
             _embedded_resources,
             _embedded_resource_info::request_id_equal(request_id)) > 0;
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_server_appeared(
  endpoint_id_t server_id) noexcept {
    auto& info = _current_servers[server_id];
    info.should_check.reset(_config.server_check_interval.value(), nothing);
    info.not_responding.reset(_config.server_response_timeout.value());
    log_info("resource server ${id} appeared")
      .tag("resSrvAppr")
      .arg("id", server_id);
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_server_lost(
  endpoint_id_t server_id) noexcept {
    for(auto& entry : _streamed_resources) {
        auto& info = std::get<1>(entry);
        if(info.source_server_id == server_id) {
            info.source_server_id = {};
        }
    }
    _current_servers.erase(server_id);
    log_info("resource server ${id} lost")
      .tag("resSrvLost")
      .arg("id", server_id);
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_resource_found(
  endpoint_id_t server_id,
  const url& locator) noexcept {
    for(auto& entry : _streamed_resources) {
        auto& info = std::get<1>(entry);
        if(info.locator == locator) {
            if(not is_valid_endpoint_id(info.source_server_id)) {
                if(const auto id{query_resource_content(
                     server_id,
                     info.locator,
                     info.resource_io,
                     info.blob_priority,
                     info.blob_timeout)}) {
                    info.source_server_id = server_id;
                    info.blob_stream_id = *id;
                    log_info("fetching resource ${locator} from server ${id}")
                      .tag("qryResCont")
                      .arg("locator", info.locator.str())
                      .arg("priority", info.blob_priority)
                      .arg("id", server_id);
                    break;
                }
            }
        }
    }
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_missing(
  endpoint_id_t server_id,
  const url& locator) noexcept {
    for(auto& entry : _streamed_resources) {
        auto& info = std::get<1>(entry);
        if(info.locator == locator) {
            if(info.source_server_id == server_id) {
                info.source_server_id = {};
                log_debug("resource ${locator} not found on server ${id}")
                  .tag("resNotFund")
                  .arg("locator", info.locator.str())
                  .arg("id", server_id);
            }
        }
    }
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_stream_done(
  identifier_t request_id) noexcept {
    if(const auto found{find(_streamed_resources, request_id)}) {
        const auto locator{found->locator.release_string()};
        _streamed_resources.erase(found.position());
        log_info("resource request id ${reqId} (${locator}) done")
          .tag("streamDone")
          .arg("reqId", request_id)
          .arg("locator", locator)
          .arg("remaining", _streamed_resources.size());
    }
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_stream_data(
  const blob_stream_chunk& chunk) noexcept {
    find(_current_servers, chunk.info.source_id).and_then([](auto& sinfo) {
        sinfo.not_responding.reset();
    });
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_ping_response(
  const result_context&,
  const ping_response& pong) noexcept {
    find(_current_servers, pong.pingable_id).and_then([&, this](auto& info) {
        log_debug("resource server ${id} responded to ping")
          .arg("id", pong.pingable_id)
          .arg("age", pong.age);
        info.not_responding.reset();
    });
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_ping_timeout(
  const ping_timeout& fail) noexcept {
    find(_current_servers, fail.pingable_id).and_then([&, this](auto& info) {
        if(info.not_responding) {
            log_info("ping to resource server ${id} timeouted")
              .arg("id", fail.pingable_id)
              .arg("age", fail.age);
            _handle_server_lost(fail.pingable_id);
        }
    });
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
