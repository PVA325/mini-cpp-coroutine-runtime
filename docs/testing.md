# Testing and harness

This project uses CMake and GoogleTest.

## Standard commands

From the repository root:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

The default preset writes build files to `build/default`, which is ignored by git.

## Existing test targets

- `task_tests` builds the normal GoogleTest executable with `ASAN_ENABLED=1` defined for compile-time test selection.
- `task_tests_no_asan` builds the same test file with `-O2` and without `ASAN_ENABLED`, so deep symmetric-transfer tests that rely on optimization can run separately.

## Suggested test organization

Keep tests grouped by contract area. This helps both humans and agents see what guarantee a test protects.

### Task ownership tests

Use these for `Task<T>`, `Task<void>`, promise behavior, result propagation, exception propagation, move-only ownership, and `co_await` ownership transfer.

Examples of documented guarantees to test:

- manual `Task` remains owner until destruction;
- moved-from `Task` does not destroy transferred handle;
- awaited child resumes parent through continuation;
- returned values and exceptions are observed through `await_resume()` or `GetValue()`.

### Scheduler and Yield tests

Use these for ready queue behavior, `Spawn`, `Run`, `Stop`, round-robin-style interleaving through `Yield`, and deferred destroy of completed top-level tasks.

Examples of documented guarantees to test:

- `Yield()` returns the current coroutine to the ready queue;
- scheduler resumes ready handles centrally;
- callbacks or awaiters schedule handles instead of directly resuming user coroutine;
- `Stop()` behavior is either explicitly guaranteed or documented as a limitation.

### SleepFor tests

Use these for timer scheduling and scheduler integration with `io_context`.

Examples of documented guarantees to test:

- `SleepFor(duration)` does not resume before timer completion;
- sleeping coroutine resumes after the timer fires;
- scheduler does not exit early while only timer work is pending;
- `SleepFor()` outside active scheduler has explicit failure behavior.

### AsyncTCPAcceptor tests

Use these for `AsyncTCPAcceptor`, blocking `Accept()`, coroutine-friendly `AsyncAccept()`, and accepted socket ownership.

Examples of documented guarantees to test:

- `AsyncAccept()` suspends until accept completion;
- `AsyncAccept()` does not block the scheduler loop;
- accepted `AsyncTCPSocket` does not depend on acceptor lifetime after successful accept;
- accept errors are reported through the documented result/exception behavior.

### AsyncTCPSocket read/write tests

Use these for `AsyncReadSome`, `AsyncWriteSome`, result/error propagation, and buffer lifetime assumptions.

Examples of documented guarantees to test:

- read coroutine resumes only after read completion;
- write coroutine resumes only after write completion;
- read/write callbacks schedule the coroutine through `Scheduler`;
- `AsyncReadSome` and `AsyncWriteSome` may complete partially and report byte counts;
- socket and buffer must remain alive until operation completion.

## Expected workflow for agents

When asked to add tests:

1. Read `docs/runtime_contract.md`, `docs/user_api.md`, and this file.
2. Identify the exact documented guarantee being tested.
3. Add the smallest focused test or tests for that guarantee.
4. Run the relevant build/test command if dependencies are available.
5. If tests fail because implementation does not match the documented contract, report the mismatch and do not rewrite runtime code unless explicitly asked.
