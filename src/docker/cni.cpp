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

#include "docker/cni.hpp"

#include <dirent.h>

#include <string>
#include <vector>

#include <stout/os.hpp>
#include <stout/strings.hpp>

using std::string;
using std::vector;

namespace mesos {
namespace internal {
namespace docker {
namespace cni {

Try<string> getNetworkConfigFile(const string& network, const string& path)
{
  foreach (const string& directory, strings::split(path, ":")) {
    DIR* dir = opendir(directory.c_str());
    if (dir == nullptr) {
      continue;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (entry->d_type != DT_REG) {
        continue;
      }

      const string filename = directory + "/" + entry->d_name;
      Try<string> content = os::read(filename);
      if (content.isError()) {
        continue;
      }

      Try<JSON::Object> config = JSON::parse<JSON::Object>(content.get());
      if (config.isError()) {
        continue;
      }

      Result<JSON::String> name = config->at<JSON::String>("name");
      if (name.isSome() && name->value == network) {
        closedir(dir);
        return filename;
      }
    }

    closedir(dir);
  }

  return Error("Cannot find CNI network configuration for '" + network + "'");
}


Try<JSON::Object> getNetworkConfigJSON(const string& filename)
{
  Try<string> content = os::read(filename);
  if (content.isError()) {
    return Error("Cannot read CNI network configuration: " + content.error());
  }

  Try<JSON::Object> config = JSON::parse<JSON::Object>(content.get());
  if (config.isError()) {
    return Error("Cannot parse CNI network configuration: " + config.error());
  }

  return config.get();
}


Try<NetworkInfo> parseNetworkInfo(const string& result)
{
  Try<JSON::Object> json = JSON::parse<JSON::Object>(result);
  if (json.isError()) {
    return Error("JSON parse CNI network info failed: " + json.error());
  }

  NetworkInfo network;
  Result<JSON::Object> ip4 = json->at<JSON::Object>("ip4");
  if (ip4.isSome()) {
    Result<JSON::String> ip = ip4->at<JSON::String>("ip");
    if (ip.isError() || ip.isNone() || ip->value.empty()) {
      return Error("CNI result ip4.ip is missing or invalid");
    }

    string address = ip->value;
    const size_t slash = address.find('/');
    if (slash != string::npos) {
      address.erase(slash);
    }
    network.add_ip_addresses()->set_ip_address(address);
    return network;
  }

  Result<JSON::Object> ip6 = json->at<JSON::Object>("ip6");
  if (ip6.isSome()) {
    Result<JSON::String> ip = ip6->at<JSON::String>("ip");
    if (ip.isError() || ip.isNone() || ip->value.empty()) {
      return Error("CNI result ip6.ip is missing or invalid");
    }

    NetworkInfo::IPAddress* address = network.add_ip_addresses();
    address->set_ip_address(ip->value);
    address->set_protocol(NetworkInfo::IPv6);
    return network;
  }

  return Error("CNI result contains neither ip4 nor ip6");
}

} // namespace cni
} // namespace docker
} // namespace internal
} // namespace mesos
