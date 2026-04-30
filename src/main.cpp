#include <drogon/drogon.h>

#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace {

std::string findConfigPath() {
    constexpr std::string_view kCandidates[] = {
        "config/config.json",
        "../config/config.json",
    };

    for (const auto candidate : kCandidates) {
        if (std::filesystem::exists(candidate)) {
            return std::string(candidate);
        }
    }

    throw std::runtime_error(
        "Config file not found. Tried: config/config.json and ../config/config.json");
}

}  // namespace

int main() {
    drogon::app().loadConfigFile(findConfigPath());
    drogon::app().run();
    return 0;
}
