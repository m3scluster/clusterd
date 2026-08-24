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

#include <algorithm>
#include <iterator>
#include <list>
#include <ostream>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include <sys/sysmacros.h>
}

#include <process/check.hpp>
#include <process/dispatch.hpp>
#include <process/future.hpp>
#include <process/once.hpp>
#include <process/process.hpp>

#include <stout/error.hpp>
#include <stout/foreach.hpp>
#include <stout/nothing.hpp>
#include <stout/os.hpp>
#include <stout/set.hpp>
#include <stout/stringify.hpp>
#include <stout/strings.hpp>
#include <stout/try.hpp>

#include "slave/flags.hpp"

#include "slave/containerizer/mesos/isolators/gpu/rocm_allocator.hpp"

using process::Failure;
using process::Future;
using process::Once;
using process::PID;

using std::list;
using std::ostream;
using std::set;
using std::string;
using std::vector;

namespace mesos {
namespace internal {
namespace slave {

namespace {

template <typename T>
set<T> difference(const set<T>& left, const set<T>& right)
{
  set<T> result;
  std::set_difference(
      left.begin(), left.end(), right.begin(), right.end(),
      std::inserter(result, result.begin()));
  return result;
}


Try<vector<Gpu>> devices()
{
  Try<list<string>> paths = os::glob("/dev/dri/renderD*");
  if (paths.isError()) {
    return Error("Failed to enumerate DRM render devices: " + paths.error());
  }

  vector<Gpu> result;
  foreach (const string& path, paths.get()) {
    Try<dev_t> device = os::stat::rdev(path);
    if (device.isError()) {
      return Error("Failed to obtain device ID for '" + path + "': " +
                   device.error());
    }

    Gpu gpu;
    gpu.major = major(device.get());
    gpu.minor = minor(device.get());
    result.push_back(gpu);
  }

  return result;
}


Try<set<Gpu>> enumerateGpus(const Flags& flags, const Resources& resources)
{
  Try<vector<Gpu>> discovered = devices();
  if (discovered.isError()) {
    return Error(discovered.error());
  }

  vector<unsigned int> indices;
  if (flags.rocm_gpu_devices.isSome()) {
    indices = flags.rocm_gpu_devices.get();
  } else {
    for (size_t i = 0; i < resources.gpus().getOrElse(0); ++i) {
      indices.push_back(i);
    }
  }

  set<Gpu> gpus;
  foreach (unsigned int index, indices) {
    if (index >= discovered->size()) {
      return Error("ROCm GPU device index " + stringify(index) +
                   " does not exist");
    }
    gpus.insert(discovered->at(index));
  }
  return gpus;
}


Try<Resources> enumerateGpuResources(const Flags& flags)
{
  const vector<string> tokens = strings::tokenize(flags.isolation, ",");
  const set<string> isolators(tokens.begin(), tokens.end());

  if (flags.rocm_gpu_devices.isSome() && isolators.count("gpu/rocm") == 0) {
    return Error("'--rocm_gpu_devices' can only be specified if the"
                 " `--isolation` flag contains 'gpu/rocm'");
  }

  Try<Resources> parsed = Resources::parse(
      flags.resources.getOrElse(""), flags.default_role);
  if (parsed.isError()) {
    return Error(parsed.error());
  }

  Resources resources = parsed->filter(
      [](const Resource& resource) { return resource.name() == "gpus"; });

  if (isolators.count("gpu/rocm") == 0) {
    return resources;
  }

  if (strings::contains(flags.resources.getOrElse(""), "gpus") &&
      resources.gpus().getOrElse(0) == 0) {
    if (flags.rocm_gpu_devices.isSome()) {
      return Error("'--rocm_gpu_devices' cannot be specified when"
                   " '--resources' specifies 0 GPUs");
    }
    return Resources();
  }

  Try<vector<Gpu>> discovered = devices();
  if (discovered.isError()) {
    return Error(discovered.error());
  }

  if (!os::exists("/dev/kfd")) {
    return Error("Cannot enable 'gpu/rocm': '/dev/kfd' does not exist");
  }

  if (resources.gpus().isSome()) {
    if (!flags.rocm_gpu_devices.isSome()) {
      return Error("The 'gpus' resource cannot be set without also"
                   " setting '--rocm_gpu_devices'");
    }
    if (flags.rocm_gpu_devices->size() != resources.gpus().get()) {
      return Error("'--resources' and '--rocm_gpu_devices' specify"
                   " different numbers of GPU devices");
    }
    return resources;
  }

  return Resources::parse("gpus", stringify(discovered->size()),
                          flags.default_role).get();
}


class RocmGpuAllocatorProcess
  : public process::Process<RocmGpuAllocatorProcess>
{
public:
  RocmGpuAllocatorProcess(const set<Gpu>& gpus)
    : available(gpus) {}

  Future<set<Gpu>> allocate(size_t count)
  {
    if (available.size() < count) {
      return Failure("Requested " + stringify(count) + " gpus but only"
                     " " + stringify(available.size()) + " available");
    }

    set<Gpu> allocation(
        available.begin(),
        std::next(available.begin(), count));

    return allocate(allocation)
      .then([=]() -> Future<set<Gpu>> { return allocation; });
  }

  Future<Nothing> allocate(const set<Gpu>& gpus)
  {
    set<Gpu> allocation = available & gpus;

    if (allocation.size() < gpus.size()) {
      return Failure(stringify(difference(gpus, allocation)) +
                     " are not available");
    }

    available = difference(available, allocation);
    allocated = allocated | allocation;

    return Nothing();
  }

  Future<Nothing> deallocate(const set<Gpu>& gpus)
  {
    set<Gpu> deallocation = allocated & gpus;

    if (deallocation.size() < gpus.size()) {
      return Failure(stringify(difference(gpus, deallocation)) +
                     " are not allocated");
    }

    allocated = difference(allocated, deallocation);
    available = available | deallocation;

    return Nothing();
  }

private:
  set<Gpu> available;
  set<Gpu> allocated;
};

} // namespace {


struct RocmGpuAllocator::Data
{
  Data(const set<Gpu>& gpus_)
    : gpus(gpus_),
      process(process::spawn(new RocmGpuAllocatorProcess(gpus_), true)) {}

  ~Data()
  {
    process::terminate(process);
  }

  const set<Gpu> gpus;
  PID<RocmGpuAllocatorProcess> process;
};


Try<RocmGpuAllocator> RocmGpuAllocator::create(
    const Flags& flags,
    const Resources& resources)
{
  Try<set<Gpu>> gpus = enumerateGpus(flags, resources);
  if (gpus.isError()) {
    return Error(gpus.error());
  }

  return RocmGpuAllocator(gpus.get());
}


Try<Resources> RocmGpuAllocator::resources(const Flags& flags)
{
  return enumerateGpuResources(flags);
}


RocmGpuAllocator::RocmGpuAllocator(
    const set<Gpu>& gpus)
  : data(std::make_shared<RocmGpuAllocator::Data>(gpus)) {}


const set<Gpu>& RocmGpuAllocator::total() const { return data->gpus; }


Future<set<Gpu>> RocmGpuAllocator::allocate(size_t count)
{
  // Need to disambiguate for the compiler.
  Future<set<Gpu>> (RocmGpuAllocatorProcess::*allocate)(size_t) =
    &RocmGpuAllocatorProcess::allocate;

  return process::dispatch(data->process, allocate, count);
}


Future<Nothing> RocmGpuAllocator::allocate(const set<Gpu>& gpus)
{
  // Need to disambiguate for the compiler.
  Future<Nothing> (RocmGpuAllocatorProcess::*allocate)(const set<Gpu>&) =
    &RocmGpuAllocatorProcess::allocate;

  return process::dispatch(data->process, allocate, gpus);
}


Future<Nothing> RocmGpuAllocator::deallocate(const set<Gpu>& gpus)
{
  return process::dispatch(
      data->process,
      &RocmGpuAllocatorProcess::deallocate,
      gpus);
}


} // namespace slave {
} // namespace internal {
} // namespace mesos {


