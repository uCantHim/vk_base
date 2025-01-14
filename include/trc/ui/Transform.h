#pragma once

#include "trc/Types.h"

namespace trc::ui
{
    enum class Format : ui8
    {
        eNorm,
        ePixel,
    };

    enum class Align : ui8
    {
        eRelative,
        eAbsolute
    };

    template<typename T>
    struct _2D
    {
        constexpr _2D() = default;
        constexpr _2D(const _2D& val) = default;
        constexpr _2D(_2D&& val) noexcept = default;
        ~_2D() noexcept = default;

        constexpr _2D(const T& val) : x(val), y(val) {}
        constexpr _2D(const T& a, const T& b) : x(a), y(b) {}

        auto operator=(const _2D&) -> _2D& = default;
        auto operator=(_2D&&) -> _2D& = default;

        // Assignment from single component
        auto operator=(const T& rhs) -> _2D<T>&
        {
            x = rhs;
            y = rhs;
            return *this;
        }

        constexpr auto operator<=>(const _2D<T>&) const = default;

        T x;
        T y;
    };

    struct Transform
    {
        struct Properties
        {
            _2D<Format> format{ Format::eNorm };
            _2D<Align> align{ Align::eRelative };
        };

        vec2 position{ 0.0f, 0.0f };
        vec2 size{ 1.0f, 1.0f };

        Properties posProp{};
        Properties sizeProp{};
    };

    class Window;

    auto concat(Transform parent, Transform child, const Window& window) noexcept -> Transform;

    /**
     * Internal representation of a pixel value
     */
    struct _pix {
        float value;
    };

    /**
     * Internal representation of a normalized value
     */
    struct _norm {
        float value;
    };

    namespace size_literals
    {
        inline constexpr _pix operator ""_px(long double val) {
            return _pix{ static_cast<float>(val) };
        }

        inline constexpr _pix operator ""_px(unsigned long long int val) {
            return _pix{ static_cast<float>(val) };
        }

        inline constexpr _norm operator ""_n(long double val) {
            return _norm{ static_cast<float>(val) };
        }

        inline constexpr _norm operator ""_n(unsigned long long int val) {
            return _norm{ static_cast<float>(val) };
        }
    } // namespace size_literals
} // namespace trc::ui
