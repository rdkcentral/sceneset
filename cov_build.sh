#!/bin/bash
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
set -x
set -e
##############################
export PATH=/usr/local/bin:$PATH
cmake --version

GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
cd ${GITHUB_WORKSPACE}

############################
# Build sceneset
echo "======================================================================================"
echo "building sceneset"

PREFIX_PATH="${CMAKE_PREFIX_PATH:+${CMAKE_PREFIX_PATH};}${GITHUB_WORKSPACE}/install/usr;/usr"

cmake -G Ninja -S "$GITHUB_WORKSPACE" -B build/sceneset \
-DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
-DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
-DCMAKE_PREFIX_PATH="${PREFIX_PATH}" \
-DSCENESET_DEFAULT_APPNAME="com.comcast.application.firebolt" \
-DFACTORY_APP_PATH="/opt/factory/apps" \
-DAPP_PREINSTALL_DIRECTORY="/opt/apps/preinstall" \
-DDAC_APP_CERT_PATH="/etc/rdk/certs" \
-DDISABLE_REFERENCE_APP_UPDATE=OFF \
-DENABLE_SYSTEM_CONFIG=OFF \
-DENABLE_CONFIG_OVERRIDE=OFF \
-DRESTART_HOMEAPP_ALWAYS=OFF \
-DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_CXX_FLAGS="-fvisibility=default -DEXCEPTIONS_ENABLE=ON -Wall -Werror -Wno-error=deprecated-declarations"

cmake --build build/sceneset --target all

echo "======================================================================================"
echo "sceneset build complete"
echo "======================================================================================"

exit 0
