/// @example eagine/msgbus/005_topology.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import std;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
using topology_printer_base = service_composition<
  require_services<subscriber, network_topology, shutdown_target>>;

class topology_printer
  : public main_ctx_object
  , public topology_printer_base {
    using base = topology_printer_base;

public:
    topology_printer(endpoint& bus)
      : main_ctx_object{"TopoPrint", bus}
      , base{bus} {
        connect<&topology_printer::on_router_appeared>(this, router_appeared);
        connect<&topology_printer::on_bridge_appeared>(this, bridge_appeared);
        connect<&topology_printer::on_endpoint_appeared>(
          this, endpoint_appeared);
        connect<&topology_printer::on_shutdown>(this, shutdown_requested);
    }

    void print_topology() {
        std::cout << "graph EMB {\n";

        std::cout << "	overlap=false\n";
        std::cout << "	splines=true\n";
        std::cout << "	node [style=filled]\n";
        std::cout << "	node [shape=egg;color=\"#B0D0B0\"]\n";
        for(const auto id : _routers) {
            std::cout << "	n" << id << "[label=\"Router-" << id << "\"]\n";
        }
        std::cout << "\n";

        std::cout << "	node [shape=parallelogram;color=\"#80B080\"]\n";
        for(const auto id : _bridges) {
            std::cout << "	n" << id << " [label=\"Bridge-" << id << "\"]\n";
        }
        std::cout << "\n";

        std::cout << "	node [shape=box;color=\"#B0E0B0\"]\n";
        std::cout << "	n" << this->bus_node().get_id()
                  << "[label=\"Self\\nEndpoint-" << this->bus_node().get_id()
                  << "\"]\n";

        for(const auto id : _endpoints) {
            std::cout << "	n" << id << "[label=\"Endpoint-" << id << "\"]\n";
        }
        std::cout << "\n";

        std::cout << "	edge [style=solid,penwidth=2]\n";
        for(auto [l, r] : _connections) {
            std::cout << "	n" << l << " -- n" << r << "\n";
        }

        std::cout << "}\n";
    }

    void on_router_appeared(
      const result_context&,
      const router_topology_info& info) noexcept {
        log_info("found router connection ${router} - ${remote}")
          .arg("remote", info.remote_id)
          .arg("router", info.router_id);

        _routers.emplace(info.router_id);
        _connections.emplace(info.router_id, info.remote_id);
    }

    void on_bridge_appeared(
      const result_context&,
      const bridge_topology_info& info) noexcept {
        if(info.opposite_id) {
            log_info("found bridge connection ${bridge} - ${remote}")
              .arg("remote", info.opposite_id)
              .arg("bridge", info.bridge_id);

            _bridges.emplace(info.opposite_id);
            _connections.emplace(info.bridge_id, info.opposite_id);
        } else {
            _log.info("found bridge ${bridge}").arg("bridge", info.bridge_id);
        }

        _bridges.emplace(info.bridge_id);
    }

    void on_endpoint_appeared(
      const result_context&,
      const endpoint_topology_info& info) noexcept {
        log_info("found endpoint ${endpoint}").arg("endpoint", info.endpoint_id);

        _endpoints.emplace(info.endpoint_id);
    }

    void on_shutdown(
      const result_context&,
      const shutdown_request& req) noexcept {
        _log.info("received ${age} old shutdown request from ${subscrbr}")
          .arg("age", req.age)
          .arg("subscrbr", req.source_id)
          .arg("verified", req.verified);
    }

private:
    std::set<identifier_t> _routers;
    std::set<identifier_t> _bridges;
    std::set<identifier_t> _endpoints;
    std::set<std::pair<identifier_t, identifier_t>> _connections;
    logger _log{};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    const signal_switch interrupted;
    enable_message_bus(ctx);

    msgbus::endpoint bus{"TopologyEx", ctx};
    bus.add_ca_certificate_pem(ca_certificate_pem(ctx));
    bus.add_certificate_pem(msgbus::endpoint_certificate_pem(ctx));

    msgbus::topology_printer topo_prn{bus};
    msgbus::setup_connectors(ctx, topo_prn);

    timeout waited_enough{std::chrono::seconds(30)};
    resetting_timeout resend_query{std::chrono::seconds(5), nothing};

    while(not(interrupted or waited_enough)) {
        if(resend_query) {
            topo_prn.discover_topology();
        }
        topo_prn.update();
        topo_prn.process_all().or_sleep_for(std::chrono::milliseconds(250));
    }

    topo_prn.print_topology();

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}

