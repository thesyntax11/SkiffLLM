cmake_minimum_required(VERSION 3.20)

project(SkiffLLM VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SKIFFLLM_BUILD_TESTS "Build SkiffLLM unit tests" ON)
option(SKIFFLLM_FETCH_LLAMA "Download and build llama.cpp automatically" ON)
option(SKIFFLLM_BUILD_SHARED_LLAMA "Build llama.cpp as a shared library" OFF)
set(SKIFFLLM_LLAMA_GIT_TAG "5d4a3be26de60093263b2d41ca24c3a49de95cd5" CACHE STRING "llama.cpp git revision")
set(SKIFFLLM_LLAMA_SOURCE_DIR "" CACHE PATH "Path to an existing llama.cpp checkout")

if(SKIFFLLM_BUILD_SHARED_LLAMA)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
else()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
endif()

set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_MTMD OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_UI OFF CACHE BOOL "" FORCE)
set(GGML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GGML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GGML_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

if(SKIFFLLM_FETCH_LLAMA OR SKIFFLLM_LLAMA_SOURCE_DIR)
    include(FetchContent)
    if(SKIFFLLM_LLAMA_SOURCE_DIR)
        FetchContent_Declare(llama SOURCE_DIR "${SKIFFLLM_LLAMA_SOURCE_DIR}")
    else()
        FetchContent_Declare(llama
            GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
            GIT_TAG ${SKIFFLLM_LLAMA_GIT_TAG}
            GIT_SHALLOW TRUE
        )
    endif()
    FetchContent_MakeAvailable(llama)
else()
    message(FATAL_ERROR "Set SKIFFLLM_LLAMA_SOURCE_DIR or enable SKIFFLLM_FETCH_LLAMA")
endif()

add_library(skifflm_core
    src/config.cpp
    src/engine.cpp
    src/session.cpp
    src/terminal.cpp
)

target_include_directories(skifflm_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(skifflm_core PUBLIC llama)

if(MSVC)
    target_compile_options(skifflm_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(skifflm_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(skifflm src/main.cpp)
target_link_libraries(skifflm PRIVATE skifflm_core)

if(MSVC)
    target_compile_options(skifflm PRIVATE /W4 /permissive-)
else()
    target_compile_options(skifflm PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SKIFFLLM_BUILD_TESTS)
    enable_testing()
    add_executable(skifflm_tests tests/test_main.cpp)
    target_link_libraries(skifflm_tests PRIVATE skifflm_core)
    if(MSVC)
        target_compile_options(skifflm_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(skifflm_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME unit COMMAND skifflm_tests)
    add_test(NAME help COMMAND skifflm --help)
endif()

install(TARGETS skifflm RUNTIME DESTINATION bin)
install(TARGETS skifflm_core ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
