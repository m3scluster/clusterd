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

#ifndef __MESOS_COMMON_SYSTEM_METRICS_HPP__
#define __MESOS_COMMON_SYSTEM_METRICS_HPP__

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include <stout/fs.hpp>
#include <stout/os.hpp>
#include <stout/try.hpp>

namespace mesos {
namespace internal {
namespace system_metrics {

// Returns host CPU utilization as a percentage. The first sample is zero
// because a delta is required to calculate utilization.
inline double cpuUtilization()
{
#ifdef __linux__
  std::ifstream input("/proc/stat");
  std::string cpu;
  unsigned long long user = 0;
  unsigned long long nice = 0;
  unsigned long long system = 0;
  unsigned long long idle = 0;
  unsigned long long iowait = 0;
  unsigned long long irq = 0;
  unsigned long long softirq = 0;
  unsigned long long steal = 0;

  if (!(input >> cpu >> user >> nice >> system >> idle >> iowait >> irq >>
        softirq >> steal) || cpu != "cpu") {
    return 0.0;
  }

  const unsigned long long total =
    user + nice + system + idle + iowait + irq + softirq + steal;
  const unsigned long long idleTime = idle + iowait;

  static unsigned long long previousTotal = 0;
  static unsigned long long previousIdle = 0;
  if (previousTotal == 0 || total <= previousTotal || idleTime < previousIdle) {
    previousTotal = total;
    previousIdle = idleTime;
    return 0.0;
  }

  const unsigned long long totalDelta = total - previousTotal;
  const unsigned long long idleDelta = idleTime - previousIdle;
  previousTotal = total;
  previousIdle = idleTime;

  return totalDelta == 0
    ? 0.0
    : 100.0 * static_cast<double>(totalDelta - idleDelta) /
        static_cast<double>(totalDelta);
#else
  return 0.0;
#endif
}


inline double memoryUtilization()
{
#ifdef __linux__
  std::ifstream input("/proc/meminfo");
  std::string key;
  unsigned long long value = 0;
  std::string unit;
  unsigned long long total = 0;
  unsigned long long available = 0;

  while (input >> key >> value >> unit) {
    if (key == "MemTotal:") {
      total = value;
    } else if (key == "MemAvailable:") {
      available = value;
    }
  }

  return total == 0
    ? 0.0
    : 100.0 * static_cast<double>(total - std::min(total, available)) /
        static_cast<double>(total);
#else
  return 0.0;
#endif
}


inline double diskUtilization(const std::string& path)
{
  Try<double> usage = fs::usage(path);
  return usage.isError() ? 0.0 : usage.get() * 100.0;
}


inline double loadUtilization()
{
  Try<os::Load> load = os::loadavg();
  Try<long> cpus = os::cpus();
  if (load.isError() || cpus.isError() || cpus.get() <= 0) {
    return 0.0;
  }

  return 100.0 * load->one / static_cast<double>(cpus.get());
}


// Returns the average NVIDIA GPU utilization when nvidia-smi is available.
// A zero value means that no supported GPU telemetry was available at scrape
// time; existing scheduler GPU capacity/allocation metrics are unaffected.
inline double gpuUtilization()
{
  FILE* process = popen(
      "nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits 2>/dev/null",
      "r");
  if (process == nullptr) {
    return 0.0;
  }

  double total = 0.0;
  unsigned int count = 0;
  char line[128];
  while (fgets(line, sizeof(line), process) != nullptr) {
    char* end = nullptr;
    const double value = std::strtod(line, &end);
    if (end != line && value >= 0.0) {
      total += value;
      ++count;
    }
  }
  pclose(process);

  return count == 0 ? 0.0 : total / static_cast<double>(count);
}

} // namespace system_metrics
} // namespace internal
} // namespace mesos

#endif // __MESOS_COMMON_SYSTEM_METRICS_HPP__
