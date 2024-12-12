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
set(MESOS_SUB_VERSION 0.5.0)
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

# Configuration.
include(MesosConfigure)


# SUBDIRECTORIES.
#################
add_subdirectory(3rdparty)
add_subdirectory(src)


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

# Set the basename of any packages to 'aventer-mesos-<version>'.
set(CPACK_PACKAGE_FILE_NAME "aventer-mesos-${MESOS_PACKAGE_VERSION}-${MESOS_SUB_VERSION}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})
set(CPACK_PACKAGE_CONTACT "https://github.com/m3scluster/clusterd")
set(CPACK_PACKAGE_MAINTAINER "AVENTER UG")

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

set(CPACK_PACKAGE_VERSION ${PACKAGE_VERSION}-${MESOS_SUB_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${MESOS_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${MESOS_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${MESOS_PATCH_VERSION})
set(CPACK_PACKAGE_VENDOR ${CPACK_PACKAGE_MAINTAINER})

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
