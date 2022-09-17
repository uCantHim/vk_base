#include "trc/ui/Window.h"

#include <ranges>
#include <iostream>



void trc::ui::initUserCallbacks(
    std::function<void(trc::basic_types::ui32, const GlyphCache&)> onFontLoad,
    std::function<void(trc::basic_types::ui32)>)
{
    FontRegistry::setFontAddCallback(std::move(onFontLoad));
}



trc::ui::Window::Window(WindowCreateInfo createInfo)
    :
    onWindowDestruction(std::move(createInfo.onWindowDestruction)),
    windowBackend(std::move(createInfo.windowBackend))
{
    assert(this->windowBackend != nullptr);

    ioConfig.keyMap = createInfo.keyMap;
}

trc::ui::Window::~Window()
{
    onWindowDestruction(*this);
}

auto trc::ui::Window::draw() -> const DrawList&
{
    realignElements();

    drawList.clear();
    traverse([this](Element& elem) {
        elem.draw(drawList);
    });

    return drawList;
}

auto trc::ui::Window::getSize() const -> vec2
{
    return windowBackend->getSize();
}

auto trc::ui::Window::getRoot() -> Element&
{
    return *root;
}

void trc::ui::Window::destroy(Element& elem)
{
    drawableElements.erase(std::remove_if(
        drawableElements.begin(), drawableElements.end(),
        [&elem](u_ptr<Element>& e) { return e.get() == &elem; }
    ));
}

void trc::ui::Window::signalMouseClick(float posPixelsX, float posPixelsY)
{
    event::Click event;
    event.mousePosPixels = vec2{ posPixelsX, posPixelsY };
    event.mousePosNormal = vec2{ posPixelsX, posPixelsY } / getSize();

    descendMouseEvent(event);
}

void trc::ui::Window::signalKeyPress(int key)
{
    ioConfig.keysDown[key] = true;

    event::KeyPress event(key);
    traverse([&](Element& e) { e.notify(event); });
}

void trc::ui::Window::signalKeyRepeat(int key)
{
    signalKeyPress(key);
}

void trc::ui::Window::signalKeyRelease(int key)
{
    ioConfig.keysDown[key] = false;

    event::KeyRelease event(key);
    traverse([&](Element& e) { e.notify(event); });
}

void trc::ui::Window::signalCharInput(ui32 character)
{
    event::CharInput event(character);
    traverse([&](Element& e) { e.notify(event); });
}

auto trc::ui::Window::getIoConfig() -> IoConfig&
{
    return ioConfig;
}

auto trc::ui::Window::getIoConfig() const -> const IoConfig&
{
    return ioConfig;
}

auto trc::ui::Window::normToPixels(vec2 p) const -> vec2
{
    return glm::floor(p * windowBackend->getSize());
}

auto trc::ui::Window::pixelsToNorm(vec2 p) const -> vec2
{
    return p / windowBackend->getSize();
}

void trc::ui::Window::realignElements()
{
    using FuncType = std::function<std::pair<vec2, vec2>(Transform, Element&)>;
    FuncType calcTransform = [&](Transform parentTransform, Element& elem)
        -> std::pair<vec2, vec2>
    {
        vec2 pos = parentTransform.position;
        vec2 size = parentTransform.size;

        // Apply padding
        const vec2 padding = elem.style.padding.calcNormalizedPadding(*this);
        parentTransform.position += padding;  // Offset all children by padding value

        elem.foreachChild([&, parentTransform](Element& child)
        {
            const auto childTransform = concat(parentTransform, child.getTransform(), *this);
            auto [childPos, childSize] = calcTransform(childTransform, child);

            /**
             * TODO: Never resize to fit children. Add default scissor rectangle to each
             * element.
             *
             * Provide a flag that enables auto-resizing.
             */
            childSize += padding;

            pos = glm::min(pos, childPos);
            size = glm::max(size, (childPos - pos) + childSize);
        });

        return { (elem.globalPos = pos), (elem.globalSize = size) };
    };

    // concat once to ensure that globalTransform is normalized
    calcTransform(concat({}, root->getTransform(), *this), *root);
}
