/// @example eagine/msgbus/014_tracker.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
#if EAGINE_MSGBUS_MODULE
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import <thread>;
#else
#include <eagine/identifier_ctr.hpp>
#include <eagine/logging/type/remote_node.hpp>
#include <eagine/main_ctx.hpp>
#include <eagine/main_fwd.hpp>
#include <eagine/math/functions.hpp>
#include <eagine/msgbus/conn_setup.hpp>
#include <eagine/msgbus/router_address.hpp>
#include <eagine/msgbus/service.hpp>
#include <eagine/msgbus/service/shutdown.hpp>
#include <eagine/msgbus/service/tracker.hpp>
#include <eagine/msgbus/service_requirements.hpp>
#include <eagine/timeout.hpp>
#endif

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
using tracker_base = service_composition<
  require_services<subscriber, node_tracker, shutdown_invoker>>;

class tracker_example
  : public main_ctx_object
  , public tracker_base {
    using base = tracker_base;

public:
    tracker_example(endpoint& bus)
      : main_ctx_object{"TrkrExampl", bus}
      , base{bus} {
        object_description("Node tracker", "Node tracker example");
        connect<&tracker_example::on_node_change>(this, this->node_changed);
    }

    void on_node_change(
      remote_node& node,
      const remote_node_changes changes) noexcept {
        log_info("node change ${nodeId}")
          .arg("changes", changes)
          .arg("nodeId", extract(node.id()));
    }

    auto is_done() const noexcept -> bool {
        return true;
    }

    auto update() -> work_done {
        some_true something_done{base::update()};

        if(_checkup_needed) {
            this->for_each_node([&](auto, auto& node) {
                this->log_info("node ${nodeId} status")
                  .arg("nodeId", node)
                  .arg("host", node.host());
            });
        }

        return something_done;
    }

    void shutdown() {
        this->for_each_node(
          [&](auto node_id, auto&) { this->shutdown_one(node_id); });
        base::update();
    }

private:
    resetting_timeout _checkup_needed{std::chrono::seconds(5)};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    ctx.preinitialize();

    msgbus::router_address address{ctx};
    msgbus::connection_setup conn_setup(ctx);

    msgbus::endpoint bus{"TrckrEndpt", ctx};

    msgbus::tracker_example the_tracker{bus};
    conn_setup.setup_connectors(the_tracker, address);

    timeout keep_going{std::chrono::minutes(5)};

    while(!keep_going) {
        the_tracker.process_all();
        if(!the_tracker.update()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    the_tracker.shutdown();

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "PingExe";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}
