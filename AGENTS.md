# Agent instructions for coroutines_runtime

This is a learning project for building a small C++ coroutine runtime. The user is using the project to understand coroutine architecture, ownership, scheduling, and async I/O contracts.

## Working style

- Prefer small, focused changes.
- Explain non-trivial architectural choices before implementing them.
- Do not replace the design with a large ready-made solution.
- Do not fix or rewrite production runtime code unless the user explicitly asks for implementation changes.
- If a task asks only for tests or docs, do not change production code.
- If implementation and docs disagree, report the mismatch instead of silently changing the contract or runtime.

## Required reading

Before changing runtime code or tests, read:

- `docs/runtime_contract.md`
- `docs/user_api.md`
- `docs/testing.md`

Treat `docs/runtime_contract.md` and `docs/user_api.md` as the source of truth for intended behavior. Do not change documented contracts unless the user explicitly asks for contract/design changes.

## Tests

- Tests should verify documented guarantees.
- Prefer one documented guarantee per test.
- Keep tests grouped by contract area:
  - Task ownership and result propagation
  - Scheduler and Yield behavior
  - SleepFor behavior
  - AsyncTCPAcceptor behavior
  - AsyncTCPSocket read/write behavior
- It is acceptable for newly added tests to fail if they correctly express the documented target contract and the implementation has not caught up yet.
- When tests fail because implementation differs from the contract, stop and explain the gap unless the user explicitly asked to fix runtime code.

## Build and test harness

Use the CMake presets when possible:

- Configure: `cmake --preset default`
- Build: `cmake --build --preset default`
- Run tests: `ctest --preset default`

For targeted test debugging, build first and then run the relevant executable or a filtered `ctest` invocation from the build directory.

## Constraints

- Do not use Boost.Asio coroutine APIs such as `asio::awaitable`, `co_spawn`, or `use_awaitable`.
- Boost.Asio is only the async I/O backend.
- Coroutine callbacks from timers and sockets should schedule handles back into `Scheduler`; they should not directly resume user coroutines.
