#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
cd ${GITHUB_WORKSPACE}

##############################
# 1. Install Dependencies and packages

apt update
apt install -y libsqlite3-dev libcurl4-openssl-dev valgrind lcov clang libsystemd-dev \
    libboost-all-dev libwebsocketpp-dev meson libcunit1 libcunit1-dev curl wget \
    protobuf-compiler-grpc libgrpc-dev libgrpc++-dev libunwind-dev libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev libjsoncpp-dev libyaml-cpp-dev ninja-build \
    libarchive-dev libxml2-dev liblz4-dev libssl-dev openssl pkg-config git

pip install jsonref

############################
# Install CMake 3.22 or higher (required by libralf which needs 3.19+)
CMAKE_VERSION="3.22.6"
CMAKE_DIR="cmake-${CMAKE_VERSION}-linux-x86_64"

# Always check and upgrade if needed
CURRENT_CMAKE_VERSION=$(cmake --version 2>/dev/null | head -n1 | awk '{print $3}' || echo "0.0.0")
REQUIRED_VERSION="3.19.0"

echo "Current CMake version: $CURRENT_CMAKE_VERSION"
echo "Required CMake version: $REQUIRED_VERSION"

# Simple version comparison: check if current is less than required
if [ "$(printf '%s\n' "$CURRENT_CMAKE_VERSION" "$REQUIRED_VERSION" | sort -V | head -n1)" != "$REQUIRED_VERSION" ]; then
    echo "CMake upgrade needed. Installing CMake ${CMAKE_VERSION}..."
    wget -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${CMAKE_DIR}.tar.gz
    tar -xzf ${CMAKE_DIR}.tar.gz
    cp -rf ${CMAKE_DIR}/bin/* /usr/local/bin/
    cp -rf ${CMAKE_DIR}/share/* /usr/local/share/
    rm -rf ${CMAKE_DIR} ${CMAKE_DIR}.tar.gz
    echo "CMake ${CMAKE_VERSION} installed successfully"
else
    echo "CMake version is sufficient, no upgrade needed"
fi

# Ensure /usr/local/bin is in PATH
export PATH=/usr/local/bin:$PATH

# Verify CMake installation
echo "Final CMake version:"
cmake --version
echo "CMake path: $(which cmake)"

############################
# Build trower-base64
if [ ! -d "trower-base64" ]; then
    git clone https://github.com/xmidt-org/trower-base64.git
fi
cd trower-base64
meson setup --warnlevel 3 --werror build
ninja -C build
ninja -C build install
cd ..

###########################################
# Clone the required repositories

git clone --branch R4.4.3 https://github.com/rdkcentral/ThunderTools.git

# Using R4.4.3 (same as ThunderTools) to avoid version mismatches
# R4.4.1 has JSONRPC API issues that require patches
git clone --branch R4.4.3 https://github.com/rdkcentral/Thunder.git 2>/dev/null || \
    git clone --branch develop https://github.com/rdkcentral/Thunder.git

# Only clone ralf-utils which is required for libralf (needed by sceneset)
git clone -b v1.2.0 https://github.com/rdkcentral/ralf-utils.git

############################
# Build Thunder-Tools
echo "======================================================================================"
echo "building thunderTools"
cd ThunderTools
if [ -f "$GITHUB_WORKSPACE/Tests/patches/00010-R4.4-Add-support-for-project-dir.patch" ]; then
    patch -p1 < $GITHUB_WORKSPACE/Tests/patches/00010-R4.4-Add-support-for-project-dir.patch
fi
cd -

cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake"

cmake --build build/ThunderTools --target install

############################
# Build Thunder
echo "======================================================================================"
echo "building thunder"

cmake -G Ninja -S Thunder -B build/Thunder \
    -DBINDING="127.0.0.1" \
    -DCMAKE_BUILD_TYPE="Debug" \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DDATA_PATH="$GITHUB_WORKSPACE/install/usr/share/Thunder" \
    -DEXCEPTIONS_ENABLE=ON \
    -DMESSAGING=ON \
    -DPERSISTENT_PATH="$GITHUB_WORKSPACE/install/usr/lib" \
    -DPLUGIN_SYSTEMCOMMANDS=ON \
    -DPORT="55555" \
    -DSYSTEM_PATH="$GITHUB_WORKSPACE/install/usr/lib/Thunder/plugins" \
    -DVOLATILE_PATH="$GITHUB_WORKSPACE/install/tmp"

cmake --build build/Thunder --target install

############################
# Build ralf-utils (libralf)
echo "======================================================================================"
echo "building ralf-utils (libralf)"

cmake -G Ninja -S ralf-utils -B build/ralf-utils \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr" \
    -DBUILD_SHARED_LIBS=ON

cmake --build build/ralf-utils --target install

echo "======================================================================================"
echo "All dependencies built successfully"
echo "======================================================================================"
