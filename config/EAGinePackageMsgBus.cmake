#  Copyright Matus Chochlik.
#  Distributed under the Boost Software License, Version 1.0.
#  See accompanying file LICENSE_1_0.txt or copy at
#  https://www.boost.org/LICENSE_1_0.txt
#
# Package specific options
#  Debian
#   Dependencies
set(CXX_RUNTIME_PKGS "libc6,libc++1-17")
set(CPACK_DEBIAN_MSGBUS-APPS_PACKAGE_DEPENDS "${CXX_RUNTIME_PKGS},libsystemd0,zlib1g,libssl3,eagine-core-tools,eagine-core-user")
set(CPACK_DEBIAN_MSGBUS-EXAMPLES_PACKAGE_DEPENDS "${CXX_RUNTIME_PKGS},libsystemd0,zlib1g,libssl3,eagine-core-tools,eagine-core-user")
set(CPACK_DEBIAN_MSGBUS-DEV_PACKAGE_DEPENDS "eagine-core-dev (>= @EAGINE_VERSION@),eagine-sslplus-dev (>= @EAGINE_VERSION@)")
#   Descriptions
set(CPACK_DEBIAN_MSGBUS-APPS_DESCRIPTION "Collection of EAGine msgbus applications.")
set(CPACK_DEBIAN_MSGBUS-EXAMPLES_DESCRIPTION "EAGine MsgBus examples.")
set(CPACK_DEBIAN_MSGBUS-DEV_DESCRIPTION "C++ implementation of an inter-process network message bus.")

