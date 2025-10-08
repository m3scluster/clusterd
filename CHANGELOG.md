## Master

- UPDATE: glog to version 0.6.2 to fix cleanup info messages

## v1.11.0-0.8.0

- ADD: Make it possible to set the nvidia library name via env variable `MESOS_NVIDIA_LIB`
- ADD: Timestamp into CGroupsV2 Metrics of the mesos-agent for docker containers.
- CHANGE: The "Could not find the CNI plugin..." message in the docker executor
            is not an error message. It's just an information. I changed the
            Log Message to INFO instead of ERROR.
- UPDATE: glog is deprecated. Migrate to glog fork https://github.com/AVENTER-UG/glog
            and bump it to version 0.6.1. That will fix the issue with cleanup old logfiles.
            Be sure to set `cleanup_log_files` to the amount of days you want to keep and
            `log_dir` to the mesos log directory (/var/log/mesos).

## v1.11.0-0.7.1

- FIX: Mesos Data path was set uncorrectly during building under debian 11 and 12.

## v1.11.0-0.7.0

-  ADD: Support for docker container metrics under cgroupsv2.

## v1.11.0-0.6.1

- FIX: Missing mesos-agent metrics when using cgroupsv2.

## v1.11.0-0.6.0

- UPDATE: zookeeper client to version 3.9.2.

## v1.11.0-0.5.1

- ADD: missing jquery and bootstrap files for webui (https://github.com/m3scluster/clusterd/issues/5)

## v1.11.0-0.5.0

- ADD: Support for Docker Engine >= v26
- ADD: CGroupsV2 support to the mesos containerizer.
- MERGE: Merge with Apache Mesos Upstream.

## v1.11.0-0.4.0

- Docker Containerizer now supports CGroupsV2. It will be enabled by the Agent
    flag "--enable_cgroupv2". If its enabled, currently it will break the mesos containerizer.
- Cleanup repository and remove more unneeded files.
- Update glog to version 0.6.0.
- Update libevent to version 2.1.12.
- Move mesos-mini to it's own repository (https://github.com/m3scluster/mesos-mini).
- Remove mesos-tidy and mesos-website builder.
- Remove python dependencies and move the "new" mesos-cli to it's on repository
    (https://github.com/AVENTER-UG/mesos-cli).
- Remove the old mesos-cli.
- Update zookeeper client to version 3.8.2.
- Update libboost to version 1.83.0.
- Change to C++14
- Update jquery to version 3.7.1.
- Change docker networking. If no docker network and no docker net attribute
    was given, add default networking.
- Cleanup CNI if the docker container exited due to an error.

## v1.11.0-0.3.0

- Add CNI support for the Docker Executor.
- Fix random SlaveRecoveryTest.PingTimeoutDuringRecovery test failure (Mesos upstream PR).
- Fix Blkio isolator. handled missing CFQ statistics (Mesos upstream PR).
- Fix mesos-tidy build with new gRPC version (Mesos upstream PR).
- Update grpc to version 1.11.1
- Add s390x support (Mesos upstream PR).

