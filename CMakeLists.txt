# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# THE MESOS PROJECT.
####################
if (NOT WIN32)
  cmake_minimum_required(VERSION 3.7.0)
else ()
  cmake_minimum_required(VERSION 3.8.0)
endif ()

project(aventer-mesos)
set(MESOS_MAJOR_VERSION 1)
set(MESOS_MINOR_VERSION 11)
set(MESOS_PATCH_VERSION 0)
set(MESOS_SUB_VERSION 0.6.1)
set(PACKAGE_VERSION
  ${MESOS_MAJOR_VERSION}.${MESOS_MINOR_VERSION}.${MESOS_PATCH_VERSION})

set(MESOS_PACKAGE_VERSION ${PACKAGE_VERSION})
set(MESOS_PACKAGE_SOVERSION 0)


# CMAKE MODULE SETUP.
#####################
# Paths that are searched when `include(...)` is called.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/3rdparty/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/src/cmake)

# Macros.
include(Versions)
include(cotire)

# TESTS.
########
add_custom_target(check COMMAND ctest -V -C $<CONFIG> DEPENDS tests)
add_custom_target(
  tests
  DEPENDS stout-tests libprocess-tests mesos-tests)

if (ENABLE_NEW_CLI)
  add_dependencies(tests cli-tests)
endif ()

# PACKAGING.
############

file(STRINGS "/etc/os-release" OS_RELEASE)
foreach(line ${OS_RELEASE})
  if(line MATCHES "^ID=")
    string(REPLACE "ID=" "" DISTRO ${line})
    string(STRIP ${DISTRO} DISTRO)
  elseif(line MATCHES "^VERSION_ID=")
    string(REPLACE "VERSION_ID=" "" VERSION ${line})
    string(STRIP ${VERSION} VERSION)
    string(REGEX REPLACE "[^a-zA-Z0-9]" "" VERSION ${VERSION})
  endif()
endforeach()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "unknown")
  execute_process(
    COMMAND uname -m
    OUTPUT_VARIABLE ARCH_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(CMAKE_SYSTEM_PROCESSOR ${ARCH_OUTPUT})
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  set(PROCESSOR_ARCH "amd64")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  set(PROCESSOR_ARCH "arm64")
else()
  set(PROCESSOR_ARCH ${CMAKE_SYSTEM_PROCESSOR})
endif()

set(OS_DISTRIBUTION "${DISTRO}${VERSION}")

# Package dependencies
######################
if(OS_DISTRIBUTION STREQUAL "debian11")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libapr1,libcurl4-nss-dev,libsvn-dev,libevent-openssl-2.1-7,libevent-pthreads-2.1-7")
elseif(OS_DISTRIBUTION STREQUAL "debian12")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libapr1,libcurl4-nss-dev,libsvn-dev,libevent-openssl-2.1-7,libevent-pthreads-2.1-7")
elseif(OS_DISTRIBUTION STREQUAL "ubuntu2204")
  set(CMAKE_INSTALL_PREFIX "/usr")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libapr1,libcurl4-nss-dev,libsvn-dev,libevent-openssl-2.1-7,libevent-pthreads-2.1-7")
elseif(OS_DISTRIBUTION STREQUAL "ubuntu2404")
  set(CMAKE_INSTALL_PREFIX "/usr")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libapr1,libsvn-dev,libevent-openssl-2.1-7t64,libevent-pthreads-2.1-7t64")
endif()

set(CPACK_PACKAGE_VERSION ${PACKAGE_VERSION}-${MESOS_SUB_VERSION}.${OS_DISTRIBUTION})
set(CPACK_PACKAGE_VERSION_MAJOR ${MESOS_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${MESOS_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${MESOS_PATCH_VERSION})

set(MESOS_FINAL_PREFIX ${CMAKE_INSTALL_PREFIX})

# Configuration.
include(MesosConfigure)


# SUBDIRECTORIES.
#################
add_subdirectory(3rdparty)
add_subdirectory(src)

message(STATUS "Distribution: ${OS_DISTRIBUTION}")
message(STATUS "Architecture: ${PROCESSOR_ARCH}")
message(STATUS "Package Version: ${CPACK_PACKAGE_VERSION}")
message(STATUS "Dependencies: ${CPACK_DEBIAN_PACKAGE_DEPENDS}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Data Install Prefix: ${DATA_INSTALL_PREFIX}")
message(STATUS "Mesos Final Prefix: ${MESOS_FINAL_PREFIX}")


# Set the basename of any packages to 'aventer-mesos-<version>'.
set(CPACK_PACKAGE_FILE_NAME "aventer-mesos_${MESOS_PACKAGE_VERSION}-${MESOS_SUB_VERSION}.${OS_DISTRIBUTION}_${PROCESSOR_ARCH}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})
set(CPACK_PACKAGE_CONTACT "https://github.com/m3scluster/clusterd")
set(CPACK_PACKAGE_MAINTAINER "AVENTER UG")
set(CPACK_PACKAGE_VENDOR ${CPACK_PACKAGE_MAINTAINER})

# By default, all forms of binary packages are disabled.
option(CPACK_BINARY_BUNDLE       "Enable to build OSX bundles"              OFF)
option(CPACK_BINARY_DEB          "Enable to build Debian packages"          OFF)
option(CPACK_BINARY_DRAGNDROP    "Enable to build OSX Drag'n'Drop packages" OFF)
option(CPACK_BINARY_IFW          "Enable to build IFW packages"             OFF)
option(CPACK_BINARY_NSIS         "Enable to build NSIS packages"            OFF)
option(CPACK_BINARY_OSXX11       "Enable to build OSX X11 packages"         OFF)
option(CPACK_BINARY_PACKAGEMAKER "Enable to build PackageMaker packages"    OFF)
option(CPACK_BINARY_RPM          "Enable to build RPM packages"             OFF)
option(CPACK_BINARY_STGZ         "Enable to build STGZ packages"            OFF)
option(CPACK_BINARY_TBZ2         "Enable to build TBZ2 packages"            OFF)
option(CPACK_BINARY_TGZ          "Enable to build TGZ packages"             OFF)
option(CPACK_BINARY_TXZ          "Enable to build TXZ packages"             OFF)

# Output a gzip'd tarball for the `package_source` target.
set(CPACK_SOURCE_TGZ ON)

# By default, other forms of source packages are disabled.
option(CPACK_SOURCE_TBZ2 "Enable to build TBZ2 source packages" OFF)
option(CPACK_SOURCE_TXZ  "Enable to build TXZ source packages"  OFF)
option(CPACK_SOURCE_TZ   "Enable to build TZ source packages"   OFF)
option(CPACK_SOURCE_ZIP  "Enable to build ZIP source packages"  OFF)

# configure systemd files

set(RUN_PATH /usr)
set(SERVICE_MASTER mesos-master.service)
set(SERVICE_AGENT mesos-agent.service)
set(SERVICE_INIT mesos-init-wrapper)

configure_file(support/packaging/common/${SERVICE_MASTER}.in ${CMAKE_CURRENT_BINARY_DIR}/${SERVICE_MASTER} @ONLY)
configure_file(support/packaging/common/${SERVICE_AGENT}.in ${CMAKE_CURRENT_BINARY_DIR}/${SERVICE_AGENT} @ONLY)
configure_file(support/packaging/common/${SERVICE_INIT}.in ${CMAKE_CURRENT_BINARY_DIR}/${SERVICE_INIT} @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${SERVICE_MASTER} DESTINATION lib/systemd/system)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${SERVICE_AGENT} DESTINATION lib/systemd/system)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${SERVICE_INIT} 
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
              GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE  
  DESTINATION bin
  )

include(CPack)

if (NOT WIN32)
  # These targets are intended to mirror the same targets of the Autotools
  # build. Because these are written as Shell scripts, they cannot be
  # run on Windows.
  add_custom_target(dist
    COMMAND ${CMAKE_SOURCE_DIR}/cmake/dist.sh ${MESOS_PACKAGE_VERSION})

  add_custom_target(distcheck
    COMMAND ${CMAKE_SOURCE_DIR}/cmake/distcheck.sh ${MESOS_PACKAGE_VERSION}
    DEPENDS dist)
endif ()
