set(UTILS_SOURCES image.cc dct.cc)

add_library(utils_seq STATIC ${UTILS_SOURCES})
target_include_directories(utils_seq PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(utils_seq PRIVATE
    -O2 -Wall -Wextra -Wpedantic -Wno-unused-result -Wno-unknown-pragmas)
target_link_libraries(utils_seq PUBLIC image_io_deps)

add_library(utils_omp STATIC ${UTILS_SOURCES})
target_include_directories(utils_omp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(utils_omp PRIVATE
    -O2 -Wall -Wextra -Wpedantic -Wno-unused-result)
target_link_libraries(utils_omp PUBLIC image_io_deps OpenMP::OpenMP_CXX)
target_compile_definitions(utils_omp PUBLIC IMG_OMP_THRESHOLD=65536 DCT_OMP_BLOCK_THRESHOLD=128)
