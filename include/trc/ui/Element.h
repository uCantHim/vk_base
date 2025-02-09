#pragma once

#include <concepts>

#include "trc/Types.h"
#include "trc/ui/CRTPNode.h"
#include "trc/ui/event/Event.h"
#include "trc/ui/event/EventListenerRegistryBase.h"
#include "trc/ui/event/InputEvent.h"
#include "trc/ui/Window.h"

namespace trc::ui
{
    class Element;

    template<typename ...EventTypes>
    struct InheritEventListener : public EventListenerRegistryBase<EventTypes>...
    {
        using EventListenerRegistryBase<EventTypes>::addEventListener...;
        using EventListenerRegistryBase<EventTypes>::removeEventListener...;
        using EventListenerRegistryBase<EventTypes>::notify...;
    };

    struct ElementEventBase : public InheritEventListener<
                                // Low-level events
                                event::KeyPress, event::KeyRelease,
                                event::CharInput,

                                // High-level user events
                                event::Click, event::Release,
                                event::Hover,
                                event::Input
                            >
    {
    };

    /**
     * Used internally by the window to store global transformations.
     * The transformation calculations are complex enough to justify
     * violating my rule against state in this regard.
     */
    struct GlobalTransformStorage
    {
    protected:
        friend class Window;

        vec2 globalPos;
        vec2 globalSize;
    };

    /**
     * @brief Base class of all UI elements
     *
     * Contains a reference to its parent window
     */
    class Element : public TransformNode<Element>
                  , public GlobalTransformStorage
                  , public Drawable
                  , public ElementEventBase
    {
    public:
        ElementStyle style;

        /**
         * @brief Create a UI element and attach it as a child
         */
        template<GuiElement E, typename ...Args>
        auto createChild(Args&&... args) -> s_ptr<E>
        {
            auto elem = window->create<E>(std::forward<Args>(args)...);
            this->attach(elem);
            return elem;
        }

    protected:
        friend class Window;

        Element() = default;
        explicit Element(Window& window)
            : window(&window)
        {}

        Window* window;
    };
} // namespace trc::ui
