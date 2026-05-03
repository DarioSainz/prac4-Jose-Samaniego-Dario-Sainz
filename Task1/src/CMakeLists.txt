add_subdirectory(utils)

function(declare_detector target utils_target async_enabled)
    add_executable(${target} main.cc)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-result -Wno-unknown-pragmas
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-march=native>)
    target_link_libraries(${target} PRIVATE ${utils_target})

    if(async_enabled)
        target_link_libraries(${target} PRIVATE Threads::Threads)
    else()
        target_compile_definitions(${target} PRIVATE DISABLE_ASYNC)
    endif()
endfunction()

declare_detector(detect_seq   utils_seq OFF)
declare_detector(detect_async utils_seq ON)
declare_detector(detect_omp   utils_omp OFF)
declare_detector(detect       utils_omp ON)

set_target_properties(detect detect_omp detect_async detect_seq
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
