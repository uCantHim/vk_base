#pragma once

#include <concepts>
#include <vector>

#include "trc/Types.h"
#include "trc/ui/Transform.h"

namespace trc::ui
{
    template<typename Derived>
    class CRTPNode
    {
    public:
        CRTPNode(const CRTPNode&) = delete;
        CRTPNode(CRTPNode&&) noexcept = default;
        auto operator=(const CRTPNode&) = delete;
        auto operator=(CRTPNode&&) noexcept -> CRTPNode& = default;

        CRTPNode() {
            static_assert(std::derived_from<Derived, CRTPNode<Derived>>, "");
        }

        ~CRTPNode() noexcept {
            if (parent != nullptr) {
                parent->detach(static_cast<Derived*>(this));
            }
        }

        void attach(s_ptr<Derived> child);
        void detach(s_ptr<Derived> child);
        void clearChildren();

        template<std::invocable<Derived&> F>
        void foreachChild(F func);

    private:
        void detach(Derived* child);

        Derived* parent{ nullptr };
        std::vector<s_ptr<Derived>> children;
    };

    template<typename Derived>
    class TransformNode : public CRTPNode<Derived>
    {
    public:
        /**
         * Can't use concepts with CRTP because neither the derived type
         * nor the parent type are complete at concept resolution time, so
         * I use static_assert instead.
         */
        TransformNode() {
            static_assert(std::derived_from<Derived, TransformNode<Derived>>, "");
        }

        auto getPos() -> vec2;
        auto getSize() -> vec2;

        void setPos(vec2 newPos);
        void setPos(float x, float y);
        void setPos(_pix x, _pix y);
        void setPos(_pix x, _norm y);
        void setPos(_norm x, _pix y);
        void setPos(_norm x, _norm y);

        void setSize(vec2 newSize);
        void setSize(float x, float y);
        void setSize(_pix x, _pix y);
        void setSize(_pix x, _norm y);
        void setSize(_norm x, _pix y);
        void setSize(_norm x, _norm y);

        auto getTransform() -> Transform;
        void setTransform(Transform newTransform);

        auto getPositionProperties() -> Transform::Properties;
        auto getSizeProperties() -> Transform::Properties;
        auto setPositionProperties(Transform::Properties newProps);
        auto setSizeProperties(Transform::Properties newProps);

    private:
        Transform localTransform;
    };

#include "trc/ui/CRTPNode.inl"

} // namespace trc::ui
