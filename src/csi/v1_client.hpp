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

#ifndef __CSI_V1_CLIENT_HPP__
#define __CSI_V1_CLIENT_HPP__

#include <vector>

#include <mesos/csi/v1.hpp>

#include <process/future.hpp>
#include <process/grpc.hpp>

#include <stout/try.hpp>

namespace mesos {
namespace csi {
namespace v1 {

template <typename Response>
using RPCResult = Try<Response, process::grpc::StatusError>;

template <typename Response>
using StreamingRPCResult =
  Try<std::vector<Response>, process::grpc::StatusError>;


class Client
{
public:
  Client(const process::grpc::client::Connection& _connection,
         const process::grpc::client::Runtime& _runtime)
    : connection(_connection), runtime(_runtime) {}

  process::Future<RPCResult<GetPluginInfoResponse>>
  getPluginInfo(GetPluginInfoRequest request);

  process::Future<RPCResult<GetPluginCapabilitiesResponse>>
  getPluginCapabilities(GetPluginCapabilitiesRequest request);

  process::Future<RPCResult<ProbeResponse>> probe(ProbeRequest request);

  process::Future<RPCResult<CreateVolumeResponse>>
  createVolume(CreateVolumeRequest request);

  process::Future<RPCResult<DeleteVolumeResponse>>
  deleteVolume(DeleteVolumeRequest request);

  process::Future<RPCResult<ControllerPublishVolumeResponse>>
  controllerPublishVolume(ControllerPublishVolumeRequest request);

  process::Future<RPCResult<ControllerUnpublishVolumeResponse>>
  controllerUnpublishVolume(ControllerUnpublishVolumeRequest request);

  process::Future<RPCResult<ValidateVolumeCapabilitiesResponse>>
  validateVolumeCapabilities(ValidateVolumeCapabilitiesRequest request);

  process::Future<RPCResult<ListVolumesResponse>>
  listVolumes(ListVolumesRequest request);

  process::Future<RPCResult<GetCapacityResponse>>
  getCapacity(GetCapacityRequest request);

  process::Future<RPCResult<ControllerGetCapabilitiesResponse>>
  controllerGetCapabilities(ControllerGetCapabilitiesRequest request);

  process::Future<RPCResult<ControllerListVolumeHealthResponse>>
  controllerListVolumeHealth(ControllerListVolumeHealthRequest request);

  process::Future<RPCResult<ControllerGetVolumeHealthResponse>>
  controllerGetVolumeHealth(ControllerGetVolumeHealthRequest request);

  process::Future<RPCResult<CreateSnapshotResponse>>
  createSnapshot(CreateSnapshotRequest request);

  process::Future<RPCResult<DeleteSnapshotResponse>>
  deleteSnapshot(DeleteSnapshotRequest request);

  process::Future<RPCResult<ListSnapshotsResponse>>
  listSnapshots(ListSnapshotsRequest request);

  process::Future<RPCResult<GetSnapshotResponse>>
  getSnapshot(GetSnapshotRequest request);

  process::Future<StreamingRPCResult<GetMetadataAllocatedResponse>>
  getMetadataAllocated(GetMetadataAllocatedRequest request);

  process::Future<StreamingRPCResult<GetMetadataDeltaResponse>>
  getMetadataDelta(GetMetadataDeltaRequest request);

  process::Future<RPCResult<ControllerExpandVolumeResponse>>
  controllerExpandVolume(ControllerExpandVolumeRequest request);

  process::Future<RPCResult<ControllerGetVolumeResponse>>
  controllerGetVolume(ControllerGetVolumeRequest request);

  process::Future<RPCResult<ControllerModifyVolumeResponse>>
  controllerModifyVolume(ControllerModifyVolumeRequest request);

  process::Future<RPCResult<GroupControllerGetCapabilitiesResponse>>
  groupControllerGetCapabilities(GroupControllerGetCapabilitiesRequest request);

  process::Future<RPCResult<CreateVolumeGroupSnapshotResponse>>
  createVolumeGroupSnapshot(CreateVolumeGroupSnapshotRequest request);

  process::Future<RPCResult<DeleteVolumeGroupSnapshotResponse>>
  deleteVolumeGroupSnapshot(DeleteVolumeGroupSnapshotRequest request);

  process::Future<RPCResult<GetVolumeGroupSnapshotResponse>>
  getVolumeGroupSnapshot(GetVolumeGroupSnapshotRequest request);

  process::Future<RPCResult<NodeStageVolumeResponse>>
  nodeStageVolume(NodeStageVolumeRequest request);

  process::Future<RPCResult<NodeUnstageVolumeResponse>>
  nodeUnstageVolume(NodeUnstageVolumeRequest request);

  process::Future<RPCResult<NodePublishVolumeResponse>>
  nodePublishVolume(NodePublishVolumeRequest request);

  process::Future<RPCResult<NodeUnpublishVolumeResponse>>
  nodeUnpublishVolume(NodeUnpublishVolumeRequest request);

  process::Future<RPCResult<NodeGetVolumeStatsResponse>>
  nodeGetVolumeStats(NodeGetVolumeStatsRequest request);

  process::Future<RPCResult<NodeGetVolumeHealthResponse>>
  nodeGetVolumeHealth(NodeGetVolumeHealthRequest request);

  process::Future<RPCResult<NodeGetStorageHealthResponse>>
  nodeGetStorageHealth(NodeGetStorageHealthRequest request);

  process::Future<RPCResult<NodeExpandVolumeResponse>>
  nodeExpandVolume(NodeExpandVolumeRequest request);

  process::Future<RPCResult<NodeGetCapabilitiesResponse>>
  nodeGetCapabilities(NodeGetCapabilitiesRequest request);

  process::Future<RPCResult<NodeGetInfoResponse>>
  nodeGetInfo(NodeGetInfoRequest request);

private:
  process::grpc::client::Connection connection;
  process::grpc::client::Runtime runtime;
};

} // namespace v1 {
} // namespace csi {
} // namespace mesos {

#endif // __CSI_V1_CLIENT_HPP__
