cmake_minimum_required(VERSION 3.16)
project(pkg CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Threads)

add_subdirectory(deps/utl EXCLUDE_FROM_ALL)
add_subdirectory(deps/fmt EXCLUDE_FROM_ALL)
add_subdirectory(deps/zlib EXCLUDE_FROM_ALL)
add_subdirectory(deps/boost EXCLUDE_FROM_ALL)

file(GLOB_RECURSE pkg-files src/*.cc)
file(GLOB main-file src/main.cc)
list(REMOVE_ITEM pkg-files main-file)
add_library(pkglib ${pkg-files})
target_include_directories(pkglib PUBLIC include)
target_link_libraries(pkglib
        utl
        fmt
        boost
        boost-filesystem
        boost-system
        ${CMAKE_THREAD_LIBS_INIT}
        )
if (MSVC)
    target_link_libraries(pkglib Userenv.lib)
endif ()
target_compile_features(pkglib PUBLIC cxx_std_17)

add_executable(pkg src/main.cc)
target_link_libraries(pkg pkglib)
target_compile_features(pkg PRIVATE cxx_std_17)
if (${CMAKE_BUILD_TYPE} STREQUAL Release)
    add_custom_command(
            TARGET pkg
            POST_BUILD
            COMMAND ${CMAKE_STRIP} ${CMAKE_CURRENT_BINARY_DIR}/pkg
    )
endif ()

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/test/test_dir.h.in
        ${CMAKE_BINARY_DIR}/generated/test_dir.h
)
file(GLOB_RECURSE pkg-test-files test/*_test.cc)
add_executable(pkg-test EXCLUDE_FROM_ALL src/read_deps.cc ${pkg-test-files})
target_include_directories(pkg-test PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(pkg-test pkglib)
set_property(TARGET pkg-test PROPERTY CXX_STANDARD 17)
