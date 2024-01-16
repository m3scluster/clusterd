# clusterd 
## based on Apache Mesos

Clusterd is a cluster manager that provides efficient resource isolation
and sharing across distributed applications, or frameworks. It can run 
workload on a dynamically shared pool of nodes.

# Documentation

Documentation is available in the [docs](https://github.com/m3scluster/clusterd-docs) repository. 

# Installation

Instructions are included on the [Getting Started](http://mesos.apache.org/getting-started/) page.

# License

Clusterd is licensed under the [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0).

For additional information, see the LICENSE and NOTICE files.

# Building

To Compile Clusterd please use `cmake` and not the deprecated `configure`.

## Compile with SSL support

```bash
mkdir build
cd build
cmake -DUNBUNDLED_LIBEVENT=true -DENABLE_SSL=true -DENABLE_LIBEVENT=true ../
make
```

## Compile and build debian package

```bash
mkdir build
cd build
cmake -DUNBUNDLED_LIBEVENT=true -DENABLE_SSL=true -DENABLE_LIBEVENT=true -DCMAKE_BUILD_TYPE=Release -DCPACK_BINARY_DEB=true ../
make
make package
```

