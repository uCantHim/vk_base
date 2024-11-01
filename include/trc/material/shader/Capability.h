#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace trc::shader
{
    class Capability
    {
    public:
        Capability() = delete;

        constexpr Capability(const char* str) : name(str) {}
        constexpr Capability(std::string_view str) : name(str) {}

        constexpr auto getName() const -> std::string_view {
            return name;
        }

        constexpr auto toString() const -> std::string {
            return std::string{name};
        }

        constexpr bool operator==(const Capability& other) const {
            return name == other.name;
        }

    private:
        std::string_view name;
    };
} // namespace trc::shader

template<>
struct std::hash<trc::shader::Capability>
{
    constexpr auto operator()(const trc::shader::Capability& capability) const -> size_t
    {
        return hash<std::string_view>{}(capability.getName());
    }
};
