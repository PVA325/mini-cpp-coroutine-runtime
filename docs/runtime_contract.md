# Runtime contract

Этот документ описывает не пользовательский API, а контракты владения и lifetime-гарантии текущего mini-runtime для coroutine.

Документ составлен на момент коммита:
2a89eab (HEAD -> dev) feat(docs): added contract

Документ фиксирует:

- кто владеет `std::coroutine_handle`;
- кто обязан вызвать `destroy()`;
- когда coroutine стартует, suspend'ится и resume'ится;
- как `Task`, awaiter'ы и `Scheduler` передают управление друг другу.

Документ также фиксирует целевые контракты для `SleepFor`, `AsyncTCPSocket` и `AsyncTCPAcceptor`. Если текущая реализация еще не полностью следует этим контрактам, это считается задачей для тестов и дальнейшей доработки runtime.

## Core model

Coroutine frame создается C++ runtime'ом при вызове coroutine-функции, которая возвращает `Task<T>`.

`Task<T>` получает `std::coroutine_handle<TaskPromise<T>>` через `promise_type::get_return_object()` и изначально является владельцем этого handle.

Главное правило runtime: валидный coroutine handle должен иметь ровно одного владельца. Владелец валидного handle отвечает за вызов `destroy()`.

Если владение handle передается другому объекту, предыдущий владелец обязан стать пустым и больше не уничтожать этот handle.

## Component contracts

### `Task<T>`

`Task<T>` - move-only ручка для управления coroutine.

Контракт:

- хранит `handle_` на coroutine frame;
- запрещает copy, чтобы не было двух владельцев одного handle;
- поддерживает move, после move source-task становится пустым;
- в деструкторе вызывает `handle_.destroy()`, если `handle_` валиден;
- в move-assignment сначала уничтожает текущий owned handle, если он есть, затем забирает handle у другого `Task`;
- `Resume()`, `IsDone()` и `GetValue()` имеют смысл только пока `Task` владеет валидным handle;
- `operator co_await() &&` забирает `handle_` через `std::exchange(handle_, nullptr)` и передает владение в `TaskAwaiter`.

После `co_await std::move(task)` или `co_await Foo()` объект `Task`, из которого забрали handle, больше не ссылается на coroutine frame.

### `TaskAwaiterBase<T>` and `TaskAwaiter`

`TaskAwaiter` владеет handle дочерней coroutine, которую await'ит родительская coroutine.

Контракт:

- получает handle от rvalue `Task<T>`;
- в `await_suspend(outer_coro)` записывает `outer_coro` в `handle_.promise().continuation_`;
- возвращает handle дочерней coroutine из `await_suspend`, тем самым делает symmetric transfer;
- в деструкторе вызывает `handle_.destroy()`, если handle валиден;
- `await_resume()` возвращает значение из promise или обрабатывает сохраненное исключение. Для `Task<T>` используется `std::rethrow_exception`; для `Task<void>` текущая реализация бросает сохраненный `std::exception_ptr` как объект.

Важное следствие: frame дочерней coroutine живет до уничтожения awaiter'а. Родительская coroutine не владеет child handle напрямую; этим занимается `TaskAwaiter`.

### `TaskPromise<T>`

`TaskPromise<T>` хранит состояние coroutine frame: результат, исключение и continuation.

Контракт:

- `initial_suspend()` всегда возвращает `std::suspend_always`, поэтому coroutine не стартует сама при создании `Task`;
- `return_value()` сохраняет результат для `Task<T>`;
- `return_void()` завершает `Task<void>`;
- `unhandled_exception()` сохраняет исключение;
- `GetReturnedValue()` возвращает результат или rethrow'ит сохраненное исключение;
- `final_suspend()` возвращает специальный awaiter, который решает, куда передать управление после завершения coroutine.

Если coroutine была awaited, в `continuation_` лежит handle родительской coroutine. Тогда `final_suspend()` возвращает continuation, и выполнение симметрично возвращается в родителя.

Если continuation нет, но coroutine выполняется внутри `Scheduler`, `final_suspend()` передает completed handle в очередь deferred destroy через `Scheduler::ScheduleDestroing()` и возвращает `std::noop_coroutine()`.

### `Scheduler`

`Scheduler` исполняет top-level `Task<void>` и ready coroutine handles.

Контракт:

- `Spawn(Task<void>&& task)` забирает handle у `Task<void>` и кладет его в ready queue;
- после `Spawn` исходный `Task<void>` становится пустым и больше не уничтожает handle;
- `Run()` устанавливает current scheduler, берет handles из ready queue и вызывает `resume()`;
- после каждого `resume()` вызывает `DestroyHandles()` для handles, которые завершились и были отложены на destroy;
- `Schedule(handle)` не забирает ownership, а только возвращает suspended coroutine в ready queue;
- `ScheduleDestroing(handle)` откладывает уничтожение completed coroutine frame до безопасной точки в scheduler loop.

`Scheduler` владеет spawned top-level handles после `Spawn()`. Для handles, которые попали в `Schedule()` через `Yield()`, ownership не меняется: это тот же coroutine frame, который временно отложили для продолжения.

### `YieldAwaiter`

`YieldAwaiter` реализует добровольную передачу управления планировщику.

Контракт:

- `await_ready()` всегда возвращает `false`;
- `await_suspend(outer)` кладет текущую coroutine обратно в scheduler ready queue;
- `await_resume()` ничего не возвращает;
- ownership handle не меняется.

Предусловие: `co_await Yield()` вызывается только внутри активного `Scheduler::Run()`, когда `Scheduler::GetCurrentScheduler()` возвращает валидный scheduler.

### `SleepAwaiter` / `SleepFor`

`SleepFor(duration)` suspend'ит текущую coroutine до истечения указанного времени.

Контракт:

- `SleepFor(duration)` возвращает awaiter для `co_await`;
- `await_ready()` всегда возвращает `false`;
- `await_suspend(outer)` регистрирует timer в `io_context`, которым владеет текущий `Scheduler`;
- callback timer'а не вызывает `outer.resume()` напрямую, а только кладет `outer` обратно в ready queue scheduler'а;
- `await_resume()` ничего не возвращает;
- `SleepFor(duration)` гарантирует, что coroutine не будет продолжена раньше, чем timer completion будет получен runtime'ом;
- точное время resume не гарантируется: coroutine может продолжиться позже из-за очереди scheduler'а, других ready coroutine или задержек `io_context`.

Предусловие: `SleepFor()` вызывается только внутри coroutine, которая исполняется текущим `Scheduler::Run()`.

Lifetime-контракт: timer operation должна жить до completion. Если timer хранится внутри awaiter'а, awaiter обязан пережить suspend. Если timer state вынесен отдельно, этот state должен жить до callback'а.

### `AsyncTCPSocket`

`AsyncTCPSocket` - move-only runtime wrapper над `asio::ip::tcp::socket`.

Контракт:

- владеет `tcp::socket`;
- связан с конкретным `Scheduler`, потому что socket создан на `io_context` этого scheduler'а;
- не копируется;
- может перемещаться только когда на нем нет pending read/write operations;
- destructor закрывает socket или освобождает его обычным RAII-путем;
- все async read/write операции возвращают suspended coroutine в ready queue того же scheduler'а;
- callbacks read/write не resume'ят coroutine напрямую и не уничтожают coroutine frame.

`AsyncReadSome(buffer)`:

- регистрирует one-shot async read operation на socket;
- coroutine продолжится только после completion этой read operation;
- `await_resume()` возвращает количество прочитанных байт или сообщает об ошибке;
- операция может завершиться с `0` bytes или error, если peer закрыл соединение или backend сообщил ошибку;
- buffer должен жить до completion операции.

`AsyncWriteSome(buffer)`:

- регистрирует one-shot async write operation на socket;
- coroutine продолжится только после completion этой write operation;
- `await_resume()` возвращает количество записанных байт или сообщает об ошибке;
- `AsyncWriteSome` не обязана записывать весь buffer целиком;
- buffer должен жить до completion операции.

Runtime не гарантирует несколько одновременных read operations на одном `AsyncTCPSocket` и несколько одновременных write operations на одном `AsyncTCPSocket`, пока это явно не реализовано и не покрыто тестами.

### `AsyncTCPAcceptor`

`AsyncTCPAcceptor` - move-only runtime wrapper над `asio::ip::tcp::acceptor`.

Контракт:

- создается внутри активного scheduler context или явно получает scheduler при создании;
- владеет `tcp::acceptor`, привязанным к `io_context` этого scheduler'а;
- не копируется;
- может перемещаться только когда на нем нет pending accept operation;
- destructor закрывает acceptor или освобождает его обычным RAII-путем;
- async accept operation возвращает suspended coroutine в ready queue scheduler'а;
- accepted `AsyncTCPSocket` после successful accept не зависит от lifetime `AsyncTCPAcceptor`, но продолжает зависеть от lifetime `Scheduler`.

`Accept()`:

- выполняет blocking accept и сразу возвращает `AsyncTCPSocket`;
- не suspend'ит coroutine и не должен использоваться как основной coroutine API;
- подходит только как низкоуровневый helper/debug path, если caller явно готов блокировать текущий поток scheduler'а.

`AsyncAccept()`:

- целевая coroutine-модель: `co_await acceptor.AsyncAccept()` suspend'ит coroutine до completion accept operation;
- `await_resume()` возвращает `AsyncTCPSocket` или сообщает об ошибке;
- callback accept operation не resume'ит coroutine напрямую;
- `AsyncTCPAcceptor` должен жить до completion pending accept operation.

## Ownership flows

### Manual task

```cpp
Task<int> task = Foo();
task.Resume();
```

Поток владения:

1. `Foo()` создает coroutine frame.
2. `Task<int>` получает handle и владеет им.
3. `task.Resume()` запускает или продолжает coroutine.
4. `Task<int>` остается владельцем handle.
5. `Task<int>::~Task()` вызывает `destroy()` для coroutine frame.

### Awaited child task

```cpp
Task<int> Parent() {
  int value = co_await Foo();
  co_return value;
}
```

Поток владения:

1. `Foo()` возвращает temporary `Task<int>`.
2. `Task<int>::operator co_await() &&` передает handle в `TaskAwaiter`.
3. Temporary `Task<int>` становится пустым.
4. `TaskAwaiter::await_suspend()` сохраняет continuation родителя.
5. Runtime делает symmetric transfer в child coroutine.
6. Child coroutine завершается и в `final_suspend()` возвращает continuation родителя.
7. `TaskAwaiter::await_resume()` возвращает результат или обрабатывает сохраненное исключение.
8. `TaskAwaiter::~TaskAwaiter()` уничтожает child coroutine frame.

### Spawned scheduler task

```cpp
Scheduler scheduler;
scheduler.Spawn(Foo());
scheduler.Run();
```

Поток владения:

1. `Foo()` возвращает `Task<void>`.
2. `Scheduler::Spawn()` забирает handle у `Task<void>`.
3. `Task<void>` становится пустым.
4. `Scheduler::Run()` resume'ит top-level coroutine.
5. У top-level coroutine нет continuation.
6. В `final_suspend()` completed handle попадает в scheduler destroy queue.
7. `Scheduler::DestroyHandles()` вызывает `destroy()`.

### Yield

```cpp
co_await Yield();
```

Поток управления:

1. Coroutine доходит до `YieldAwaiter`.
2. `YieldAwaiter::await_suspend()` получает handle текущей coroutine.
3. Handle ставится обратно в scheduler ready queue.
4. Ownership не меняется.
5. Scheduler позже снова вызывает `resume()` для того же handle.

### Sleep

```cpp
co_await SleepFor(duration);
```

Поток управления:

1. Coroutine доходит до `SleepAwaiter`.
2. `SleepAwaiter::await_suspend()` получает handle текущей coroutine.
3. Awaiter регистрирует timer в scheduler `io_context`.
4. Coroutine остается suspended, handle не возвращается в ready queue сразу.
5. После timer completion callback кладет handle в scheduler ready queue.
6. Scheduler позже вызывает `resume()` для этого handle.
7. Ownership coroutine frame не меняется.

### TCP read/write

```cpp
auto bytes = co_await socket.AsyncReadSome(buffer);
auto written = co_await socket.AsyncWriteSome(buffer);
```

Поток управления:

1. Coroutine доходит до socket awaiter.
2. Awaiter ссылается на `AsyncTCPSocket`, buffer и scheduler.
3. Awaiter регистрирует read/write operation в socket backend.
4. Coroutine остается suspended до completion операции.
5. Callback сохраняет результат операции и кладет coroutine handle в scheduler ready queue.
6. Scheduler позже resume'ит coroutine.
7. `await_resume()` возвращает результат или сообщает об ошибке.

Awaiter не владеет socket и buffer. Он предполагает, что referenced objects живут до `await_resume()`.

### TCP accept

```cpp
AsyncTCPSocket socket = co_await acceptor.AsyncAccept();
```

Поток управления:

1. Coroutine доходит до accept awaiter.
2. Awaiter регистрирует accept operation на `AsyncTCPAcceptor`.
3. Coroutine остается suspended до completion accept operation.
4. Callback сохраняет success/error и кладет coroutine handle в scheduler ready queue.
5. Scheduler позже resume'ит coroutine.
6. `await_resume()` возвращает новый `AsyncTCPSocket` или сообщает об ошибке.
7. Новый `AsyncTCPSocket` больше не зависит от `AsyncTCPAcceptor`, но зависит от scheduler/io_context.

## Guarantees

Текущая реализация гарантирует:

- coroutine handle не копируется как ownership;
- moved-from `Task` не уничтожает переданный handle;
- `Task` и `TaskAwaiterBase` вызывают `destroy()`, если владеют валидным handle;
- awaited child coroutine возвращает управление родителю через `continuation_`;
- awaited child coroutine уничтожается awaiter'ом;
- scheduler-spawned top-level coroutine уничтожается scheduler'ом после completion;
- `Yield()` возвращает suspended coroutine в ready queue без передачи ownership;
- `SleepFor()` не продолжает coroutine раньше timer completion;
- socket read/write awaiter'ы не продолжают coroutine раньше completion соответствующей I/O operation;
- accepted socket после successful accept не зависит от lifetime acceptor'а.

## Preconditions

Код, использующий runtime, должен соблюдать следующие условия:

- `Yield()` вызывается только внутри активного `Scheduler::Run()`;
- `Task<T>::operator co_await()` используется только на rvalue task: temporary task или `std::move(task)`;
- `Resume()`, `IsDone()`, `GetValue()` и `Promise()` вызываются только на `Task`, который все еще владеет валидным handle;
- after ownership transfer объект `Task` считается пустым и не должен использоваться как handle owner;
- `Scheduler::Spawn()` сейчас принимает только `Task<void>` как top-level scheduler task;
- `SleepFor()` требует current scheduler;
- `AsyncTCPAcceptor` и `AsyncTCPSocket` требуют, чтобы связанный `Scheduler` жил дольше них;
- `AsyncTCPAcceptor` должен жить до completion pending accept operation;
- `AsyncTCPSocket` должен жить до completion pending read/write operation;
- buffers, переданные в read/write operation, должны жить до completion операции;
- socket/acceptor move допустим только когда нет pending operation на этом объекте.

## Non-guarantees and current limitations

Текущая реализация не гарантирует:

- thread-safety;
- cancellation;
- cleanup всех queued handles после `Stop()`, если scheduler остановлен до полного опустошения очереди;
- корректное поведение `Yield()` вне scheduler;
- cleanup pending timers/socket operations при shutdown без отдельной cancellation-модели;
- несколько одновременных read operations на одном socket;
- несколько одновременных write operations на одном socket;
- thread-safe доступ к scheduler/socket/acceptor;
- стабильный public ABI.

Эти пункты нужно либо явно реализовать, либо оставить как documented limitations.

## Tests as contract anchors

Существующие тесты уже проверяют часть контрактов:

- `TaskTest.SimpleResumeAndValue` - manual lifecycle `Task`;
- `TaskTest.SequentialAwaitOfTemporaryTasks` - await temporary task и ownership transfer в awaiter;
- `TaskTest.DeepRecursiveAwaitDoesNotOverflowStack` - symmetric transfer на глубокой цепочке await;
- `TaskVoidTest.*` - completion для `Task<void>`;
- `SchedulerTest.*` - ready queue, `Yield()`, round-robin и `Stop()`;
- будущие `SleepFor` tests - no early resume, resume after timer completion, scheduler waits for timer when ready queue is empty;
- будущие TCP tests - accept returns independent socket, read/write resume only after operation completion, buffer lifetime assumptions are explicit.

Будущие тесты, которые стоит добавить при закреплении контрактов:

- moved-from `Task` после `co_await` или `Spawn()` пустой и не уничтожает transferred handle;
- `Scheduler::Stop()` явно очищает или документированно не очищает queued handles;
- `Yield()` вне scheduler либо запрещен проверкой, либо имеет явное failure behavior;
- `SleepFor()` вне scheduler имеет явное failure behavior;
- `AsyncTCPSocket` read/write errors propagate through documented result/exception behavior;
- `AsyncTCPAcceptor::AsyncAccept()` is asynchronous and does not block scheduler loop;
- `AsyncTCPAcceptor::Accept()` is blocking and should not be used in scheduler tasks unless blocking is intentional.
