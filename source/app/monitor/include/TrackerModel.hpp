///
/// Copyright Matus Chochlik.
//// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
//

#ifndef EAGINE_MSGBUS_MONITOR_TRACKER_MODEL
#define EAGINE_MSGBUS_MONITOR_TRACKER_MODEL

import eagine.core;
import eagine.msgbus;
import std;
#include "HostParameterModel.hpp"
#include "NodeParameterModel.hpp"
#include <QObject>

class MonitorBackend;
//------------------------------------------------------------------------------
class TrackerModel
  : public QObject
  , public eagine::main_ctx_object {
    Q_OBJECT
public:
    TrackerModel(MonitorBackend&);

    void update();

    auto tracker() noexcept -> auto& {
        return _tracker;
    }

    auto tracker() const noexcept -> auto& {
        return _tracker;
    }

    auto hostParameters(eagine::identifier_t hostId) noexcept
      -> std::shared_ptr<HostParameterModel>;
    auto nodeParameters(eagine::identifier_t nodeId) noexcept
      -> std::shared_ptr<NodeParameterModel>;
signals:
    void nodeKindChanged(const eagine::msgbus::remote_node&);
    void nodeRelocated(const eagine::msgbus::remote_node&);
    void nodeInfoChanged(const eagine::msgbus::remote_node&);
    void instanceRelocated(const eagine::msgbus::remote_instance&);
    void instanceInfoChanged(const eagine::msgbus::remote_instance&);
    void hostInfoChanged(const eagine::msgbus::remote_host&);

    void nodeDisappeared(eagine::identifier_t);

private:
    void handleHostChanged(
      eagine::msgbus::remote_host&,
      eagine::msgbus::remote_host_changes) noexcept;

    void handleInstanceChanged(
      eagine::msgbus::remote_instance&,
      eagine::msgbus::remote_instance_changes) noexcept;

    void handleNodeChanged(
      eagine::msgbus::remote_node&,
      eagine::msgbus::remote_node_changes) noexcept;

    void handleRouterDisappeared(
      const eagine::msgbus::router_shutdown&) noexcept;
    void handleBridgeDisappeared(
      const eagine::msgbus::bridge_shutdown&) noexcept;
    void handleEndpointDisappeared(
      const eagine::msgbus::endpoint_shutdown&) noexcept;

    MonitorBackend& _backend;
    eagine::msgbus::endpoint _bus;
    eagine::msgbus::service_composition<
      eagine::msgbus::node_tracker<eagine::msgbus::shutdown_invoker<>>>
      _tracker;

    std::map<eagine::identifier_t, std::weak_ptr<HostParameterModel>>
      _host_parameters;
    std::map<eagine::identifier_t, std::weak_ptr<NodeParameterModel>>
      _node_parameters;
};
//------------------------------------------------------------------------------
#endif
