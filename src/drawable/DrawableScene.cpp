#include "trc/drawable/DrawableScene.h"

#include "trc/drawable/AnimationComponent.h"
#include "trc/drawable/RasterComponent.h"
#include "trc/drawable/RayComponent.h"



namespace trc
{

DrawableScene::DrawableScene()
{
    registerModule(std::make_unique<RasterSceneModule>());
    registerModule(std::make_unique<RaySceneModule>());
    registerModule(std::make_unique<LightSceneModule>());
}

void DrawableScene::update(float timeDeltaMs)
{
    // Update transformations in the node tree
    root.updateAsRoot();

    updateAnimations(timeDeltaMs);
    updateRayInstances();
}

void DrawableScene::updateAnimations(const float timeDelta)
{
    for (auto& anim : get<AnimationComponent>())
    {
        anim.engine.update(timeDelta);
    }
}

void DrawableScene::updateRayInstances()
{
    for (const auto& ray : get<RayComponent>()) {
        getRayModule().setInstanceTransform(ray.instanceDataIndex, ray.modelMatrix.get());
    }
}

auto DrawableScene::getRasterModule() -> RasterSceneModule&
{
    return getModule<RasterSceneModule>();
}

auto DrawableScene::getRayModule() -> RaySceneModule&
{
    return getModule<RaySceneModule>();
}

auto DrawableScene::getLights() -> LightSceneModule&
{
    return getModule<LightSceneModule>();
}

auto DrawableScene::getLights() const -> const LightSceneModule&
{
    return getModule<LightSceneModule>();
}

auto DrawableScene::getRoot() noexcept -> Node&
{
    return root;
}

auto DrawableScene::getRoot() const noexcept -> const Node&
{
    return root;
}

auto DrawableScene::makeDrawable(const DrawableCreateInfo& info) -> Drawable
{
    const DrawableID id = DrawableScene::makeDrawable();

    std::shared_ptr<DrawableObj> drawable(
        new DrawableObj{ id, *this, info.geo, info.mat },
        [this](DrawableObj* drawable) {
            destroyDrawable(drawable->id);
            delete drawable;
        }
    );

    // Create a rasterization component
    if (info.rasterized)
    {
        auto geo = info.geo.getDeviceDataHandle();

        add<RasterComponent>(id, RasterComponentCreateInfo{
            .geo=info.geo,
            .mat=info.mat,
            .modelMatrixId=drawable->getGlobalTransformID(),
            .anim=geo.hasRig()
                ? add<AnimationComponent>(id, geo.getRig()).engine.getState()
                : AnimationEngine::ID{}
        });
    }

    // Create a ray tracing component
    if (info.rayTraced)
    {
        add<RayComponent>(
            id,
            RayComponentCreateInfo{ info.geo, info.mat, drawable->getGlobalTransformID() }
        );
    }

    return drawable;
}

} // namespace trc
