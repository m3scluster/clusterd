# clusterd

<a href="https://matrix.to/#/#mesos:matrix.aventer.biz" target="_new"><img src="https://img.shields.io/static/v1?label=Chat&message=Matrix&color=brightgreen"></a></span></a>
<a href="https://www.aventer.biz" target="_new"><img src="https://img.shields.io/static/v1?label=Support&message=AVENTER&color=brightgreen"></a></span></a>

## Continued development of Apache Mesos

Clusterd is a cluster manager that provides efficient resource isolation
and sharing across distributed applications, or frameworks. It can run 
workload on a dynamically shared pool of nodes.

## My promise

As long as Apache Mesos/clusterd is used, and funding/time permits, I will
continue to maintain this with security updates and features. Since there is no
tracking in clusterd or at my [repository](rpm.aventer.biz), I have no overview
of who benefits from my work outside of my client base. Therefore, I would
greatly appreciate any feedback.

## Funding

[![](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/donate/?hosted_button_id=H553XE4QJ9GJ8)

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
