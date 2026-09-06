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


#ifndef __CSI_STATE_HPP__
#define __CSI_STATE_HPP__

// ONLY USEFUL AFTER RUNNING PROTOC.
#include "csi/state.pb.h"

namespace mesos {
namespace csi {
namespace state {

inline bool isValidModifyTransition(
    const VolumeModifyState::State from,
    const VolumeModifyState::State to)
{
  return
    (from == VolumeModifyState::UNKNOWN && to == VolumeModifyState::PENDING) ||
    (from == VolumeModifyState::PENDING &&
     (to == VolumeModifyState::APPLIED || to == VolumeModifyState::FAILED));
}


inline bool isValidGroupSnapshotTransition(
    const GroupSnapshotState::State from,
    const GroupSnapshotState::State to)
{
  return
    (from == GroupSnapshotState::UNKNOWN &&
     to == GroupSnapshotState::CREATING) ||
    (from == GroupSnapshotState::CREATING &&
     (to == GroupSnapshotState::READY || to == GroupSnapshotState::FAILED)) ||
    (from == GroupSnapshotState::READY && to == GroupSnapshotState::DELETING) ||
    (from == GroupSnapshotState::DELETING &&
     (to == GroupSnapshotState::DELETED || to == GroupSnapshotState::FAILED));
}


inline std::ostream& operator<<(
    std::ostream& stream,
    const VolumeState::State& state)
{
  return stream << VolumeState::State_Name(state);
}

} // namespace state {
} // namespace csi {
} // namespace mesos {

#endif // __CSI_STATE_HPP__
