# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set (TARGET "sceneset")

add_executable(${TARGET} SceneSet.cpp RalfPackageSupport.cpp main.cpp)

set_target_properties(${TARGET} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# GCC < 9 requires explicit -lstdc++fs when using <filesystem> in C++17 mode.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(${TARGET} PRIVATE stdc++fs)
endif()

find_package(PkgConfig REQUIRED)
find_package(libralf CONFIG REQUIRED)

pkg_check_modules(WPEFRAMEWORKCORE REQUIRED WPEFrameworkCore)
pkg_check_modules(WPEFRAMEWORKPLUGINS REQUIRED WPEFrameworkPlugins)
pkg_check_modules(WPEFRAMEWORKWEBSOCKET REQUIRED WPEFrameworkWebSocket)
pkg_check_modules(WPEFRAMEWORKDEFINITIONS REQUIRED WPEFrameworkDefinitions)


set(COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WPEFRAMEWORKCORE_INCLUDE_DIRS}
    ${WPEFRAMEWORKPLUGINS_INCLUDE_DIRS}
    ${WPEFRAMEWORKWEBSOCKET_INCLUDE_DIRS}
    ${WPEFRAMEWORKDEFINITIONS_INCLUDE_DIRS}
    )

set(COMMON_LIBRARIES
    ${WPEFRAMEWORKCORE_LIBRARIES}
    ${WPEFRAMEWORKPLUGINS_LIBRARIES}
    ${WPEFRAMEWORKWEBSOCKET_LIBRARIES}
    ${WPEFRAMEWORKDEFINITIONS_LIBRARIES}
    )

message(STATUS "Common include directories: ${COMMON_INCLUDE_DIRS}")
message(STATUS "Common libraries: ${COMMON_LIBRARIES}")

if(NOT DEFINED SCENESET_DEFAULT_APPNAME)
    set(SCENESET_DEFAULT_APPNAME "" CACHE STRING "Default application name for SceneSet")
endif()

if(NOT DEFINED FACTORY_APP_PATH)
    set(FACTORY_APP_PATH "" CACHE STRING "Factory apps location")
endif()

if(NOT DEFINED APP_PREINSTALL_DIRECTORY)
    set(APP_PREINSTALL_DIRECTORY "" CACHE STRING "Preinstall apps folder")
endif()

if(FACTORY_APP_PATH AND NOT APP_PREINSTALL_DIRECTORY)
    message(FATAL_ERROR "FACTORY_APP_PATH is set but APP_PREINSTALL_DIRECTORY is not set")
endif()

if(NOT SCENESET_DEFAULT_APPNAME)
    message(WARNING "SCENESET_DEFAULT_APPNAME is not set")
endif()

if(NOT FACTORY_APP_PATH)
    message(WARNING "FACTORY_APP_PATH is not set")
endif()

if(NOT DAC_APP_CERT_PATH)
    message(WARNING "DAC_APP_CERT_PATH is not set, using default value /etc/rdk/certs")
    set(DAC_APP_CERT_PATH "/etc/rdk/certs" CACHE STRING "DAC app certificate path")
endif()

set(DISABLE_REFERENCE_APP_UPDATE OFF CACHE BOOL "Disable reference app update")
if(DISABLE_REFERENCE_APP_UPDATE)
    message(WARNING "Reference app update will be disabled, SceneSet will not update the reference app")
endif()

set(ENABLE_SYSTEM_CONFIG OFF CACHE BOOL "Enable reading system config from /etc/sceneset.conf")
set(ENABLE_CONFIG_OVERRIDE OFF CACHE BOOL "Enable optional /opt/sceneset.conf override on top of /etc/sceneset.conf")
set(RESTART_HOMEAPP_ALWAYS OFF CACHE BOOL "Restart reference app on every TERMINATING to UNLOADED transition")
set(SCENESET_TELEMETRY_METRICS_SUPPORT OFF CACHE BOOL "Enable SceneSet telemetry marker publication via T2")

include(GNUInstallDirs)
target_include_directories(${TARGET} PRIVATE ${COMMON_INCLUDE_DIRS})
target_link_libraries(${TARGET} PRIVATE ${COMMON_LIBRARIES} systemd)

if(TARGET ralf-utils::libralf)
    target_link_libraries(${TARGET} PRIVATE ralf-utils::libralf)
elseif(TARGET libralf)
    target_link_libraries(${TARGET} PRIVATE libralf)
elseif(TARGET ralf-utils::libralf-static)
    message(WARNING "Only static libralf target available. Linking ralf-utils::libralf-static.")
    target_link_libraries(${TARGET} PRIVATE ralf-utils::libralf-static)
elseif(TARGET libralf-static)
    message(WARNING "Only static libralf target available. Linking libralf-static.")
    target_link_libraries(${TARGET} PRIVATE libralf-static)
else()
    message(FATAL_ERROR "libralf package found but no usable CMake target exported (expected ralf-utils::libralf, libralf, ralf-utils::libralf-static, or libralf-static)")
endif()

# Get Git short SHA for logging version.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHORT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# If Git command fails, set a default value
if(NOT GIT_SHORT_SHA)
    set(GIT_SHORT_SHA "unknown")
endif()

target_compile_definitions(${TARGET} PRIVATE
    SCENESET_DEFAULT_APPNAME="${SCENESET_DEFAULT_APPNAME}"
    FACTORY_APP_PATH="${FACTORY_APP_PATH}"
    APP_PREINSTALL_DIRECTORY="${APP_PREINSTALL_DIRECTORY}"
    DAC_APP_CERT_PATH="${DAC_APP_CERT_PATH}"
    ENABLE_SYSTEM_CONFIG=$<BOOL:${ENABLE_SYSTEM_CONFIG}>
    ENABLE_CONFIG_OVERRIDE=$<BOOL:${ENABLE_CONFIG_OVERRIDE}>
    RESTART_HOMEAPP_ALWAYS=$<BOOL:${RESTART_HOMEAPP_ALWAYS}>
    SCENESET_TELEMETRY_METRICS_SUPPORT=$<BOOL:${SCENESET_TELEMETRY_METRICS_SUPPORT}>
    DISABLE_REFERENCE_APP_UPDATE=$<BOOL:${DISABLE_REFERENCE_APP_UPDATE}>
    GIT_SHORT_SHA="${GIT_SHORT_SHA}"
)

INSTALL (TARGETS ${TARGET}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
