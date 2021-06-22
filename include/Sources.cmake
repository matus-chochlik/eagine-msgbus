# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
set(HEADERS
    eagine/logging/type/remote_node.hpp
    eagine/message_bus.hpp
    eagine/msgbus/acceptor.hpp
    eagine/msgbus/actor.hpp
    eagine/msgbus/asio.hpp
    eagine/msgbus/blobs.hpp
    eagine/msgbus/bridge.hpp
    eagine/msgbus/connection.hpp
    eagine/msgbus/connection_kind.hpp
    eagine/msgbus/conn_factory.hpp
    eagine/msgbus/conn_setup.hpp
    eagine/msgbus/context_fwd.hpp
    eagine/msgbus/context.hpp
    eagine/msgbus/direct.hpp
    eagine/msgbus/endpoint.hpp
    eagine/msgbus/future.hpp
    eagine/msgbus/handler_map.hpp
    eagine/msgbus/invoker.hpp
    eagine/msgbus/loopback.hpp
    eagine/msgbus/message.hpp
    eagine/msgbus/network.hpp
    eagine/msgbus/node_kind.hpp
    eagine/msgbus/posix_mqueue.hpp
    eagine/msgbus/registry.hpp
    eagine/msgbus/remote_node.hpp
    eagine/msgbus/resources.hpp
    eagine/msgbus/router_address.hpp
    eagine/msgbus/router.hpp
    eagine/msgbus/serialize.hpp
    eagine/msgbus/service/ability.hpp
    eagine/msgbus/service/application_info.hpp
    eagine/msgbus/service/build_info.hpp
    eagine/msgbus/service/common_info.hpp
    eagine/msgbus/service/compiler_info.hpp
    eagine/msgbus/service/discovery.hpp
    eagine/msgbus/service/endpoint_info.hpp
    eagine/msgbus/service/host_info.hpp
    eagine/msgbus/service.hpp
    eagine/msgbus/service_interface.hpp
    eagine/msgbus/service/ping_pong.hpp
    eagine/msgbus/service_requirements.hpp
    eagine/msgbus/service/resource_transfer.hpp
    eagine/msgbus/service/shutdown.hpp
    eagine/msgbus/service/statistics.hpp
    eagine/msgbus/service/sudoku.hpp
    eagine/msgbus/service/system_info.hpp
    eagine/msgbus/service/topology.hpp
    eagine/msgbus/service/tracker.hpp
    eagine/msgbus/signal.hpp
    eagine/msgbus/skeleton.hpp
    eagine/msgbus/subscriber.hpp
    eagine/msgbus/types.hpp
    eagine/msgbus/verification.hpp
)

set(PUB_INLS
)

set(LIB_INLS
    eagine/msgbus/blobs.inl
    eagine/msgbus/bridge.inl
    eagine/msgbus/conn_setup.inl
    eagine/msgbus/context.inl
    eagine/msgbus/endpoint.inl
    eagine/msgbus/message.inl
    eagine/msgbus/registry.inl
    eagine/msgbus/remote_node.inl
    eagine/msgbus/resources.inl
    eagine/msgbus/router.inl
)

