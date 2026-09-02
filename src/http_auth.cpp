#include "skifflm/http_auth.hpp"

#include <cctype>

#include "skifflm/config.hpp"

namespace skifflm {

namespace {

bool starts_with_ci(const std::string& text, const std::string& prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(prefix[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

bool constant_time_equal(const std::string& expected, const std::string& actual) {
    if (expected.size() != actual.size()) {
        return false;
    }
    unsigned char difference = 0;
    for (size_t i = 0; i < expected.size(); ++i) {
        difference |=
            static_cast<unsigned char>(expected[i]) ^ static_cast<unsigned char>(actual[i]);
    }
    return difference == 0;
}

}  // namespace

bool bearer_token_matches(const std::string& authorization, const std::string& api_key) {
    if (api_key.empty()) {
        return true;
    }

    const std::string trimmed = trim(authorization);
    const std::string scheme = "bearer ";
    if (!starts_with_ci(trimmed, scheme)) {
        return false;
    }

    const std::string token = trim(trimmed.substr(scheme.size()));
    if (token.empty()) {
        return false;
    }
    return constant_time_equal(api_key, token);
}

}  // namespace skifflm
