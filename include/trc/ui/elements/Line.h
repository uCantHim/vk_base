#pragma once

#include "trc/ui/Element.h"

namespace trc::ui
{
    class Line : public Element
    {
    public:
        explicit Line(Window& window) : Element(window) {}

        void draw(DrawList& drawList) override
        {
            drawList.push(types::Line{}, *this);
        }
    };
} // namespace trc::ui
