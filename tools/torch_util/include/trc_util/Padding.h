#pragma once

#include <type_traits>

namespace trc::util
{
    template<typename T, typename U>
    inline constexpr T pad(T val, U padding)
    {
        static_assert(std::is_integral_v<T>, "Type must be integral type!");
        static_assert(std::is_integral_v<U>, "Type must be integral type!");

        auto remainder = val % static_cast<T>(padding);
        if (remainder == 0)
            return val;
        else
            return val + (static_cast<T>(padding) - remainder);
    }

    template<typename T>
    inline constexpr T pad_16(T val)
    {
        return pad(val, static_cast<T>(16));
    }

    template<typename T>
    inline constexpr size_t sizeof_pad_16()
    {
        return pad_16(sizeof(T));
    }

    template<size_t Value>
    constexpr auto pad_16_v = pad_16(Value);

    template<typename T>
    constexpr auto sizeof_pad_16_v = sizeof_pad_16<T>();
} // namespace trc::util
