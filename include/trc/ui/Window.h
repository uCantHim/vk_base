#pragma once

#include "trc/text/GlyphLoading.h"
#include "trc/ui/DrawInfo.h"
#include "trc/ui/FontRegistry.h"
#include "trc/ui/IoConfig.h"
#include "trc/ui/event/Event.h"

namespace trc::ui
{
    class Element;
    class Window;

    template<typename T>
    concept GuiElement = std::derived_from<T, Element>;

    template<GuiElement E>
    class ElementHandleProxy;

    /**
     * @brief Initialize global callbacks to the user
     *
     * The user is notified when a resource is loaded, for example. These
     * callbacks are usually set by the active backend that has to manage
     * its versions of loaded resources.
     */
    void initUserCallbacks(std::function<void(ui32, const GlyphCache&)> onFontLoad,
                           std::function<void(ui32)>                    onImageLoad);

    struct WindowCreateInfo
    {
        uvec2 initialSize{ 1, 1 };
        KeyMapping keyMap;
    };

    /**
     * Acts as a root for the gui
     */
    class Window
    {
    public:
        explicit Window(const WindowCreateInfo& createInfo);

        Window(Window&&) noexcept = default;
        ~Window() noexcept = default;

        Window(const Window&) = delete;
        auto operator=(const Window&) -> Window& = delete;
        auto operator=(Window&&) -> Window& = delete;

        /**
         * Calculate global transformatio, then build a list of DrawInfos
         * from all elements in the tree.
         */
        auto draw() -> const DrawList&;

        auto getSize() const -> vec2;
        auto getRoot() -> Element&;

        /**
         * @brief Create an element
         *
         * The new element is not attached to anything. If you want it to
         * be drawn, attach it to the window's root element
         * (`Window::getRoot()`) or to any of the root's child elements.
         */
        template<GuiElement E, typename... Args>
        auto create(Args&&... args) -> s_ptr<E>;

        /**
         * @brief Notify a change in logical UI window size.
         *
         * Use this to change the size of the logical UI window. Sizes and
         * positions of elements are calculated relative to this.
         */
        void setSize(uvec2 newSize);

        /**
         * @brief Signal to the window that a mouse click has occured
         */
        void signalMouseClick(float posPixelsX, float posPixelsY);

        // void signalMouseRelease(float posPixelsX, float posPixelsY);
        // void signalMouseMove(float posPixelsX, float posPixelsY);

        void signalKeyPress(int key);
        void signalKeyRepeat(int key); // Just issues a key press event for now
        void signalKeyRelease(int key);
        void signalCharInput(ui32 character);

        auto getIoConfig() -> IoConfig&;
        auto getIoConfig() const -> const IoConfig&;

        /**
         * Calculate the absolute pixel values of a normalized point
         */
        auto normToPixels(vec2 p) const -> vec2;

        /**
         * Normalize a point in pixels relative to the window size
         */
        auto pixelsToNorm(vec2 p) const -> vec2;

    private:
        /**
         * @brief Dispatch an event to all events that the mouse hovers
         */
        template<std::derived_from<event::MouseEvent> EventType>
        void descendMouseEvent(EventType event);

        /**
         * Stops descending the event if `breakCondition` returns `false`.
         */
        template<
            std::derived_from<event::EventBase> EventType,
            typename F
        >
        void descendEvent(EventType event, F breakCondition)
            requires std::is_same_v<bool, std::invoke_result_t<F, Element&>>;

        /**
         * @brief Recalculate positions of elements
         *
         * This converts normalized or absolute positions based on window size
         * and stuff like that.
         */
        void realignElements();

        uvec2 windowSize;
        IoConfig ioConfig;

        DrawList drawList;

        /**
         * Traverses the tree recusively and applies a function to all
         * visited elements. The function is applied to parents first, then
         * to their children.
         */
        template<std::invocable<Element&> F>
        void traverse(F elemCallback);

        // The root element
        u_ptr<Element> root;
    };



    template<GuiElement E, typename... Args>
    inline auto Window::create(Args&&... args) -> s_ptr<E>
    {
        // Construct with Window in constructor if possible
        if constexpr (std::is_constructible_v<E, Window&, Args...>) {
            return std::make_shared<E>(*this, std::forward<Args>(args)...);
        }
        // Construct with args only and set window member later
        else {
            auto newElem = std::make_shared<E>(std::forward<Args>(args)...);
            newElem->window = this;

            return newElem;
        }
    }
} // namespace trc::ui
