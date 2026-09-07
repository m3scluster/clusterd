// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

#include <process/gtest.hpp>

#include "cli/execute.hpp"
#include "internal/devolve.hpp"

namespace mesos {
namespace internal {
namespace tests {

TEST(MesosExecuteTest, SetsModernRoleAndMultiRoleCapability)
{
  v1::FrameworkInfo frameworkInfo;

  cli::setFrameworkRole(&frameworkInfo, "mc");

  EXPECT_FALSE(frameworkInfo.has_role());
  ASSERT_EQ(1, frameworkInfo.roles_size());
  EXPECT_EQ("mc", frameworkInfo.roles(0));
  ASSERT_EQ(1, frameworkInfo.capabilities_size());
  EXPECT_EQ(
      ::mesos::v1::FrameworkInfo::Capability::MULTI_ROLE,
      frameworkInfo.capabilities(0).type());

  const FrameworkInfo legacy = ::mesos::internal::devolve(frameworkInfo);
  ASSERT_EQ(1, legacy.roles_size());
  EXPECT_EQ("mc", legacy.roles(0));

  v1::scheduler::Call call;
  call.set_type(v1::scheduler::Call::SUBSCRIBE);
  call.mutable_subscribe()->mutable_framework_info()->CopyFrom(frameworkInfo);

  const scheduler::Call legacyCall = ::mesos::internal::devolve(call);
  ASSERT_EQ(
      1,
      legacyCall.subscribe().framework_info().roles_size());
  EXPECT_EQ("mc", legacyCall.subscribe().framework_info().roles(0));
}


TEST(MesosExecuteTest, DefaultRoleIsMesosExecute)
{
  v1::FrameworkInfo frameworkInfo;

  cli::setFrameworkRole(&frameworkInfo, "mesos-execute");

  ASSERT_EQ(1, frameworkInfo.roles_size());
  EXPECT_EQ("mesos-execute", frameworkInfo.roles(0));
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {