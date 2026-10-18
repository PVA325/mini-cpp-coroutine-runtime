# MiniCoroutineRuntime

A small, single-threaded C++23 coroutine runtime with an asynchronous TCP echo server. It implements its own `Task<T>`, scheduler, yield and sleep awaiters, and TCP accept/read/write awaiters. Standalone Asio is used only as the I/O backend; the runtime does not use Asio's coroutine APIs.

## Build and test

Requires CMake 3.28+, a C++23 compiler, and Git. CMake downloads standalone Asio 1.30.2 and GoogleTest 1.15.2 on the first configure.

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## Echo server

Run `./build/default/src/task_demo`. It listens on `127.0.0.1:5555`; for example, connect with `nc 127.0.0.1 5555` and type a message to receive it back.

For the runtime's ownership and API contracts, see [runtime_contract.md](docs/runtime_contract.md) and [user_api.md](docs/user_api.md).

## Third-party license

[Standalone Asio](https://think-async.com/Asio/License) is distributed under the Boost Software License 1.0. Its license text is included in the Asio source fetched by CMake.
