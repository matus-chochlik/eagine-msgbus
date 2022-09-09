/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module eagine.msgbus.utility;

import eagine.core.types;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.logging;
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
      .arg("age", age)
      .arg("source", source_id)
      .arg("verified", verified);

    _done = true;
}
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
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::update() noexcept -> work_done {
    some_true something_done{base::update()};

    for(auto& [server_id, info] : _current_servers) {
        if(!info.is_alive) {
            if(info.should_check) {
                info.should_check.reset();
                // TODO: ping
            }
        }
    }

    for(auto& [resource_id, info] : _pending_resources) {
        if(!is_valid_endpoint_id(info.source_server_id)) {
            if(info.should_search) {
                info.should_search.reset();
                // TODO: search
            }
        }
    }

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
  std::shared_ptr<blob_io> io) -> identifier_t {
    auto& info = _pending_resources[resource_id];
    info.locator = std::move(locator);
    info.resource_io = std::move(io);
    info.source_server_id = invalid_endpoint_id();
    return resource_id;
}
//------------------------------------------------------------------------------
auto resource_data_consumer_node::stream_resource(url locator) -> identifier_t {
    const auto res_id{_get_resource_id()};
    return _query_resource(
      res_id, std::move(locator), make_blob_stream_io(res_id, *this, _buffers));
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
  identifier_t endpoint_id) noexcept {
    auto& info = _current_servers[endpoint_id];
    info.is_alive.reset();
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_server_lost(
  identifier_t endpoint_id) noexcept {
    for(auto& entry : _pending_resources) {
        auto& info = std::get<1>(entry);
        if(info.source_server_id == endpoint_id) {
            info.source_server_id = invalid_endpoint_id();
        }
    }
    _current_servers.erase(endpoint_id);
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_resource_found(
  identifier_t endpoint_id,
  const url& locator) noexcept {
    for(auto& entry : _pending_resources) {
        auto& info = std::get<1>(entry);
        if(info.locator == locator) {
            if(!is_valid_endpoint_id(info.source_server_id)) {
                info.source_server_id = endpoint_id;
                // TODO: fetch
            }
        }
    }
}
//------------------------------------------------------------------------------
void resource_data_consumer_node::_handle_missing(
  identifier_t endpoint_id,
  const url& locator) noexcept {
    for(auto& entry : _pending_resources) {
        auto& info = std::get<1>(entry);
        if(info.locator == locator) {
            if(info.source_server_id == endpoint_id) {
                info.source_server_id = invalid_endpoint_id();
            }
        }
    }
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
