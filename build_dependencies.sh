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
# Install CMake 3.22 or higher (required by CMakeLists.txt)
CMAKE_VERSION="3.22.6"
CMAKE_DIR="cmake-${CMAKE_VERSION}-linux-x86_64"

# Check if cmake needs upgrade
NEED_CMAKE_INSTALL=false
if ! command -v cmake &> /dev/null; then
    echo "CMake not found, will install ${CMAKE_VERSION}"
    NEED_CMAKE_INSTALL=true
else
    CURRENT_CMAKE_VERSION=$(cmake --version | head -n1 | awk '{print $3}')
    REQUIRED_VERSION="3.16"
    if [ "$(printf '%s\n' "$REQUIRED_VERSION" "$CURRENT_CMAKE_VERSION" | sort -V | head -n1)" != "$REQUIRED_VERSION" ]; then
        echo "Current CMake version $CURRENT_CMAKE_VERSION is less than required $REQUIRED_VERSION"
        NEED_CMAKE_INSTALL=true
    fi
fi

if [ "$NEED_CMAKE_INSTALL" = true ]; then
    echo "Installing CMake ${CMAKE_VERSION}..."
    wget -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${CMAKE_DIR}.tar.gz
    tar -xzf ${CMAKE_DIR}.tar.gz
    cp -rf ${CMAKE_DIR}/bin/* /usr/local/bin/
    cp -rf ${CMAKE_DIR}/share/* /usr/local/share/
    rm -rf ${CMAKE_DIR} ${CMAKE_DIR}.tar.gz
    echo "CMake ${CMAKE_VERSION} installed successfully"
fi

# Ensure /usr/local/bin is in PATH for Coverity environment
export PATH=/usr/local/bin:$PATH

# Verify CMake installation
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

git clone --branch R4.4.1 https://github.com/rdkcentral/Thunder.git

git clone --branch develop https://github.com/rdkcentral/entservices-apis.git

git clone -b develop https://github.com/rdkcentral/eshelpers.git

git clone -b v1.2.0 https://github.com/rdkcentral/ralf-utils.git

git clone -b develop https://github.com/rdkcentral/libPackage.git

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
    -DPERSISTENT_PATH="$GITHUB_WORKSPACE/install/usr/lib" \
    -DPLUGIN_SYSTEMCOMMANDS=ON \
    -DPORT="55555" \
    -DSYSTEM_PATH="$GITHUB_WORKSPACE/install/usr/lib/Thunder/plugins" \
    -DVOLATILE_PATH="$GITHUB_WORKSPACE/install/tmp"

cmake --build build/Thunder --target install

############################
# Build entservices-apis
echo "======================================================================================"
echo "building entservices-apis"

cmake -G Ninja -S entservices-apis -B build/entservices-apis \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr" \
    -DBUILD_ENTSERVICES_COMMON=ON

cmake --build build/entservices-apis --target install

############################
# Build eshelpers
echo "======================================================================================"
echo "building eshelpers"

cmake -G Ninja -S eshelpers -B build/eshelpers \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr"

cmake --build build/eshelpers --target install

############################
# Build ralf-utils (libralf)
echo "======================================================================================"
echo "building ralf-utils"

cmake -G Ninja -S ralf-utils -B build/ralf-utils \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr" \
    -DBUILD_SHARED_LIBS=ON

cmake --build build/ralf-utils --target install

############################
# Build libPackage
echo "======================================================================================"
echo "building libPackage"

cmake -G Ninja -S libPackage -B build/libPackage \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr"

cmake --build build/libPackage --target install

echo "======================================================================================"
echo "All dependencies built successfully"
echo "======================================================================================"
