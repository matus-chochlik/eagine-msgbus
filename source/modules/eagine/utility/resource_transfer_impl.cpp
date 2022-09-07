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
import eagine.core.runtime;
import eagine.core.logging;
import eagine.msgbus.core;
import eagine.msgbus.services;
import <chrono>;
import <filesystem>;
import <string>;

namespace eagine::msgbus {
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
} // namespace eagine::msgbus
