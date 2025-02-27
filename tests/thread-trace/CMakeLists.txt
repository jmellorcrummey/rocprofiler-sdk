#
#
#
cmake_minimum_required(VERSION 3.21.0 FATAL_ERROR)

if(NOT CMAKE_HIP_COMPILER)
    find_program(
        amdclangpp_EXECUTABLE
        NAMES amdclang++
        HINTS ${ROCM_PATH} ENV ROCM_PATH /opt/rocm
        PATHS ${ROCM_PATH} ENV ROCM_PATH /opt/rocm
        PATH_SUFFIXES bin llvm/bin NO_CACHE)
    mark_as_advanced(amdclangpp_EXECUTABLE)

    if(amdclangpp_EXECUTABLE)
        set(CMAKE_HIP_COMPILER "${amdclangpp_EXECUTABLE}")
    endif()
endif()

project(
    rocprofiler-tests-thread-trace
    LANGUAGES CXX HIP
    VERSION 0.0.0)

find_package(PkgConfig)

if(PkgConfig_FOUND)
    set(ENV{PKG_CONFIG_SYSTEM_INCLUDE_PATH} "")
    pkg_check_modules(DW libdw)

    if(DW_FOUND
       AND DW_INCLUDE_DIRS
       AND DW_LIBRARIES)
        set(libdw_INCLUDE_DIR
            "${DW_INCLUDE_DIRS}"
            CACHE FILEPATH "libdw include directory")
        set(libdw_LIBRARY
            "${DW_LIBRARIES}"
            CACHE FILEPATH "libdw libraries")
    endif()
endif()

if(NOT libdw_INCLUDE_DIR OR NOT libdw_LIBRARY)
    find_path(
        libdw_ROOT_DIR
        NAMES include/elfutils/libdw.h
        HINTS ${libdw_ROOT}
        PATHS ${libdw_ROOT})

    mark_as_advanced(libdw_ROOT_DIR)

    find_path(
        libdw_INCLUDE_DIR
        NAMES elfutils/libdw.h
        HINTS ${libdw_ROOT}
        PATHS ${libdw_ROOT}
        PATH_SUFFIXES include)

    find_library(
        libdw_LIBRARY
        NAMES dw
        HINTS ${libdw_ROOT}
        PATHS ${libdw_ROOT}
        PATH_SUFFIXES lib lib64)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libdw DEFAULT_MSG libdw_LIBRARY libdw_INCLUDE_DIR)

if(libdw_FOUND AND NOT TARGET libdw::libdw)
    add_library(libdw::libdw INTERFACE IMPORTED)
    if(TARGET PkgConfig::DW AND DW_FOUND)
        target_link_libraries(libdw::libdw INTERFACE PkgConfig::DW)
    else()
        target_link_libraries(libdw::libdw INTERFACE ${libdw_LIBRARY})
        target_include_directories(libdw::libdw SYSTEM INTERFACE ${libdw_INCLUDE_DIR})
    endif()
endif()

find_package(rocprofiler-sdk REQUIRED)
find_package(
    amd_comgr
    REQUIRED
    CONFIG
    HINTS
    ${rocm_version_DIR}
    ${ROCM_PATH}
    PATHS
    ${rocm_version_DIR}
    ${ROCM_PATH}
    PATH_SUFFIXES
    lib/cmake/amd_comgr)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_HIP_STANDARD 17)
set(CMAKE_HIP_EXTENSIONS OFF)
set(CMAKE_HIP_STANDARD_REQUIRED ON)

foreach(_TYPE DEBUG MINSIZEREL RELEASE RELWITHDEBINFO)
    if("${CMAKE_HIP_FLAGS_${_TYPE}}" STREQUAL "")
        set(CMAKE_HIP_FLAGS_${_TYPE} "${CMAKE_CXX_FLAGS_${_TYPE}}")
    endif()
endforeach()

set_source_files_properties(kernel_branch.cpp PROPERTIES COMPILE_FLAGS "-g -O2")
set_source_files_properties(kernel_branch.cpp PROPERTIES LANGUAGE HIP)
set_source_files_properties(kernel_lds.cpp PROPERTIES COMPILE_FLAGS "-g -O2")
set_source_files_properties(kernel_lds.cpp PROPERTIES LANGUAGE HIP)
set_source_files_properties(main.cpp PROPERTIES LANGUAGE HIP)

# Single dispatch test
add_executable(thread-trace-api-single-test)
target_sources(
    thread-trace-api-single-test PRIVATE main.cpp trace_callbacks.cpp single_dispatch.cpp
                                         kernel_branch.cpp kernel_lds.cpp)
target_link_libraries(thread-trace-api-single-test
                      PRIVATE rocprofiler-sdk::rocprofiler-sdk amd_comgr dw)

if(ROCPROFILER_MEMCHECK_PRELOAD_ENV)
    set(PRELOAD_ENV
        "${ROCPROFILER_MEMCHECK_PRELOAD_ENV}:$<TARGET_FILE:thread-trace-api-single-test>")
else()
    set(PRELOAD_ENV "LD_PRELOAD=$<TARGET_FILE:thread-trace-api-single-test>")
endif()

add_test(NAME thread-trace-api-single-test
         COMMAND $<TARGET_FILE:thread-trace-api-single-test>)

set_tests_properties(
    thread-trace-api-single-test
    PROPERTIES TIMEOUT 10 LABELS "integration-tests" ENVIRONMENT "${PRELOAD_ENV}"
               FAIL_REGULAR_EXPRESSION "${ROCPROFILER_DEFAULT_FAIL_REGEX}")

# Multi dispatch test
add_executable(thread-trace-api-multi-test)
target_sources(
    thread-trace-api-multi-test PRIVATE main.cpp trace_callbacks.cpp multi_dispatch.cpp
                                        kernel_branch.cpp kernel_lds.cpp)
target_link_libraries(thread-trace-api-multi-test PRIVATE rocprofiler-sdk::rocprofiler-sdk
                                                          amd_comgr dw)

if(ROCPROFILER_MEMCHECK_PRELOAD_ENV)
    set(PRELOAD_ENV
        "${ROCPROFILER_MEMCHECK_PRELOAD_ENV}:$<TARGET_FILE:thread-trace-api-multi-test>")
else()
    set(PRELOAD_ENV "LD_PRELOAD=$<TARGET_FILE:thread-trace-api-multi-test>")
endif()

add_test(NAME thread-trace-api-multi-test
         COMMAND $<TARGET_FILE:thread-trace-api-multi-test>)

set_tests_properties(
    thread-trace-api-multi-test
    PROPERTIES TIMEOUT 10 LABELS "integration-tests" ENVIRONMENT "${PRELOAD_ENV}"
               FAIL_REGULAR_EXPRESSION "${ROCPROFILER_DEFAULT_FAIL_REGEX}")
