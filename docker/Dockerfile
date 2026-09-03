# SkiffLLM server image.
#
# The image builds the desktop binary and its llama.cpp dependency. Models are
# never baked into the image; mount a read-only GGUF directory at runtime.
#
#   docker build -t llm:local .
#   docker run --rm -p 127.0.0.1:8080:8080 \
#     -v /srv/llm/models:/models:ro \
#     llm:local --model /models/model-q4_k_m.gguf \
#     --serve --host 0.0.0.0 --port 8080 --api-key "$LLM_SERVER_KEY"

# ---- Build stage --------------------------------------------------------
FROM alpine:3.20 AS build

RUN apk add --no-cache \
    build-base \
    cmake \
    ninja \
    git \
    python3

WORKDIR /src
COPY . .

# Force the local (CMAKE_BUILD_TYPE=Release) configuration and let CMake fetch
# the pinned llama.cpp source at configure time. If you have a vendored
# llama.cpp checkout, pass LLM_LLAMA_SOURCE_DIR instead.
RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_BUILD_PARALLEL_LEVEL="$(nproc)" \
      -DLLM_FETCH_LLAMA=ON && \
    cmake --build build -j"$(nproc)" && \
    ctest --test-dir build --output-on-failure

# ---- Runtime stage ------------------------------------------------------
FROM alpine:3.20

RUN apk add --no-cache \
    ca-certificates \
    libstdc++ \
    libgcc

RUN addgroup -S llm && adduser -S -G llm -H llm

WORKDIR /app
COPY --from=build /src/build/llm /usr/local/bin/llm

# Models are expected at /models; sessions and history go to a writable volume.
USER llm

ENTRYPOINT ["/usr/local/bin/llm"]
CMD ["--help"]
