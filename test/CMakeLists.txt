#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g -fno-omit-frame-pointer")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -g -fno-omit-frame-pointer")

#####################################################################################################
#######################                task_tests (with ASAN)               #########################
#####################################################################################################

SET(ENABLE_PENDING_IO_CONTRACT_TESTS ON)

add_executable(task_tests
  task_tests.cpp
  scheduler_yield_tests.cpp
)

target_include_directories(task_tests
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_definitions(task_tests
  PRIVATE
    ASAN_ENABLED=1
)

target_link_libraries(task_tests
  PRIVATE
    GTest::gtest_main
    coroutines_runtime_lib
)

include(GoogleTest)
gtest_discover_tests(task_tests PROPERTIES TIMEOUT 5)

#####################################################################################################
#######################             task_tests_no_asan (without ASAN)       #########################
#####################################################################################################

add_executable(task_tests_no_asan
  task_tests.cpp
)

target_include_directories(task_tests_no_asan
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# No ASAN_ENABLED definition - tests will see it as undefined
target_compile_options(task_tests_no_asan
  PRIVATE
    -O2
)

target_link_libraries(task_tests_no_asan
  PRIVATE
    GTest::gtest_main
    coroutines_runtime_lib
)

gtest_discover_tests(task_tests_no_asan)

# These files express target contracts. The current runtime still lacks the
# required co_await methods and the AsyncAccept() definition.
option(ENABLE_PENDING_IO_CONTRACT_TESTS "Build pending SleepFor and TCP contract tests" OFF)
if(ENABLE_PENDING_IO_CONTRACT_TESTS)
  target_sources(task_tests PRIVATE
    sleep_for_tests.cpp
    async_tcp_acceptor_tests.cpp
    async_tcp_socket_tests.cpp
  )
endif()
