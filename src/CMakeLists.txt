add_executable(test
  main.cpp
)

target_include_directories(test
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)