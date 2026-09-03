cmake_minimum_required(VERSION 3.20)

project(SkiffLLM VERSION 1.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

option(SKIFFLLM_BUILD_TESTS "Build SkiffLLM unit tests" ON)
option(SKIFFLLM_FETCH_LLAMA "Download and build llama.cpp automatically" ON)
option(SKIFFLLM_BUILD_SHARED_LLAMA "Build llama.cpp as a shared library" OFF)
option(SKIFFLLM_ENABLE_ROOTFS_CHECK "Allow the CMake configure step to continue when running as root" OFF)
option(SKIFFLLM_USE_READLINE "Use GNU Readline for line editing when available" ON)
set(SKIFFLLM_LLAMA_GIT_TAG "85c55223caf0a2ad0d1d88e5a73ab3fe36107867" CACHE STRING "llama.cpp git revision")
set(SKIFFLLM_LLAMA_SOURCE_DIR "" CACHE PATH "Path to an existing llama.cpp checkout")
set(SKIFFLLM_LLAMA_BACKEND "auto" CACHE STRING "Hardware backend: auto, cuda, metal, vulkan, opencl, blas, cpu")
set_property(CACHE SKIFFLLM_LLAMA_BACKEND PROPERTY STRINGS auto cuda metal vulkan opencl blas cpu)

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

# Hardware-acceleration backend selection. The default "auto" keeps the
# upstream llm.cpp defaults (CPU only unless the platform already enables
# another backend).
if(SKIFFLLM_LLAMA_BACKEND STREQUAL "cuda")
    set(GGML_CUDA ON CACHE BOOL "" FORCE)
elseif(SKIFFLLM_LLAMA_BACKEND STREQUAL "metal")
    set(GGML_METAL ON CACHE BOOL "" FORCE)
elseif(SKIFFLLM_LLAMA_BACKEND STREQUAL "vulkan")
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)
elseif(SKIFFLLM_LLAMA_BACKEND STREQUAL "opencl")
    set(GGML_OPENCL ON CACHE BOOL "" FORCE)
elseif(SKIFFLLM_LLAMA_BACKEND STREQUAL "blas")
    set(GGML_BLAS ON CACHE BOOL "" FORCE)
elseif(SKIFFLLM_LLAMA_BACKEND STREQUAL "cpu")
    set(GGML_CUDA OFF CACHE BOOL "" FORCE)
    set(GGML_METAL OFF CACHE BOOL "" FORCE)
    set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
    set(GGML_OPENCL OFF CACHE BOOL "" FORCE)
    set(GGML_BLAS OFF CACHE BOOL "" FORCE)
endif()

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
    find_package(llama CONFIG REQUIRED)
endif()

add_library(skifflm_core
    src/cli_utils.cpp
    src/config.cpp
    src/engine.cpp
    src/http_auth.cpp
    src/server.cpp
    src/session.cpp
    src/terminal.cpp
    src/tools.cpp
)
target_compile_definitions(skifflm_core PRIVATE SKIFFLLM_VERSION="${PROJECT_VERSION}")

# Mirror the build-time backend selection into the core target so that
# `skifflm --backend-info` reports the real compile-time accelerators.
if(GGML_CUDA)
    target_compile_definitions(skifflm_core PRIVATE GGML_USE_CUDA)
endif()
if(GGML_CUBLAS)
    target_compile_definitions(skifflm_core PRIVATE GGML_USE_CUBLAS)
endif()
if(GGML_METAL)
    target_compile_definitions(skifflm_core PRIVATE GGML_USE_METAL)
endif()
if(GGML_VULKAN)
    target_compile_definitions(skifflm_core PRIVATE GGML_USE_VULKAN)
endif()
if(GGML_OPENCL)
    target_compile_definitions(skifflm_core PRIVATE GGML_USE_OPENCL)
endif()
if(GGML_BLAS)
    target_compile_definitions(skifflm_core PRIVATE GGML_USE_BLAS)
endif()

target_include_directories(skifflm_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(skifflm_core PUBLIC llama)

if(WIN32)
    target_link_libraries(skifflm_core PUBLIC ws2_32)
endif()

if(SKIFFLLM_USE_READLINE)
    find_package(Readline QUIET)
    if(Readline_FOUND)
        target_compile_definitions(skifflm_core PRIVATE SKIFFLLM_HAVE_READLINE)
        target_link_libraries(skifflm_core PRIVATE Readline::Readline)
        message(STATUS "SkiffLLM: GNU Readline line editing enabled")
    else()
        message(STATUS "SkiffLLM: GNU Readline not found; falling back to getline")
    endif()
endif()

target_compile_features(skifflm_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(skifflm_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(skifflm_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(skifflm src/main.cpp)
target_link_libraries(skifflm PRIVATE skifflm_core)
target_compile_definitions(skifflm PRIVATE SKIFFLLM_VERSION="${PROJECT_VERSION}")

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
    add_test(NAME version COMMAND skifflm --version)
    add_test(NAME config COMMAND skifflm --show-config)
    add_test(NAME list_models COMMAND skifflm --list-models)
    add_test(NAME doctor COMMAND skifflm --doctor)
    add_test(NAME backend_info COMMAND skifflm --backend-info)
    add_test(NAME export COMMAND skifflm --export ${CMAKE_CURRENT_BINARY_DIR}/export-test.md --no-save --quiet)
endif()

install(TARGETS skifflm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS skifflm_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY scripts/completions/ DESTINATION ${CMAKE_INSTALL_DATADIR}/skifflm/completions)
install(FILES configs/skifflm.example.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/skifflm)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/skifflm/docs)
install(FILES docs/skifflm.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
