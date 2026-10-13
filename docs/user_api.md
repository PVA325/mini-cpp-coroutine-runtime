# User-space API

Этот документ описывает пользовательский слой runtime: какие классы доступны пользователю, какие методы они дают и что эти методы обещают. Детальные ownership/lifetime контракты описаны в `docs/runtime_contract.md`.

Документ описывает целевую форму API. Если текущая реализация еще отличается от нее, тесты должны помогать привести код к этому контракту.

## `Task<T>`

`Task<T>` представляет coroutine, которая со временем возвращает `T`.

Пользовательские операции:

- создать `Task<T>` вызовом coroutine-функции;
- вручную продолжить через `Resume()`;
- проверить завершение через `IsDone()`;
- получить результат через `GetValue()` после завершения;
- await'ить task через `co_await` на temporary task или `std::move(task)`.

Ограничения:

- `Task<T>` move-only;
- после передачи владения через `co_await` или `Scheduler::Spawn()` task считается пустым;
- методы, работающие с handle, нельзя вызывать на пустом task.

## `Scheduler`

`Scheduler` управляет ready queue coroutine handles и общим `io_context` для timers и TCP operations.

Пользовательские операции:

- `Spawn(Task<void>&&)` добавляет top-level coroutine в scheduler;
- `Run()` исполняет ready coroutine и I/O callbacks до завершения работы или остановки;
- `Stop()` просит scheduler остановиться.

Гарантии:

- spawned task передает ownership coroutine handle scheduler'у;
- coroutine callbacks из timers/socket operations должны возвращать handles в ready queue, а не resume'ить их напрямую;
- scheduler должен жить дольше async resources, созданных на его `io_context`.

Ограничения:

- `Scheduler` не является thread-safe;
- shutdown/cancellation pending I/O пока не является полноценной гарантией;
- после `Stop()` может потребоваться дополнительный `Run()` или отдельная cleanup-модель, если есть pending operations.

## `Yield()`

`Yield()` добровольно отдает управление scheduler'у.

Использование:

- вызывается внутри coroutine как `co_await Yield()`;
- текущая coroutine suspend'ится;
- handle возвращается в ready queue scheduler'а;
- coroutine может продолжиться позже, когда scheduler снова выберет ее из очереди.

Предусловие: `Yield()` вызывается только внутри активного `Scheduler::Run()`.

## `SleepFor(duration)`

`SleepFor(duration)` откладывает продолжение coroutine минимум на указанное время.

Использование:

- вызывается внутри coroutine как `co_await SleepFor(duration)`;
- регистрирует timer на scheduler `io_context`;
- после timer completion coroutine возвращается в ready queue scheduler'а.

Гарантии:

- coroutine не продолжится раньше completion timer'а;
- coroutine может продолжиться позже указанного времени, если scheduler занят другой работой;
- `SleepFor` не передает ownership coroutine frame.

Предусловие: вызов происходит внутри active scheduler context.

## `AsyncTCPAcceptor`

`AsyncTCPAcceptor` представляет TCP acceptor, привязанный к `Scheduler` и его `io_context`.

Пользовательские операции:

- создать acceptor для endpoint;
- await'ить `AsyncAccept()` для получения нового `AsyncTCPSocket`;
- вызвать `Accept()` для blocking accept, если блокировка текущего потока допустима.

Семантика `AsyncAccept()`:

- `co_await acceptor.AsyncAccept()` suspend'ит coroutine до прихода нового connection или ошибки;
- после success возвращается новый `AsyncTCPSocket`;
- accepted socket не зависит от lifetime acceptor'а;
- acceptor должен жить до завершения pending accept operation;
- callback accept operation только возвращает coroutine handle в scheduler ready queue, но не resume'ит coroutine напрямую.

Семантика `Accept()`:

- выполняет blocking accept;
- сразу возвращает `AsyncTCPSocket` после принятия connection;
- блокирует поток, в котором вызван;
- не является основным coroutine API для server loop.

Ограничения:

- acceptor move-only;
- move допустим только когда нет pending accept operation;
- multiple concurrent accepts на одном acceptor не гарантируются, пока это явно не реализовано.

## `AsyncTCPSocket`

`AsyncTCPSocket` представляет TCP socket, привязанный к `Scheduler` и его `io_context`.

Пользовательские операции:

- `AsyncReadSome(buffer)` запускает one-shot read operation;
- `AsyncWriteSome(buffer)` запускает one-shot write operation;
- доступ к raw socket допустим только как низкоуровневый escape hatch, если он остается в рамках scheduler `io_context`.

`AsyncReadSome(buffer)`:

- suspend'ит coroutine до completion read operation;
- возвращает количество прочитанных байт или сообщает об ошибке;
- не гарантирует чтение полного logical message;
- buffer должен жить до completion операции.

`AsyncWriteSome(buffer)`:

- suspend'ит coroutine до completion write operation;
- возвращает количество записанных байт или сообщает об ошибке;
- не гарантирует запись всего buffer целиком;
- buffer должен жить до completion операции.

Ограничения:

- socket move-only;
- move допустим только когда нет pending read/write operation;
- multiple concurrent reads на одном socket не гарантируются;
- multiple concurrent writes на одном socket не гарантируются;
- socket должен жить до завершения pending операций на нем.

## Recommended echo-server shape

Пользовательский echo server должен мыслиться так:

- top-level server coroutine создает `AsyncTCPAcceptor`;
- server loop await'ит `AsyncAccept()`;
- для каждого accepted socket запускается `ClientSession` как `Task<void>`;
- `ClientSession` хранит socket и buffer как локальные переменные coroutine;
- session loop await'ит `AsyncReadSome`, затем `AsyncWriteSome` или будущий `WriteAll` helper.

Такой стиль делает socket и buffer частью coroutine frame, поэтому они живут между suspend/resume.
