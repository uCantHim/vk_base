#pragma once

#include <componentlib/ComponentStorage.h>
#include <trc_util/data/IdPool.h>
#include <trc_util/data/IndexMap.h>

#include "trc/LightSceneModule.h"
#include "trc/RasterSceneModule.h"
#include "trc/RaySceneModule.h"
#include "trc/Types.h"
#include "trc/core/SceneBase.h"
#include "trc/drawable/Drawable.h"

namespace trc
{
    class DrawableScene;

    /**
     * @brief A shared handle to a drawable object
     *
     * # Example
     * ```cpp
     *
     * Scene scene;
     * Drawable myDrawable = scene.makeDrawable({ myGeo, myMat });
     * ```
     *
     * @note I can extend this to a custom struct with an overloaded `operator->`
     * if a more sophisticated interface is required.
     */
    using Drawable = s_ptr<DrawableObj>;

    struct DrawableCreateInfo
    {
        GeometryID geo;
        MaterialID mat;

        bool rasterized{ true };
        bool rayTraced{ false };
    };

    /**
     * @brief
     */
    class DrawableScene
        : public componentlib::ComponentStorage<DrawableScene, DrawableID>
        , public SceneBase
    {
    public:
        DrawableScene();

        void update(float timeDeltaMs);

        auto getRasterModule() -> RasterSceneModule&;
        auto getRayModule() -> RaySceneModule&;
        auto getLights() -> LightSceneModule&;
        auto getLights() const -> const LightSceneModule&;
        auto getRoot() noexcept -> Node&;
        auto getRoot() const noexcept -> const Node&;

        auto makeDrawable(const DrawableCreateInfo& createInfo) -> Drawable;

    private:
        template<componentlib::ComponentType T>
        friend struct componentlib::ComponentTraits;

        /**
         * @brief A more expressive name for `createObject`
         */
        inline auto makeDrawable() -> DrawableID {
            return createObject();
        }

        /**
         * @brief A more expressive name for `deleteObject`
         */
        inline void destroyDrawable(DrawableID drawable) {
            deleteObject(drawable);
        }

        void updateAnimations(float timeDelta);

        /**
         * @brief Update transformations of ray geometry instances
         */
        void updateRayInstances();

        Node root;
    };
} // namespace trc
