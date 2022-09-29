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

import eagine.core.types;
import eagine.core.memory;
import eagine.core.valid_if;
import eagine.core.reflection;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.logging;
import eagine.core.main_ctx;
import eagine.core.resource;
import eagine.msgbus.core;
import eagine.msgbus.services;
import <chrono>;
import <map>;
import <string>;
import <filesystem>;

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

    if(const auto fs_root_path{main_context().config().get<std::string>(
         "msgbus.resource_server.root_path")}) {
        set_file_root(extract(fs_root_path));
    }
}
//------------------------------------------------------------------------------
void resource_data_server_node::_handle_shutdown(
  const std::chrono::milliseconds age,
  const identifier_t source_id,
  const verification_bits verified) noexcept {
    log_info("received shutdown request from ${source}")
      .tag("shutdwnReq")
      .arg("age", age)
      .arg("source", source_id)
      .arg("verified", verified);

    _done = true;
}
//------------------------------------------------------------------------------
// resource_data_consumer_node_config
//------------------------------------------------------------------------------
resource_data_consumer_node_config::resource_data_consumer_node_config(
  application_config& c)
  : server_check_interval{c, "resource.consumer.server_check_interval", std::chrono::seconds{3}}
  , server_response_timeout{c, "resource.consumer.server_response_timeout", std::chrono::seconds{15}}
  , resource_search_interval{c, "resource.consumer.search_interval", std::chrono::seconds{3}}
  , resource_stream_timeout{c, "resource.consumer.stream_timeout", std::chrono::seconds{3600}}
  , _dummy{0} {}
//------------------------------------------------------------------------------
// resource_data_consumer_node
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
auto resource_data_consumer_node::update() noexcept -> work_done {
    some_true something_done;

    for(auto& [server_id, sinfo] : _current_servers) {
        if(ping_if(server_id, sinfo.should_check)) {
            something_done();
        }
    }

    for(auto& [request_id, info] : _streamed_resources) {
        if(!is_valid_endpoint_id(info.source_server_id)) {
            if(info.should_search) {
                for(auto& [server_id, sinfo] : _current_servers) {
                    if(!sinfo.not_responding) {
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
    for(auto& [request_id, info] : _embedded_resources) {
        blob_info binfo{};
        binfo.source_id = this->bus_node().get_id();
        binfo.target_id = binfo.source_id;

        auto append = [&, offset{span_size(0)}, this](
                        const memory::const_block data) mutable {
            blob_stream_data_appended(
              request_id, offset, view_one(data), binfo);
            offset += data.size();
            return true;
        };
        info.resource.fetch(main_context(), {construct_from, append});
        something_done();
    }
    _embedded_resources.clear();

    something_done(base::update_and_process_all());

    return something_done;
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::has_pending_resource(
  identifier_t request_id) const noexcept -> bool {
    return (_streamed_resources.find(request_id) !=
            _streamed_resources.end()) ||
           (_embedded_resources.find(request_id) != _embedded_resources.end());
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::has_pending_resources() const noexcept
  -> bool {
    return !_streamed_resources.empty() || !_embedded_resources.empty();
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::get_request_id() noexcept -> identifier_t {
    do {
        ++_res_id_seq;
    } while((_res_id_seq == 0) || has_pending_resource(_res_id_seq));
    return _res_id_seq;
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::_query_resource(
  identifier_t request_id,
  url locator,
  std::shared_ptr<target_blob_io> io,
  const message_priority priority,
  const std::chrono::seconds max_time) -> std::pair<identifier_t, const url&> {
    assert(request_id != 0);

    if(const auto res_id{locator.path_identifier()}) {
        if(const auto res{_embedded_loader.search(res_id)}) {
            auto& info = _embedded_resources[request_id];
            info.locator = std::move(locator);
            info.resource = std::move(res);
            log_info("fetching embedded resource ${locator}")
              .tag("embResCont")
              .arg("locator", info.locator.str());
            return {request_id, info.locator};
        }
    }
    auto& info = _streamed_resources[request_id];
    info.locator = std::move(locator);
    info.resource_io = std::move(io);
    info.source_server_id = invalid_endpoint_id();
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
      max_time);
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
      max_time);
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::cancel_resource_stream(
  identifier_t request_id) noexcept -> bool {
    const auto spos{_streamed_resources.find(request_id)};
    if(spos != _streamed_resources.end()) {
        // TODO: cancel in base
        _streamed_resources.erase(spos);
        return true;
    }

    const auto epos{_embedded_resources.find(request_id)};
    if(epos != _embedded_resources.end()) {
        _embedded_resources.erase(epos);
        return true;
    }

    return false;
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_server_appeared(
  identifier_t server_id) noexcept {
    auto& info = _current_servers[server_id];
    info.should_check.reset(_config.server_check_interval.value(), nothing);
    info.not_responding.reset(_config.server_response_timeout.value());
    log_info("resource server ${id} appeared")
      .tag("resSrvAppr")
      .arg("id", server_id);
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_server_lost(
  identifier_t server_id) noexcept {
    for(auto& entry : _streamed_resources) {
        auto& info = std::get<1>(entry);
        if(info.source_server_id == server_id) {
            info.source_server_id = invalid_endpoint_id();
        }
    }
    _current_servers.erase(server_id);
    log_info("resource server ${id} lost")
      .tag("resSrvLost")
      .arg("id", server_id);
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_resource_found(
  identifier_t server_id,
  const url& locator) noexcept {
    for(auto& entry : _streamed_resources) {
        auto& info = std::get<1>(entry);
        if(info.locator == locator) {
            if(!is_valid_endpoint_id(info.source_server_id)) {
                if(const auto id{query_resource_content(
                     server_id,
                     info.locator,
                     info.resource_io,
                     info.blob_priority,
                     info.blob_timeout)}) {
                    info.source_server_id = server_id;
                    info.blob_stream_id = extract(id);
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
  identifier_t server_id,
  const url& locator) noexcept {
    for(auto& entry : _streamed_resources) {
        auto& info = std::get<1>(entry);
        if(info.locator == locator) {
            if(info.source_server_id == server_id) {
                info.source_server_id = invalid_endpoint_id();
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
    _streamed_resources.erase(request_id);
    log_info("resource request id ${reqId} done")
      .tag("streamDone")
      .arg("reqId", request_id)
      .arg("remaining", _streamed_resources.size());
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_stream_data(
  identifier_t,
  const span_size_t,
  const memory::span<const memory::const_block>,
  const blob_info& binfo) noexcept {
    if(const auto spos{_current_servers.find(binfo.source_id)};
       spos != _current_servers.end()) {
        auto& sinfo = std::get<1>(*spos);
        sinfo.not_responding.reset();
    }
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_ping_response(
  const identifier_t server_id,
  const message_sequence_t,
  const std::chrono::microseconds age,
  const verification_bits) noexcept {
    const auto pos{_current_servers.find(server_id)};
    if(pos != _current_servers.end()) {
        log_debug("resource server ${id} responded to ping")
          .arg("id", server_id)
          .arg("age", age);
        auto& info = std::get<1>(*pos);
        info.not_responding.reset();
    }
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_ping_timeout(
  const identifier_t server_id,
  const message_sequence_t,
  const std::chrono::microseconds age) noexcept {
    const auto pos{_current_servers.find(server_id)};
    if(pos != _current_servers.end()) {
        auto& info = std::get<1>(*pos);
        if(info.not_responding) {
            log_info("ping to resource server ${id} timeouted")
              .arg("id", server_id)
              .arg("age", age);
            _handle_server_lost(server_id);
        }
    }
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
