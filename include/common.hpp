#pragma once

template <typename T>
class Task;
template <typename T>
class TaskPromise;

class Scheduler;

class YieldAwaiter;
template <typename Rep, typename Period>
class SleepAwaiter;