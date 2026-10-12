add_library(coroutines_runtime_lib STATIC
  Scheduler.cpp
)

target_include_directories(coroutines_runtime_lib
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(coroutines_runtime_lib
  PUBLIC
    asio_lib
    Threads::Threads
)

add_executable(task_demo
  main.cpp
)

target_link_libraries(task_demo
  PRIVATE
    coroutines_runtime_lib
)