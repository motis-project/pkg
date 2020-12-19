cmake_minimum_required(VERSION 3.16)
project(pkg CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Threads)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
if (MSVC)
  add_definitions(/D_WIN32_WINNT=0x0601)
  set(Boost_USE_STATIC_RUNTIME ON)
endif()
find_package(Boost COMPONENTS system filesystem REQUIRED)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

add_subdirectory(deps/utl EXCLUDE_FROM_ALL)
add_subdirectory(deps/fmt EXCLUDE_FROM_ALL)

file(GLOB_RECURSE pkg-files src/*.cc)
file(GLOB main-file src/main.cc)
list(REMOVE_ITEM pkg-files main-file)
add_library(pkglib ${pkg-files})
target_include_directories(pkglib PUBLIC include)
target_link_libraries(pkglib
  utl
  fmt
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_PROPERTYTREE_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
)
if (MSVC)
  target_link_libraries(pkglib Userenv.lib)
endif()
set_property(TARGET pkglib PROPERTY CXX_STANDARD 17)

add_executable(pkg src/main.cc)
target_link_libraries(pkg pkglib)
set_property(TARGET pkg PROPERTY CXX_STANDARD 17)

configure_file (
  ${CMAKE_CURRENT_SOURCE_DIR}/test/test_dir.h.in
  ${CMAKE_BINARY_DIR}/generated/test_dir.h
)
file(GLOB_RECURSE pkg-test-files test/*_test.cc)
add_executable(pkg-test EXCLUDE_FROM_ALL src/read_deps.cc ${pkg-test-files})
target_include_directories(pkg-test PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(pkg-test pkglib)
set_property(TARGET pkg-test PROPERTY CXX_STANDARD 17)
