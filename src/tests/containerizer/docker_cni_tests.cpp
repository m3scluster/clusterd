// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <stout/gtest.hpp>
#include <stout/os.hpp>
#include <stout/path.hpp>

#include "docker/cni.hpp"
#include "docker/docker.hpp"

using mesos::internal::docker::cni::getNetworkConfigFile;
using mesos::internal::docker::cni::getNetworkConfigJSON;
using mesos::internal::docker::cni::parseNetworkInfo;

using std::string;

using ::Docker;

namespace mesos {
namespace internal {
namespace tests {

class DockerCniTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Try<string> directory = os::mkdtemp();
    ASSERT_SOME(directory) << "Unable to create test directory";
    root = directory.get();
  }

  void TearDown() override
  {
    os::rm(root);
  }

  string root;
};

TEST_F(DockerCniTest, FindsConfigInSecondSearchDirectoryWithoutMutatingPath)
{
  const string first = path::join(root, "first");
  const string second = path::join(root, "second");
  ASSERT_SOME(os::mkdir(first));
  ASSERT_SOME(os::mkdir(second));

  const string config = path::join(second, "network.conf");
  ASSERT_SOME(os::write(
      config,
      "{\"name\":\"synthetic\",\"type\":\"synthetic-plugin\"}"));

  const string searchPath = first + ":" + second;
  ASSERT_SOME_EQ(config, getNetworkConfigFile("synthetic", searchPath));
  ASSERT_EQ(first + ":" + second, searchPath);
}

TEST_F(DockerCniTest, RejectsMalformedConfig)
{
  const string config = path::join(root, "malformed.conf");
  ASSERT_SOME(os::write(config, "{not-json"));

  EXPECT_ERROR(getNetworkConfigJSON(config));
  EXPECT_ERROR(getNetworkConfigFile("synthetic", root));
}

TEST_F(DockerCniTest, ParsesIPv4AndStripsPrefix)
{
  Try<NetworkInfo> result = parseNetworkInfo(
      "{\"ip4\":{\"ip\":\"192.0.2.10/24\"}}");
  ASSERT_SOME(result);
  ASSERT_EQ(1, result->ip_addresses_size());
  EXPECT_EQ("192.0.2.10", result->ip_addresses(0).ip_address());
}

TEST_F(DockerCniTest, ParsesIPv6AndSetsProtocol)
{
  ASSERT_SOME(parseNetworkInfo(
      "{\"ip6\":{\"ip\":\"2001:db8::10/64\"}}"));

  NetworkInfo info = parseNetworkInfo(
      "{\"ip6\":{\"ip\":\"2001:db8::10/64\"}}").get();
  ASSERT_EQ(1, info.ip_addresses_size());
  EXPECT_EQ("2001:db8::10/64", info.ip_addresses(0).ip_address());
  EXPECT_EQ(NetworkInfo::IPv6, info.ip_addresses(0).protocol());
}

TEST_F(DockerCniTest, ParsesIPsArrayIPv4)
{
  Try<NetworkInfo> result = parseNetworkInfo(
      "{\"ips\":[{\"address\":\"192.0.2.10/24\"}]}");
  ASSERT_SOME(result);
  ASSERT_EQ(1, result->ip_addresses_size());
  EXPECT_EQ("192.0.2.10", result->ip_addresses(0).ip_address());
}

TEST_F(DockerCniTest, ParsesIPsArrayIPv6)
{
  Try<NetworkInfo> result = parseNetworkInfo(
      "{\"ips\":[{\"address\":\"2001:db8::10/64\"}]}");
  ASSERT_SOME(result);
  ASSERT_EQ(1, result->ip_addresses_size());
  EXPECT_EQ("2001:db8::10", result->ip_addresses(0).ip_address());
  EXPECT_EQ(NetworkInfo::IPv6, result->ip_addresses(0).protocol());
}

TEST_F(DockerCniTest, RejectsMissingResultAddress)
{
  EXPECT_ERROR(parseNetworkInfo("{}"));
  EXPECT_ERROR(parseNetworkInfo("{\"ip4\":{}}"));
  EXPECT_ERROR(parseNetworkInfo("{\"ip6\":{\"ip\":\"\"}}"));
}

TEST(DockerCniContractTest, RejectsMultipleDockerUserNetworks)
{
  ContainerInfo container;
  container.set_type(ContainerInfo::DOCKER);
  container.mutable_docker()->set_image("synthetic-image");
  container.mutable_docker()->set_network(ContainerInfo::DockerInfo::USER);
  container.add_network_infos()->set_name("network-a");
  container.add_network_infos()->set_name("network-b");

  CommandInfo command;
  command.set_value("true");

  EXPECT_ERROR(Docker::RunOptions::create(
      container,
      command,
      "synthetic-container",
      "/tmp",
      "/tmp",
      Resources::parse("cpus:0.1;mem:32").get()));
}

} // namespace tests
} // namespace internal
} // namespace mesos
