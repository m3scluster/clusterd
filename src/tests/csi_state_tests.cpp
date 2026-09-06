// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

#include <gtest/gtest.h>

#include "csi/state.hpp"

using mesos::csi::state::GroupSnapshotState;
using mesos::csi::state::PluginState;
using mesos::csi::state::VolumeModifyState;

TEST(CSIStateTest, ControllerLifecycleTransitions)
{
  EXPECT_TRUE(mesos::csi::state::isValidModifyTransition(
      VolumeModifyState::UNKNOWN, VolumeModifyState::PENDING));
  EXPECT_TRUE(mesos::csi::state::isValidModifyTransition(
      VolumeModifyState::PENDING, VolumeModifyState::APPLIED));
  EXPECT_FALSE(mesos::csi::state::isValidModifyTransition(
      VolumeModifyState::APPLIED, VolumeModifyState::PENDING));

  EXPECT_TRUE(mesos::csi::state::isValidGroupSnapshotTransition(
      GroupSnapshotState::UNKNOWN, GroupSnapshotState::CREATING));
  EXPECT_TRUE(mesos::csi::state::isValidGroupSnapshotTransition(
      GroupSnapshotState::READY, GroupSnapshotState::DELETING));
  EXPECT_FALSE(mesos::csi::state::isValidGroupSnapshotTransition(
      GroupSnapshotState::DELETED, GroupSnapshotState::READY));
}

TEST(CSIStateTest, PluginStateRoundTripPreservesRecoveryRecords)
{
  PluginState state;
  (*state.mutable_volume_health())["volume-1"].set_volume_id("volume-1");
  (*state.mutable_volume_health())["volume-1"].set_abnormal(true);
  (*state.mutable_volume_modifications())["volume-1"].set_state(
      VolumeModifyState::PENDING);
  (*state.mutable_volume_modifications())["volume-1"].set_volume_id("volume-1");
  (*state.mutable_group_snapshots())["group-1"].set_state(
      GroupSnapshotState::READY);
  (*state.mutable_group_snapshots())["group-1"].set_group_snapshot_id("group-1");

  std::string wire;
  ASSERT_TRUE(state.SerializeToString(&wire));

  PluginState recovered;
  ASSERT_TRUE(recovered.ParseFromString(wire));
  ASSERT_EQ(1, recovered.volume_health_size());
  EXPECT_TRUE(recovered.volume_health().at("volume-1").abnormal());
  EXPECT_EQ(
      VolumeModifyState::PENDING,
      recovered.volume_modifications().at("volume-1").state());
  EXPECT_EQ(
      GroupSnapshotState::READY,
      recovered.group_snapshots().at("group-1").state());
}