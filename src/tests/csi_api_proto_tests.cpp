// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.

#include <string>

#include <gtest/gtest.h>

#include <mesos/master/master.hpp>
#include <mesos/v1/master/master.hpp>

namespace mesos {
namespace internal {
namespace tests {

namespace {

template <typename Call>
void VerifyCSIFields()
{
  const google::protobuf::Descriptor* descriptor = Call::descriptor();

  ASSERT_NE(nullptr, descriptor->FindFieldByName("type"));
  EXPECT_EQ(1, descriptor->FindFieldByName("type")->number());
  EXPECT_EQ(25, descriptor->FindFieldByName("get_volume_health")->number());
  EXPECT_EQ(26, descriptor->FindFieldByName("modify_volume")->number());
  EXPECT_EQ(
      27, descriptor->FindFieldByName("create_volume_group_snapshot")->number());
  EXPECT_EQ(
      28, descriptor->FindFieldByName("delete_volume_group_snapshot")->number());
  EXPECT_EQ(
      29, descriptor->FindFieldByName("get_volume_group_snapshot")->number());

  Call call;
  call.set_type(Call::MODIFY_VOLUME);
  call.mutable_modify_volume()->set_volume_id("volume-1");
  (*call.mutable_modify_volume()->mutable_parameters())["size"] = "10GiB";

  std::string wire;
  ASSERT_TRUE(call.SerializeToString(&wire));

  Call parsed;
  ASSERT_TRUE(parsed.ParseFromString(wire));
  EXPECT_EQ(Call::MODIFY_VOLUME, parsed.type());
  ASSERT_TRUE(parsed.has_modify_volume());
  EXPECT_EQ("volume-1", parsed.modify_volume().volume_id());
  EXPECT_EQ("10GiB", parsed.modify_volume().parameters().at("size"));
}

} // namespace

TEST(CSIAPIProtoTest, V0DescriptorAndWireCompatibility)
{
  VerifyCSIFields<mesos::master::Call>();
}

TEST(CSIAPIProtoTest, V1DescriptorAndWireCompatibility)
{
  VerifyCSIFields<mesos::v1::master::Call>();
}

} // namespace tests
} // namespace internal
} // namespace mesos
