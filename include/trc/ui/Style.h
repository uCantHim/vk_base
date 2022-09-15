#pragma once

#include <variant>

#include "trc/Types.h"
#include "trc/ui/Transform.h"

namespace trc::ui
{
    struct TextureInfo
    {
        vec2 uvLL;
        vec2 uvUR;
        ui32 textureIndex;
    };

    class DefaultStyle
    {
    public:
        static inline vec4 background{ 0.3f, 0.3f, 0.7f, 1.0f };
        static inline vec4 textColor{ 1.0f };

        static inline ui32 borderThickness{ 0 };
        static inline vec4 borderColor{ 0.8f, 0.8f, 1.0f, 1.0f };
        static inline vec2 padding{ 8, 5 };

        static inline ui32 font{ 0 };
        static inline ui32 fontSize{ 20 };
    };

    /**
     * Only pixel-format padding is possible.
     *
     * Padding is always applied on both opposite sides of the element.
     * Padding on the x-axis is applied left and right, padding on the
     * y-axis is applied top and bottom.
     */
    class Padding
    {
    public:
        Padding() = default;
        Padding(float x, float y);

        void set(vec2 v);
        void set(float x, float y);
        auto get() const -> vec2;

        /**
         * @param const Window& window
         *
         * @return vec2 Normalized padding value.
         */
        auto calcNormalizedPadding(const Window& window) const -> vec2;

    private:
        vec2 padding{ DefaultStyle::padding };
    };

    /**
     * Generic draw information for all elements
     */
    struct ElementStyle
    {
        std::variant<vec4, TextureInfo> background{ DefaultStyle::background };
        std::variant<vec4, TextureInfo> foreground{ DefaultStyle::textColor };

        ui32 borderThickness{ DefaultStyle::borderThickness };
        vec4 borderColor{ DefaultStyle::borderColor };

        Padding padding;
    };
} // namespace trc::ui
