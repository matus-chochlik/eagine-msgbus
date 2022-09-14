/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
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
  , server_response_timeout{c, "resource.consumer.server_response_timeout", std::chrono::seconds{10}}
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
    connect<&resource_data_consumer_node::_handle_ping_response>(
      this, ping_responded);
    connect<&resource_data_consumer_node::_handle_ping_timeout>(
      this, ping_timeouted);
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::update() noexcept -> work_done {
    some_true something_done;

    for(auto& [server_id, info] : _current_servers) {
        if(ping_if(server_id, info.should_check)) {
            something_done();
        }
    }

    for(auto& [resource_id, pres] : _pending_resources) {
        if(std::holds_alternative<_streamed_resource_info>(pres.info)) {
            auto& rinfo = std::get<_streamed_resource_info>(pres.info);
            if(!is_valid_endpoint_id(rinfo.source_server_id)) {
                if(rinfo.should_search) {
                    for(auto& [server_id, sinfo] : _current_servers) {
                        if(!sinfo.not_responding) {
                            search_resource(server_id, pres.locator);
                        }
                    }
                    rinfo.should_search.reset();
                    log_debug("searching resource: ${locator}")
                      .tag("resrceSrch")
                      .arg("streamId", resource_id)
                      .arg("locator", pres.locator.str());
                    something_done();
                }
            }
        } else if(std::holds_alternative<_embedded_resource_info>(pres.info)) {
            auto& rinfo = std::get<_embedded_resource_info>(pres.info);
            auto append = [&, offset{span_size(0)}, this](
                            const memory::const_block data) mutable {
                blob_stream_data_appended(resource_id, offset, view_one(data));
                offset += data.size();
                return true;
            };
            rinfo.resource.fetch(main_context(), {construct_from, append});
        }
    }
    auto pos{_pending_resources.begin()};
    while(pos != _pending_resources.end()) {
        if(std::holds_alternative<_embedded_resource_info>(
             std::get<1>(*pos).info)) {
            pos = _pending_resources.erase(pos);
        } else {
            ++pos;
        }
    }

    something_done(base::update_and_process_all());

    return something_done;
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::_has_pending(
  identifier_t resource_id) const noexcept -> bool {
    return _pending_resources.find(resource_id) != _pending_resources.end();
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::_get_resource_id() noexcept -> identifier_t {
    do {
        ++_res_id_seq;
    } while(!_res_id_seq || _has_pending(_res_id_seq));
    return _res_id_seq;
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::_query_resource(
  identifier_t resource_id,
  url locator,
  std::shared_ptr<blob_io> io,
  const message_priority priority,
  const std::chrono::seconds max_time)
  -> std::pair<identifier_t, resource_data_consumer_node::_resource_info&> {
    auto& pres = _pending_resources[resource_id];
    pres.locator = std::move(locator);
    if(const auto res_id{pres.locator.path_identifier()}) {
        if(const auto res{_embedded_loader.search(res_id.value())}) {
            pres.info = _embedded_resource_info{};
            auto& info = std::get<_embedded_resource_info>(pres.info);
            info.resource = std::move(res);
            return {resource_id, pres};
        }
    }
    pres.info = _embedded_resource_info{};
    auto& info = std::get<_streamed_resource_info>(pres.info);
    info.resource_io = std::move(io);
    info.source_server_id = invalid_endpoint_id();
    info.should_search.reset(_config.resource_search_interval.value(), nothing);
    info.blob_timeout.reset(max_time);
    info.blob_priority = priority;
    return {resource_id, pres};
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::stream_resource(
  url locator,
  const message_priority priority,
  const std::chrono::seconds max_time) -> std::pair<identifier_t, const url&> {
    const auto resource_id{_get_resource_id()};
    auto result{_query_resource(
      resource_id,
      std::move(locator),
      make_blob_stream_io(resource_id, *this, _buffers),
      priority,
      max_time)};
    return {std::get<0>(result), std::get<1>(result).locator};
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::cancel_resource_stream(
  identifier_t resource_id) noexcept -> bool {
    const auto pos{_pending_resources.find(resource_id)};
    if(pos != _pending_resources.end()) {
        // TODO: cancel in base
        _pending_resources.erase(pos);
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
    for(auto& entry : _pending_resources) {
        auto& pres = std::get<1>(entry);
        if(std::holds_alternative<_streamed_resource_info>(pres.info)) {
            auto& info = std::get<_streamed_resource_info>(pres.info);
            if(info.source_server_id == server_id) {
                info.source_server_id = invalid_endpoint_id();
            }
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
    for(auto& entry : _pending_resources) {
        auto& pres = std::get<1>(entry);
        if(pres.locator == locator) {
            if(std::holds_alternative<_streamed_resource_info>(pres.info)) {
                auto& info = std::get<_streamed_resource_info>(pres.info);
                if(!is_valid_endpoint_id(info.source_server_id)) {
                    if(const auto id{query_resource_content(
                         server_id,
                         pres.locator,
                         info.resource_io,
                         info.blob_priority,
                         info.blob_timeout)}) {
                        info.source_server_id = server_id;
                        info.blob_stream_id = extract(id);
                        log_info(
                          "fetching resource ${locator} from server ${id}")
                          .tag("qryResCont")
                          .arg("locator", pres.locator.str())
                          .arg("priority", info.blob_priority)
                          .arg("id", server_id);
                        break;
                    }
                }
            }
        }
    }
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_missing(
  identifier_t server_id,
  const url& locator) noexcept {
    for(auto& entry : _pending_resources) {
        auto& pres = std::get<1>(entry);
        if(pres.locator == locator) {
            if(std::holds_alternative<_streamed_resource_info>(pres.info)) {
                auto& info = std::get<_streamed_resource_info>(pres.info);
                if(info.source_server_id == server_id) {
                    info.source_server_id = invalid_endpoint_id();
                    log_debug("resource ${locator} not found on server ${id}")
                      .tag("resNotFund")
                      .arg("locator", pres.locator.str())
                      .arg("id", server_id);
                }
            }
        }
    }
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_stream_done(
  identifier_t resource_id) noexcept {
    _pending_resources.erase(resource_id);
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_ping_response(
  const identifier_t server_id,
  const message_sequence_t,
  const std::chrono::microseconds age,
  const verification_bits) noexcept {
    const auto pos{_current_servers.find(server_id)};
    if(pos != _current_servers.end()) {
        auto& info = std::get<1>(*pos);
        info.not_responding.reset();
    }
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_ping_timeout(
  const identifier_t server_id,
  const message_sequence_t,
  const std::chrono::microseconds) noexcept {
    _handle_server_lost(server_id);
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
