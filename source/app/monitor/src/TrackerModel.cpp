///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///
#include <QDebug>

import eagine.core;
import eagine.msgbus;
#include "MonitorBackend.hpp"
#include "TrackerModel.hpp"
//------------------------------------------------------------------------------
TrackerModel::TrackerModel(MonitorBackend& backend)
  : QObject{nullptr}
  , eagine::main_ctx_object{"Tracker", backend}
  , _backend{backend}
  , _bus{"TrckrEndpt", *this}
  , _tracker{_bus} {
    eagine::msgbus::setup_connectors(main_context(), _tracker);

    eagine::msgbus::connect<&TrackerModel::handleHostChanged>(
      this, _tracker.host_changed);
    eagine::msgbus::connect<&TrackerModel::handleInstanceChanged>(
      this, _tracker.instance_changed);
    eagine::msgbus::connect<&TrackerModel::handleNodeChanged>(
      this, _tracker.node_changed);

    eagine::msgbus::connect<&TrackerModel::handleNodeDisappeared>(
      this, _tracker.router_disappeared);
    eagine::msgbus::connect<&TrackerModel::handleNodeDisappeared>(
      this, _tracker.bridge_disappeared);
    eagine::msgbus::connect<&TrackerModel::handleNodeDisappeared>(
      this, _tracker.endpoint_disappeared);
}
//------------------------------------------------------------------------------
auto TrackerModel::hostParameters(eagine::identifier_t hostId) noexcept
  -> std::shared_ptr<HostParameterModel> {
    auto pos = _host_parameters.find(hostId);
    std::shared_ptr<HostParameterModel> result;
    if(pos == _host_parameters.end()) {
        pos = _host_parameters.emplace(hostId, result).first;
    }
    result = pos->second.lock();
    if(!result) {
        result = std::make_shared<HostParameterModel>();
        pos->second = result;
    }
    return result;
}
//------------------------------------------------------------------------------
auto TrackerModel::nodeParameters(eagine::identifier_t nodeId) noexcept
  -> std::shared_ptr<NodeParameterModel> {
    auto pos = _node_parameters.find(nodeId);
    std::shared_ptr<NodeParameterModel> result;
    if(pos == _node_parameters.end()) {
        pos = _node_parameters.emplace(nodeId, result).first;
    }
    result = pos->second.lock();
    if(!result) {
        result = std::make_shared<NodeParameterModel>();
        pos->second = result;
    }
    return result;
}
//------------------------------------------------------------------------------
void TrackerModel::handleHostChanged(
  eagine::msgbus::remote_host& host,
  eagine::msgbus::remote_host_changes changes) noexcept {
    using eagine::msgbus::remote_host_change;

    if(changes) {
        emit hostInfoChanged(host);
    }

    if(changes.has(remote_host_change::sensor_values)) {
        using eagine::extract;
        if(auto id{host.id()}) {
            const auto pos = _host_parameters.find(extract(id));
            if(pos != _host_parameters.end()) {
                if(auto model{pos->second.lock()}) {
                    if(auto value{host.short_average_load()}) {
                        extract(model)._short_average_load_history =
                          extract(value);
                    }
                    if(auto value{host.long_average_load()}) {
                        extract(model)._long_average_load_history =
                          extract(value);
                    }
                }
            }
        }
    }
}
//------------------------------------------------------------------------------
void TrackerModel::handleInstanceChanged(
  eagine::msgbus::remote_instance& instance,
  eagine::msgbus::remote_instance_changes changes) noexcept {
    using eagine::msgbus::remote_instance_change;

    if(changes.has(remote_instance_change::host_id)) {
        emit instanceRelocated(instance);
    }

    if(
      changes.has(remote_instance_change::statistics) ||
      changes.has(remote_instance_change::started_responding) ||
      changes.has(remote_instance_change::stopped_responding) ||
      changes.has(remote_instance_change::application_info) ||
      changes.has(remote_instance_change::build_info)) {
        emit instanceInfoChanged(instance);
    }
}
//------------------------------------------------------------------------------
void TrackerModel::handleNodeChanged(
  eagine::msgbus::remote_node& node,
  eagine::msgbus::remote_node_changes changes) noexcept {
    using eagine::msgbus::remote_node_change;

    if(changes.has(remote_node_change::response_rate)) {
        using eagine::extract;
        if(auto id{node.id()}) {
            const auto pos = _node_parameters.find(extract(id));
            if(pos != _node_parameters.end()) {
                if(auto model{pos->second.lock()}) {
                    if(auto value{node.ping_success_rate()}) {
                        extract(model)._ping_success_rate_history =
                          extract(value);
                    }
                }
            }
        }
    }

    if(changes.has(remote_node_change::kind)) {
        emit nodeKindChanged(node);
    }

    if(
      changes.has(remote_node_change::host_id) ||
      changes.has(remote_node_change::instance_id)) {
        emit nodeRelocated(node);
    }

    if(
      changes.has(remote_node_change::endpoint_info) ||
      changes.has(remote_node_change::response_rate) ||
      changes.has(remote_node_change::started_responding) ||
      changes.has(remote_node_change::stopped_responding)) {
        emit nodeInfoChanged(node);
    }

    if(changes.has(remote_node_change::methods_added)) {
    }

    if(changes.has(remote_node_change::methods_removed)) {
    }

    if(changes.has(remote_node_change::connection_info)) {
    }
}
//------------------------------------------------------------------------------
void TrackerModel::handleNodeDisappeared(eagine::identifier_t nodeId) noexcept {
    emit nodeDisappeared(nodeId);
}
//------------------------------------------------------------------------------
void TrackerModel::update() {
    _tracker.process_all();
    _tracker.update();
}
//------------------------------------------------------------------------------
