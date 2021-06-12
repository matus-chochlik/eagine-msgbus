# Copyright Matus Chochlik.
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
set(HEADERS
    eagine/logging/type/remote_node.hpp
    eagine/message_bus/acceptor.hpp
    eagine/message_bus/actor.hpp
    eagine/message_bus/asio.hpp
    eagine/message_bus/blobs.hpp
    eagine/message_bus/bridge.hpp
    eagine/message_bus/connection.hpp
    eagine/message_bus/connection_kind.hpp
    eagine/message_bus/conn_factory.hpp
    eagine/message_bus/conn_setup.hpp
    eagine/message_bus/context_fwd.hpp
    eagine/message_bus/context.hpp
    eagine/message_bus/direct.hpp
    eagine/message_bus/endpoint.hpp
    eagine/message_bus/future.hpp
    eagine/message_bus/handler_map.hpp
    eagine/message_bus.hpp
    eagine/message_bus/invoker.hpp
    eagine/message_bus/loopback.hpp
    eagine/message_bus/message.hpp
    eagine/message_bus/network.hpp
    eagine/message_bus/node_kind.hpp
    eagine/message_bus/posix_mqueue.hpp
    eagine/message_bus/registry.hpp
    eagine/message_bus/remote_node.hpp
    eagine/message_bus/resources.hpp
    eagine/message_bus/router_address.hpp
    eagine/message_bus/router.hpp
    eagine/message_bus/serialize.hpp
    eagine/message_bus/service/ability.hpp
    eagine/message_bus/service/application_info.hpp
    eagine/message_bus/service/build_info.hpp
    eagine/message_bus/service/common_info.hpp
    eagine/message_bus/service/compiler_info.hpp
    eagine/message_bus/service/discovery.hpp
    eagine/message_bus/service/endpoint_info.hpp
    eagine/message_bus/service/host_info.hpp
    eagine/message_bus/service.hpp
    eagine/message_bus/service_interface.hpp
    eagine/message_bus/service/ping_pong.hpp
    eagine/message_bus/service_requirements.hpp
    eagine/message_bus/service/resource_transfer.hpp
    eagine/message_bus/service/shutdown.hpp
    eagine/message_bus/service/statistics.hpp
    eagine/message_bus/service/sudoku.hpp
    eagine/message_bus/service/system_info.hpp
    eagine/message_bus/service/topology.hpp
    eagine/message_bus/service/tracker.hpp
    eagine/message_bus/signal.hpp
    eagine/message_bus/skeleton.hpp
    eagine/message_bus/subscriber.hpp
    eagine/message_bus/types.hpp
    eagine/message_bus/verification.hpp
)

set(PUB_INLS
)

set(LIB_INLS
    eagine/message_bus/blobs.inl
    eagine/message_bus/bridge.inl
    eagine/message_bus/conn_setup.inl
    eagine/message_bus/context.inl
    eagine/message_bus/endpoint.inl
    eagine/message_bus/message.inl
    eagine/message_bus/registry.inl
    eagine/message_bus/remote_node.inl
    eagine/message_bus/resources.inl
    eagine/message_bus/router.inl
)

