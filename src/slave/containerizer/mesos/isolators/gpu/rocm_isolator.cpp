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

#include <stdint.h>

#include <sys/mount.h>

// This header include must be enclosed in an `extern "C"` block to
// workaround a bug in glibc <= 2.12 (see MESOS-7378).
//
// TODO(neilc): Remove this when we no longer support glibc <= 2.12.
extern "C" {
#include <sys/sysmacros.h>
}

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <process/collect.hpp>
#include <process/defer.hpp>
#include <process/future.hpp>
#include <process/id.hpp>

#include <stout/error.hpp>
#include <stout/foreach.hpp>
#include <stout/hashmap.hpp>
#include <stout/linkedhashmap.hpp>
#include <stout/option.hpp>
#include <stout/os.hpp>
#include <stout/try.hpp>

#include "common/protobuf_utils.hpp"

#include "linux/cgroups.hpp"
#include "linux/cgroups2.hpp"
#include "linux/fs.hpp"

#include "slave/flags.hpp"

#include "slave/containerizer/containerizer.hpp"

#include "slave/containerizer/mesos/isolator.hpp"

#include "slave/containerizer/mesos/isolators/cgroups/constants.hpp"

#include "slave/containerizer/mesos/isolators/gpu/rocm_allocator.hpp"
#include "slave/containerizer/mesos/isolators/gpu/rocm_isolator.hpp"

#include "slave/containerizer/mesos/paths.hpp"

using cgroups::devices::Entry;

using docker::spec::v1::ImageManifest;

using mesos::slave::ContainerClass;
using mesos::slave::ContainerConfig;
using mesos::slave::ContainerLaunchInfo;
using mesos::slave::ContainerLimitation;
using mesos::slave::ContainerMountInfo;
using mesos::slave::ContainerState;
using mesos::slave::Isolator;

using process::defer;
using process::Failure;
using process::Future;
using process::PID;

using std::list;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace mesos {
namespace internal {
namespace slave {


DeviceManager::NonWildcardEntry rocmDeviceEntry(const Gpu& gpu)
{
  DeviceManager::NonWildcardEntry entry;
  entry.selector.type =
    DeviceManager::NonWildcardEntry::Selector::Type::CHARACTER;
  entry.selector.major = gpu.major;
  entry.selector.minor = gpu.minor;
  entry.access.read = true;
  entry.access.write = true;
  entry.access.mknod = false;
  return entry;
}


vector<DeviceManager::NonWildcardEntry> rocmDeviceEntries(const set<Gpu>& gpus)
{
  vector<DeviceManager::NonWildcardEntry> device_entries;
  foreach (const Gpu& gpu, gpus) {
    device_entries.push_back(rocmDeviceEntry(gpu));
  }
  return device_entries;
}


RocmGpuIsolatorProcess::RocmGpuIsolatorProcess(
    const Flags& _flags,
    const string& _hierarchy,
    const RocmGpuAllocator& _allocator,
    const LinkedHashMap<string, cgroups::devices::Entry>& _controlDevices,
    const process::Owned<DeviceManager>& _deviceManager,
    const bool _usingCgroups2)
  : ProcessBase(process::ID::generate("mesos-rocm-gpu-isolator")),
    flags(_flags),
    hierarchy(_hierarchy),
    allocator(_allocator),
    controlDevices(_controlDevices),
    deviceManager(_deviceManager),
    usingCgroups2(_usingCgroups2) {}


Try<Isolator*> RocmGpuIsolatorProcess::create(
    const Flags& flags,
    const RocmGpuAllocator& allocator,
    const process::Owned<DeviceManager>& deviceManager)
{
  // Make sure both the 'cgroups/devices' (or 'cgroups/all')
  // and the 'filesystem/linux' isolators are present.
  vector<string> tokens = strings::tokenize(flags.isolation, ",");

  auto gpuIsolator =
    std::find(tokens.begin(), tokens.end(), "gpu/rocm");

  auto devicesIsolator =
    std::find(tokens.begin(), tokens.end(), "cgroups/devices");

  auto cgroupsAllIsolator =
    std::find(tokens.begin(), tokens.end(), "cgroups/all");

  auto filesystemIsolator =
    std::find(tokens.begin(), tokens.end(), "filesystem/linux");

  CHECK(gpuIsolator != tokens.end());

  if (cgroupsAllIsolator != tokens.end()) {
    // The reason that we need to check if `devices` cgroups subsystem is
    // enabled is, when `cgroups/all` is specified in the `--isolation` agent
    // flag, cgroups isolator will only load the enabled subsystems. So if
    // `cgroups/all` is specified but `devices` is not enabled, cgroups isolator
    // will not load `devices` subsystem in which case we should error out.
    Try<bool> result = cgroups::enabled("devices");
    if (result.isError()) {
      return Error(
          "Failed to check if the `devices` cgroups subsystem"
          " is enabled by kernel: " + result.error());
    } else if (!result.get()) {
      return Error(
          "The `devices` cgroups subsystem is not enabled by the kernel");
    }
  } else if (devicesIsolator == tokens.end()) {
    return Error(
        "The 'cgroups/devices' or 'cgroups/all' isolator must be"
        " enabled in order to use the 'gpu/rocm' isolator");
  }

  if (filesystemIsolator == tokens.end()) {
    return Error("The 'filesystem/linux' isolator must be enabled in"
                 " order to use the 'gpu/rocm' isolator");
  }

  Result<string> hierarchy = cgroups::hierarchy(CGROUP_SUBSYSTEM_DEVICES_NAME);
  Try<bool> cgroups2Mounted = cgroups2::mounted();

  if (cgroups2Mounted.isError()) {
    return Error("Failed to check whether system is using cgroups v2: " +
                 cgroups2Mounted.error());
  }

  if (!(*cgroups2Mounted) && hierarchy.isError()) {
    return Error("Failed to retrieve the " + CGROUP_SUBSYSTEM_DEVICES_NAME
                 + " subsystem hierarchy when cgroups v2 is not being used: "
                 + hierarchy.error());
  }

  if (!(*cgroups2Mounted) && !hierarchy.isSome()) {
    return Error("Failed to determine whether cgroups v1 or v2 is being used: "
                 "did not find cgroups v2 mount, nor a v1 'devices' subsystem");
  }

  // `/dev/kfd` is the ROCm control device.  Per-GPU access is granted
  // through the selected DRM render nodes in `update()`.
  LinkedHashMap<string, cgroups::devices::Entry> deviceEntries;
  Try<dev_t> device = os::stat::rdev("/dev/kfd");
  if (device.isError()) {
    return Error("Failed to obtain device ID for '/dev/kfd': " +
                 device.error());
  }

  cgroups::devices::Entry entry;
  entry.selector.type = Entry::Selector::Type::CHARACTER;
  entry.selector.major = major(device.get());
  entry.selector.minor = minor(device.get());
  entry.access.read = true;
  entry.access.write = true;
  entry.access.mknod = true;
  deviceEntries["/dev/kfd"] = entry;

  process::Owned<MesosIsolatorProcess> process(
      new RocmGpuIsolatorProcess(
          flags,
          *cgroups2Mounted ? flags.cgroups_hierarchy : hierarchy.get(),
          allocator,
          deviceEntries,
          deviceManager,
          *cgroups2Mounted));

  return new MesosIsolator(process);
}


bool RocmGpuIsolatorProcess::supportsNesting()
{
  return true;
}


bool RocmGpuIsolatorProcess::supportsStandalone()
{
  return true;
}


Future<Nothing> RocmGpuIsolatorProcess::recover(
    const vector<ContainerState>& states,
    const hashset<ContainerID>& orphans)
{
  if (usingCgroups2) {
    return deviceManager->state()
      .then(defer(PID<RocmGpuIsolatorProcess>(this),
                  &RocmGpuIsolatorProcess::_recover,
                  states,
                  lambda::_1));
  }
  return _recover(states);
}


Future<Nothing> RocmGpuIsolatorProcess::_recover(
    const vector<ContainerState>& states,
    const hashmap<string, DeviceManager::CgroupDeviceAccess>& cgroup_states)
{
  vector<Future<Nothing>> futures;

  foreach (const ContainerState& state, states) {
    const ContainerID& containerId = state.container_id();

    // If we are a nested container, we skip the recover because our
    // root ancestor will recover the GPU state from the cgroup for us.
    if (containerId.has_parent()) {
      continue;
    }

    const string cgroup = path::join(flags.cgroups_root, containerId.value());

    if ((usingCgroups2 && !cgroups2::exists(cgroup)) ||
        (!usingCgroups2 && !cgroups::exists(hierarchy, cgroup))) {
      // This may occur if the executor has exited and the isolator
      // has destroyed the cgroup but the slave dies before noticing
      // this. This will be detected when the containerizer tries to
      // monitor the executor's pid.
      LOG(WARNING) << "Couldn't find the cgroup '" << cgroup << "' "
                   << "in hierarchy '" << hierarchy << "' "
                   << "for container " << containerId;
      continue;
    }

    infos[containerId] = new Info(containerId, cgroup);

    // Determine which GPUs are allocated to this container.
    const set<Gpu>& available = allocator.total();
    set<Gpu> containerGpus;

    if (usingCgroups2) {
      // If the cgroup does not have a recorded state in the DeviceManager,
      // then no gpu would be granted access anyway. So we can skip to the
      // cgroup for the next container state.
      if (!cgroup_states.contains(cgroup)) {
        LOG(WARNING) << "Couldn't find the cgroup '" << cgroup << "'"
                     << " in the device manager for container " << containerId;
        continue;
      }

      const DeviceManager::CgroupDeviceAccess& device_access =
        cgroup_states.at(cgroup);
      foreach (const Gpu& gpu, available) {
        if (device_access.is_access_granted(rocmDeviceEntry(gpu))) {
          containerGpus.insert(gpu);
        }
      }
    } else {
      Try<vector<Entry>> entries = cgroups::devices::list(hierarchy, cgroup);
      if (entries.isError()) {
        return Failure("Failed to obtain devices list for cgroup"
                       " '" + cgroup + "': " + entries.error());
      }

      foreach (const Entry& entry, entries.get()) {
        foreach (const Gpu& gpu, available) {
          if (entry.selector.major == gpu.major &&
              entry.selector.minor == gpu.minor) {
            containerGpus.insert(gpu);
            break;
          }
        }
      }
    }

    futures.push_back(__recover(containerId, containerGpus));
  }

  return collect(futures).then([]() { return Nothing(); });
}


Future<Nothing> RocmGpuIsolatorProcess::__recover(
    const ContainerID& containerId,
    const set<Gpu>& containerGpus)
{
  return allocator.allocate(containerGpus)
    .then(defer(self(), [=]() -> Future<Nothing> {
      infos[containerId]->allocated = containerGpus;
      return Nothing();
    }));
}


Future<Option<ContainerLaunchInfo>> RocmGpuIsolatorProcess::prepare(
    const ContainerID& containerId,
    const mesos::slave::ContainerConfig& containerConfig)
{
  if (containerId.has_parent()) {
    // If we are a nested container in the `DEBUG` class, then we
    // don't need to do anything special to prepare ourselves for GPU
    // support. All Rocm volumes will be inherited from our parent.
    if (containerConfig.has_container_class() &&
        containerConfig.container_class() == ContainerClass::DEBUG) {
      return None();
    }

    // If we are a nested container in a different class, we don't
    // need to maintain an `Info()` struct about the container (since
    // we don't directly allocate any GPUs to it), but we do need to
    // mount the necessary Rocm libraries into the container (since
    // we live in a different mount namespace than our parent). We
    // directly call `_prepare()` to do this for us.
    return _prepare(containerId, containerConfig);
  }

  if (infos.contains(containerId)) {
    return Failure("Container has already been prepared");
  }

  infos[containerId] = new Info(
      containerId, path::join(flags.cgroups_root, containerId.value()));

  // Grant access to all control devices.
  //
  // This allows standard NVIDIA tools like `nvidia-smi` to be
  // used within the container even if no GPUs are allocated.
  // Without these devices, these tools fail abnormally.
  if (usingCgroups2) {
    // Cgroups2 requires us to attach ebpf programs so we use the deviceManager.
    vector<DeviceManager::NonWildcardEntry> control_entries =
      CHECK_NOTERROR(DeviceManager::NonWildcardEntry::create(
          controlDevices.values()));

    return deviceManager->reconfigure(
        infos[containerId]->cgroup,
        control_entries,
        {})
      .then(defer(self(), [=] {
        return update(containerId, containerConfig.resources())
          .then(defer(PID<RocmGpuIsolatorProcess>(this),
                      &RocmGpuIsolatorProcess::_prepare,
                      containerId,
                      containerConfig));
      }));
  }

  // Cgroups v1:
  foreachpair (const string& devicePath, const Entry& device, controlDevices) {
    Try<Nothing> allow = cgroups::devices::allow(
        hierarchy, infos[containerId]->cgroup, device);

    if (allow.isError()) {
      return Failure("Failed to grant cgroups access to"
                     " '" + devicePath + "': " + allow.error());
    }
  }

  return update(containerId, containerConfig.resources())
    .then(defer(PID<RocmGpuIsolatorProcess>(this),
                &RocmGpuIsolatorProcess::_prepare,
                containerId,
                containerConfig));
}


// Copy the ROCm device nodes into an isolated root filesystem.  ROCm
// userspace libraries intentionally remain part of the container image.
Future<Option<ContainerLaunchInfo>> RocmGpuIsolatorProcess::_prepare(
    const ContainerID& containerId,
    const mesos::slave::ContainerConfig& containerConfig)
{
  if (!containerConfig.has_rootfs()) {
    return None();
  }

  ContainerLaunchInfo launchInfo;
  const string devicesDir = containerizer::paths::getContainerDevicesPath(
      flags.runtime_dir, containerId);
  if (!os::exists(devicesDir)) {
    return Failure("Missing container devices directory '" + devicesDir + "'");
  }

  vector<string> devices = {"/dev/kfd"};
  Try<list<string>> renderNodes = os::glob("/dev/dri/renderD*");
  if (renderNodes.isError()) {
    return Failure("Failed to glob DRM render devices: " + renderNodes.error());
  }

  if (!infos.contains(containerId)) {
    return Failure("Unknown container");
  }

  const set<Gpu>& allocated = infos.at(containerId)->allocated;
  foreach (const string& node, renderNodes.get()) {
    Try<dev_t> device = os::stat::rdev(node);
    if (device.isError()) {
      return Failure("Failed to obtain device ID for '" + node + "': " +
                     device.error());
    }

    Gpu gpu;
    gpu.major = major(device.get());
    gpu.minor = minor(device.get());
    if (allocated.count(gpu) > 0) {
      devices.push_back(node);
    }
  }

  foreach (const string& device, devices) {
    const string devicePath = path::join(
        devicesDir, strings::remove(device, "/dev/", strings::PREFIX), device);
    Try<Nothing> mknod = fs::chroot::copyDeviceNode(device, devicePath);
    if (mknod.isError()) {
      return Failure("Failed to copy device '" + device + "': " + mknod.error());
    }
    Try<Nothing> chmod = os::chmod(devicePath, 0666);
    if (chmod.isError()) {
      return Failure("Failed to set permissions on device '" + device + "': " +
                     chmod.error());
    }
    *launchInfo.add_mounts() = protobuf::slave::createContainerMount(
        devicePath, path::join(containerConfig.rootfs(), device), MS_BIND);
  }
  return launchInfo;
}

Future<Nothing> RocmGpuIsolatorProcess::update(
    const ContainerID& containerId,
    const Resources& resourceRequests,
    const google::protobuf::Map<string, Value::Scalar>& resourceLimits)
{
  if (containerId.has_parent()) {
    return Failure("Not supported for nested containers");
  }

  if (!infos.contains(containerId)) {
    return Failure("Unknown container");
  }

  Info* info = CHECK_NOTNULL(infos[containerId]);

  Option<double> gpus = resourceRequests.gpus();

  // Make sure that the `gpus` resource is not fractional.
  // We rely on scalar resources only having 3 digits of precision.
  if (static_cast<long long>(gpus.getOrElse(0.0) * 1000.0) % 1000 != 0) {
    return Failure("The 'gpus' resource must be an unsigned integer");
  }

  size_t requested =
    static_cast<size_t>(resourceRequests.gpus().getOrElse(0.0));

  // Update the GPU allocation to reflect the new total.
  if (requested > info->allocated.size()) {
    size_t additional = requested - info->allocated.size();

    return allocator.allocate(additional)
      .then(defer(PID<RocmGpuIsolatorProcess>(this),
                  &RocmGpuIsolatorProcess::_update,
                  containerId,
                  lambda::_1));
  } else if (requested < info->allocated.size()) {
    size_t fewer = info->allocated.size() - requested;

    set<Gpu> deallocated;

    if (usingCgroups2) {
      vector<DeviceManager::NonWildcardEntry> deallocated_entries;

      for (size_t i = 0; i < fewer; i++) {
        const auto gpu = info->allocated.begin();

        deallocated_entries.push_back(rocmDeviceEntry(*gpu));
        deallocated.insert(*gpu);
        info->allocated.erase(gpu);
      }

      return deviceManager->reconfigure(info->cgroup, {}, {deallocated_entries})
        .then(defer(self(), [=] { return allocator.deallocate(deallocated); }));
    }

    // Cgroups v1:
    for (size_t i = 0; i < fewer; i++) {
      const auto gpu = info->allocated.begin();

      cgroups::devices::Entry entry;
      entry.selector.type = Entry::Selector::Type::CHARACTER;
      entry.selector.major = gpu->major;
      entry.selector.minor = gpu->minor;
      entry.access.read = true;
      entry.access.write = true;
      entry.access.mknod = true;

      Try<Nothing> deny = cgroups::devices::deny(
          hierarchy, info->cgroup, entry);

      if (deny.isError()) {
        return Failure("Failed to deny cgroups access to GPU device"
                       " '" + stringify(entry) + "': " + deny.error());
      }

      deallocated.insert(*gpu);
      info->allocated.erase(gpu);
    }

    return allocator.deallocate(deallocated);
  }

  return Nothing();
}


Future<Nothing> RocmGpuIsolatorProcess::_update(
    const ContainerID& containerId,
    const set<Gpu>& allocation)
{
  if (!infos.contains(containerId)) {
    return Failure("Failed to complete GPU allocation: unknown container");
  }

  Info* info = CHECK_NOTNULL(infos.at(containerId));

  if (usingCgroups2) {
    return deviceManager->reconfigure(
        info->cgroup,
        {rocmDeviceEntries(allocation)},
        {})
      .then(defer(self(), [=] {
        info->allocated = allocation;
        return Nothing();
      }));
  }

  // Cgroups v1:
  foreach (const Gpu& gpu, allocation) {
    cgroups::devices::Entry entry;
    entry.selector.type = Entry::Selector::Type::CHARACTER;
    entry.selector.major = gpu.major;
    entry.selector.minor = gpu.minor;
    entry.access.read = true;
    entry.access.write = true;
    entry.access.mknod = true;

    Try<Nothing> allow = cgroups::devices::allow(
        hierarchy, info->cgroup, entry);

    if (allow.isError()) {
      return Failure("Failed to grant cgroups access to GPU device"
                     " '" + stringify(entry) + "': " + allow.error());
    }
  }

  info->allocated = allocation;

  return Nothing();
}


Future<ResourceStatistics> RocmGpuIsolatorProcess::usage(
    const ContainerID& containerId)
{
  if (containerId.has_parent()) {
    return Failure("Not supported for nested containers");
  }

  if (!infos.contains(containerId)) {
    return Failure("Unknown container");
  }

  // TODO(rtodd): Obtain usage information from ROCM.

  ResourceStatistics result;
  return result;
}


Future<Nothing> RocmGpuIsolatorProcess::cleanup(
    const ContainerID& containerId)
{
  // If we are a nested container, we don't have an `Info()` struct to
  // cleanup, so we just return immediately.
  if (containerId.has_parent()) {
    return Nothing();
  }

  // Multiple calls may occur during test clean up.
  if (!infos.contains(containerId)) {
    VLOG(1) << "Ignoring cleanup request for unknown container " << containerId;

    return Nothing();
  }

  Info* info = CHECK_NOTNULL(infos.at(containerId));

  // Make any remaining GPUs available.
  return allocator.deallocate(info->allocated)
    .then(defer(self(), [=]() -> Future<Nothing> {
      CHECK(infos.contains(containerId));
      delete infos.at(containerId);
      infos.erase(containerId);

      return Nothing();
    }));
}

} // namespace slave {
} // namespace internal {
} // namespace mesos {


