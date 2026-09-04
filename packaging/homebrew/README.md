# Homebrew packaging

This directory documents how to publish SkiffLLM as a Homebrew formula once a
release archive exists.

## What is needed

1. A real GitHub release with an archive asset, for example:
   `https://github.com/thesyntax11/SkiffLLM/archive/refs/tags/v1.9.0.tar.gz`
2. The SHA-256 of that archive:
   ```bash
   curl -L https://github.com/thesyntax11/SkiffLLM/archive/refs/tags/v1.9.0.tar.gz -o v1.9.0.tar.gz
   sha256sum v1.9.0.tar.gz
   ```

## Adding a tap

Create or edit a file named `skiffllm.rb` in a Homebrew tap with the following
shape and replace the URL, SHA-256, and version fields:

```ruby
class Skifflm < Formula
  desc "Offline-first local LLM terminal assistant built on llama.cpp"
  homepage "https://github.com/thesyntax11/SkiffLLM"
  url "https://github.com/thesyntax11/SkiffLLM/archive/refs/tags/v1.9.0.tar.gz"
  sha256 "REPLACE_WITH_REAL_SHA256"
  license "MIT"

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build",
           "-DSKIFFLLM_BUILD_TESTS=OFF",
           "-DCMAKE_BUILD_TYPE=Release"
    system "cmake", "--build", "build"
    bin.install "build/skiffllm"
    man1.install "docs/skiffllm.1"
    doc.install Dir["docs/*.md"]
    (share/"skiffllm/completions").install Dir["scripts/completions/*"]
  end
end
```

## Publishing a personal tap

Create a repository such as `thesyntax11/homebrew-skiffllm`, add the formula,
then users install with:

```bash
brew tap thesyntax11/skiffllm
brew install skiffllm
```

## Publishing to homebrew-core

Homebrew-core requires passing their formula review and bottle-building
process. That is outside this repository and should follow the official
Homebrew contribution guide.
