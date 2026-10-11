add_executable(task_tests
  task_tests.cpp
)

target_include_directories(task_tests
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(task_tests
  PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(task_tests)
