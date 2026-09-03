cmake_minimum_required(VERSION 3.22)
project(skiffllm_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP OFF CACHE BOOL "" FORCE)
set(GGML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GGML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GGML_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
# Do not enable -march=native while cross-compiling for the Android NDK.
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)

# Android GPU acceleration. Default is CPU-only; pass
# -DSKIFFLLM_LLAMA_BACKEND=vulkan or opencl to enable a GPU backend.
if(SKIFFLLM_LLAMA_BACKEND STREQUAL "vulkan")
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)
elseif(SKIFFLLM_LLAMA_BACKEND STREQUAL "opencl")
    set(GGML_OPENCL ON CACHE BOOL "" FORCE)
elseif(SKIFFLLM_LLAMA_BACKEND STREQUAL "cpu")
    set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
    set(GGML_OPENCL OFF CACHE BOOL "" FORCE)
endif()

if(SKIFFLLM_LLAMA_SOURCE_DIR)
    add_subdirectory("${SKIFFLLM_LLAMA_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/llama")
else()
    include(FetchContent)
    FetchContent_Declare(llama
        GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
        GIT_TAG 85c55223caf0a2ad0d1d88e5a73ab3fe36107867
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(llama)
endif()

add_library(skiffllm SHARED
    jni_bridge.cpp
    "${PROJECT_ROOT}/src/config.cpp"
    "${PROJECT_ROOT}/src/engine.cpp"
)

target_include_directories(skiffllm
    PRIVATE
        "${PROJECT_ROOT}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(skiffllm PUBLIC llama)
