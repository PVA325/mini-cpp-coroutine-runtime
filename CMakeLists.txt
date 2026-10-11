cmake_minimum_required(VERSION 3.28)
project(coroutines_runtime)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)

FetchContent_Declare(
  asio_standalone
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-30-2
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(asio_standalone)
add_library(asio_lib INTERFACE)

target_include_directories(asio_lib INTERFACE ${asio_standalone_SOURCE_DIR}/asio/include)
target_compile_definitions(asio_lib INTERFACE ASIO_STANDALONE)
find_package(Threads REQUIRED)
target_link_libraries(asio_lib INTERFACE Threads::Threads)

add_subdirectory(src)
