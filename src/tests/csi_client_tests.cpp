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

#include <functional>
#include <string>

#include <process/gtest.hpp>

#include <stout/nothing.hpp>

#include <stout/tests/utils.hpp>

#include "csi/v0_client.hpp"
#include "csi/v1_client.hpp"

#include "tests/mock_csi_plugin.hpp"

using std::string;

using process::Future;

using process::grpc::StatusError;

using process::grpc::client::Connection;
using process::grpc::client::Runtime;

using testing::TestParamInfo;
using testing::Values;
using testing::WithParamInterface;

namespace mesos {
namespace internal {
namespace tests {

struct RPCParam
{
  struct Printer
  {
    string operator()(const TestParamInfo<RPCParam>& info) const
    {
      return info.param.name;
    }
  };

  template <typename Client, typename Request, typename Response>
  static RPCParam create(
      Future<Try<Response, StatusError>> (Client::*rpc)(Request))
  {
    return RPCParam{
      Request::descriptor()->name(),
      [rpc](const Connection& connection, const Runtime& runtime) {
        return (Client(connection, runtime).*rpc)(Request())
          .then([] { return Nothing(); });
      }};
  }

  const string name;
  const std::function<Future<Nothing>(const Connection&, const Runtime&)> call;
};


class CSIClientTest
  : public TemporaryDirectoryTest,
    public WithParamInterface<RPCParam>
{
protected:
  void SetUp() override
  {
    TemporaryDirectoryTest::SetUp();

    Try<Connection> _connection = plugin.startup();
    ASSERT_SOME(_connection);

    connection = _connection.get();
  }

  void TearDown() override
  {
    runtime.terminate();
    AWAIT_ASSERT_READY(runtime.wait());

    ASSERT_SOME(plugin.shutdown());

    TemporaryDirectoryTest::TearDown();
  }

  MockCSIPlugin plugin;
  Option<Connection> connection;
  Runtime runtime;
};


INSTANTIATE_TEST_CASE_P(
    V0,
    CSIClientTest,
    Values(
        RPCParam::create(&csi::v0::Client::getPluginInfo),
        RPCParam::create(&csi::v0::Client::getPluginCapabilities),
        RPCParam::create(&csi::v0::Client::probe),
        RPCParam::create(&csi::v0::Client::createVolume),
        RPCParam::create(&csi::v0::Client::deleteVolume),
        RPCParam::create(&csi::v0::Client::controllerPublishVolume),
        RPCParam::create(&csi::v0::Client::controllerUnpublishVolume),
        RPCParam::create(&csi::v0::Client::validateVolumeCapabilities),
        RPCParam::create(&csi::v0::Client::listVolumes),
        RPCParam::create(&csi::v0::Client::getCapacity),
        RPCParam::create(&csi::v0::Client::controllerGetCapabilities),
        RPCParam::create(&csi::v0::Client::nodeStageVolume),
        RPCParam::create(&csi::v0::Client::nodeUnstageVolume),
        RPCParam::create(&csi::v0::Client::nodePublishVolume),
        RPCParam::create(&csi::v0::Client::nodeUnpublishVolume),
        RPCParam::create(&csi::v0::Client::nodeGetId),
        RPCParam::create(&csi::v0::Client::nodeGetCapabilities)),
    RPCParam::Printer());


INSTANTIATE_TEST_CASE_P(
    V1,
    CSIClientTest,
    Values(
        RPCParam::create(&csi::v1::Client::getPluginInfo),
        RPCParam::create(&csi::v1::Client::getPluginCapabilities),
        RPCParam::create(&csi::v1::Client::probe),
        RPCParam::create(&csi::v1::Client::createVolume),
        RPCParam::create(&csi::v1::Client::deleteVolume),
        RPCParam::create(&csi::v1::Client::controllerPublishVolume),
        RPCParam::create(&csi::v1::Client::controllerUnpublishVolume),
        RPCParam::create(&csi::v1::Client::validateVolumeCapabilities),
        RPCParam::create(&csi::v1::Client::listVolumes),
        RPCParam::create(&csi::v1::Client::getCapacity),
        RPCParam::create(&csi::v1::Client::controllerGetCapabilities),
        RPCParam::create(&csi::v1::Client::controllerListVolumeHealth),
        RPCParam::create(&csi::v1::Client::controllerGetVolumeHealth),
        RPCParam::create(&csi::v1::Client::createSnapshot),
        RPCParam::create(&csi::v1::Client::deleteSnapshot),
        RPCParam::create(&csi::v1::Client::listSnapshots),
        RPCParam::create(&csi::v1::Client::getSnapshot),
        RPCParam::create(&csi::v1::Client::controllerExpandVolume),
        RPCParam::create(&csi::v1::Client::controllerGetVolume),
        RPCParam::create(&csi::v1::Client::controllerModifyVolume),
        RPCParam::create(&csi::v1::Client::groupControllerGetCapabilities),
        RPCParam::create(&csi::v1::Client::createVolumeGroupSnapshot),
        RPCParam::create(&csi::v1::Client::deleteVolumeGroupSnapshot),
        RPCParam::create(&csi::v1::Client::getVolumeGroupSnapshot),
        RPCParam::create(&csi::v1::Client::nodeStageVolume),
        RPCParam::create(&csi::v1::Client::nodeUnstageVolume),
        RPCParam::create(&csi::v1::Client::nodePublishVolume),
        RPCParam::create(&csi::v1::Client::nodeUnpublishVolume),
        RPCParam::create(&csi::v1::Client::nodeGetVolumeStats),
        RPCParam::create(&csi::v1::Client::nodeGetVolumeHealth),
        RPCParam::create(&csi::v1::Client::nodeGetStorageHealth),
        RPCParam::create(&csi::v1::Client::nodeExpandVolume),
        RPCParam::create(&csi::v1::Client::nodeGetCapabilities),
        RPCParam::create(&csi::v1::Client::nodeGetInfo)),
    RPCParam::Printer());


// This test verifies that the all methods of CSI clients work.
TEST_P(CSIClientTest, Call)
{
  AWAIT_EXPECT_READY(GetParam().call(connection.get(), runtime));
}


TEST_F(CSIClientTest, MetadataStreams)
{
  csi::v1::Client client(connection.get(), runtime);

  csi::v1::GetMetadataAllocatedRequest allocatedRequest;
  allocatedRequest.set_snapshot_id("snapshot");
  allocatedRequest.set_starting_offset(0);
  Future<csi::v1::StreamingRPCResult<csi::v1::GetMetadataAllocatedResponse>>
    allocated = client.getMetadataAllocated(allocatedRequest);
  AWAIT_ASSERT_READY(allocated);
  ASSERT_SOME(allocated.get());
  EXPECT_EQ(2u, allocated->get().size());
  EXPECT_EQ(1, allocated->get()[0].volume_capacity_bytes());
  EXPECT_EQ(2, allocated->get()[1].volume_capacity_bytes());

  csi::v1::GetMetadataDeltaRequest deltaRequest;
  deltaRequest.set_base_snapshot_id("base");
  deltaRequest.set_target_snapshot_id("target");
  deltaRequest.set_starting_offset(0);
  Future<csi::v1::StreamingRPCResult<csi::v1::GetMetadataDeltaResponse>>
    delta = client.getMetadataDelta(deltaRequest);
  AWAIT_ASSERT_READY(delta);
  ASSERT_SOME(delta.get());
  EXPECT_EQ(2u, delta->get().size());
  EXPECT_EQ(1, delta->get()[0].volume_capacity_bytes());
  EXPECT_EQ(2, delta->get()[1].volume_capacity_bytes());

  csi::v1::GetMetadataAllocatedRequest emptyAllocatedRequest;
  emptyAllocatedRequest.set_snapshot_id("empty");
  emptyAllocatedRequest.set_starting_offset(0);
  Future<csi::v1::StreamingRPCResult<csi::v1::GetMetadataAllocatedResponse>>
    emptyAllocated = client.getMetadataAllocated(emptyAllocatedRequest);
  AWAIT_ASSERT_READY(emptyAllocated);
  ASSERT_SOME(emptyAllocated.get());
  EXPECT_TRUE(emptyAllocated->get().empty());

  csi::v1::GetMetadataDeltaRequest emptyDeltaRequest;
  emptyDeltaRequest.set_base_snapshot_id("empty");
  emptyDeltaRequest.set_target_snapshot_id("target");
  emptyDeltaRequest.set_starting_offset(0);
  Future<csi::v1::StreamingRPCResult<csi::v1::GetMetadataDeltaResponse>>
    emptyDelta = client.getMetadataDelta(emptyDeltaRequest);
  AWAIT_ASSERT_READY(emptyDelta);
  ASSERT_SOME(emptyDelta.get());
  EXPECT_TRUE(emptyDelta->get().empty());

  csi::v1::GetMetadataAllocatedRequest errorRequest;
  errorRequest.set_starting_offset(0);
  errorRequest.set_snapshot_id("error");
  Future<csi::v1::StreamingRPCResult<csi::v1::GetMetadataAllocatedResponse>>
    error = client.getMetadataAllocated(errorRequest);
  AWAIT_ASSERT_READY(error);
  EXPECT_TRUE(error->isError());
  EXPECT_NE(string::npos, error->error().message.find("synthetic stream failure"));

  csi::v1::GetMetadataDeltaRequest deltaErrorRequest;
  deltaErrorRequest.set_target_snapshot_id("target");
  deltaErrorRequest.set_starting_offset(0);
  deltaErrorRequest.set_base_snapshot_id("error");
  Future<csi::v1::StreamingRPCResult<csi::v1::GetMetadataDeltaResponse>>
    deltaError = client.getMetadataDelta(deltaErrorRequest);
  AWAIT_ASSERT_READY(deltaError);
  EXPECT_TRUE(deltaError->isError());
  EXPECT_NE(
      string::npos,
      deltaError->error().message.find("synthetic delta stream failure"));

  csi::v1::GetMetadataAllocatedRequest partialErrorRequest;
  partialErrorRequest.set_snapshot_id("partial-error");
  partialErrorRequest.set_starting_offset(0);
  Future<csi::v1::StreamingRPCResult<csi::v1::GetMetadataAllocatedResponse>>
    partialError = client.getMetadataAllocated(partialErrorRequest);
  AWAIT_ASSERT_READY(partialError);
  EXPECT_TRUE(partialError->isError());
  EXPECT_NE(
      string::npos, partialError->error().message.find(
          "synthetic allocated receive failure"));

  csi::v1::GetMetadataDeltaRequest deltaPartialErrorRequest;
  deltaPartialErrorRequest.set_base_snapshot_id("partial-error");
  deltaPartialErrorRequest.set_target_snapshot_id("target");
  deltaPartialErrorRequest.set_starting_offset(0);
  Future<csi::v1::StreamingRPCResult<csi::v1::GetMetadataDeltaResponse>>
    deltaPartialError = client.getMetadataDelta(deltaPartialErrorRequest);
  AWAIT_ASSERT_READY(deltaPartialError);
  EXPECT_TRUE(deltaPartialError->isError());
  EXPECT_NE(
      string::npos, deltaPartialError->error().message.find(
          "synthetic delta receive failure"));
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
