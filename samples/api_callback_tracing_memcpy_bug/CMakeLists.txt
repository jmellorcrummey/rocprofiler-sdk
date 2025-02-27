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

project(rocprofiler-sdk-samples-callback-api-tracing-memcpy-bug LANGUAGES CXX HIP)

foreach(_TYPE DEBUG MINSIZEREL RELEASE RELWITHDEBINFO)
    if("${CMAKE_HIP_FLAGS_${_TYPE}}" STREQUAL "")
        set(CMAKE_HIP_FLAGS_${_TYPE} "${CMAKE_CXX_FLAGS_${_TYPE}}")
    endif()
endforeach()

find_package(rocprofiler-sdk REQUIRED)

add_library(callback-api-tracing-client-memcpy-bug SHARED)
target_sources(callback-api-tracing-client-memcpy-bug PRIVATE client.cpp client.hpp)
target_link_libraries(
    callback-api-tracing-client-memcpy-bug
    PRIVATE rocprofiler-sdk::rocprofiler-sdk rocprofiler-sdk::samples-build-flags
            rocprofiler-sdk::samples-common-library)

set_source_files_properties(main.cpp PROPERTIES LANGUAGE HIP)

find_package(Threads REQUIRED)
find_package(rocprofiler-sdk-roctx REQUIRED)

add_executable(callback-api-tracing-memcpy-bug)
target_sources(callback-api-tracing-memcpy-bug PRIVATE main.cpp)
target_link_libraries(
    callback-api-tracing-memcpy-bug
    PRIVATE callback-api-tracing-client-memcpy-bug Threads::Threads
            rocprofiler-sdk-roctx::rocprofiler-sdk-roctx
            rocprofiler-sdk::samples-build-flags)

rocprofiler_samples_get_preload_env(PRELOAD_ENV callback-api-tracing-client-memcpy-bug)
rocprofiler_samples_get_ld_library_path_env(
    LIBRARY_PATH_ENV rocprofiler-sdk-roctx::rocprofiler-sdk-roctx-shared-library)

set(callback-api-tracing-memcpy-bug-env ${PRELOAD_ENV} ${LIBRARY_PATH_ENV})

add_test(NAME callback-api-tracing-memcpy-bug COMMAND $<TARGET_FILE:callback-api-tracing-memcpy-bug>)

set_tests_properties(
    callback-api-tracing-memcpy-bug
    PROPERTIES TIMEOUT 45 LABELS "samples" ENVIRONMENT "${callback-api-tracing-memcpy-bug-env}"
               FAIL_REGULAR_EXPRESSION "${ROCPROFILER_DEFAULT_FAIL_REGEX}")
