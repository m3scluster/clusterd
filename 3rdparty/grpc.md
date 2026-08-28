# gRPC

## Project website

https://github.com/grpc/grpc

## How To Bundle

```sh
git clone -b v<version> https://github.com/grpc/grpc.git grpc-<version>
(cd grpc-<version> && git submodule update --init third_party/cares)
tar zcvf grpc-<version>.tar.gz --exclude .git grpc-<version>
```

## Bundled Version

We bundle 1.58.0 for OpenSSL 3 support and modern CMake builds.

The former 1.11.1-specific cherry-picks are no longer applied; their fixes are
included in the 1.58.0 release.
