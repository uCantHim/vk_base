#include "trc/drawable/DrawableComponentScene.h"

#include "trc/TorchRenderStages.h"
#include "trc/GBufferPass.h"
#include "trc/drawable/DefaultDrawable.h"

using namespace trc::drawcomp;



trc::UniqueDrawableID::UniqueDrawableID(DrawableComponentScene& scene, DrawableID id)
    :
    scene(&scene),
    id(id)
{
}

trc::UniqueDrawableID::UniqueDrawableID(UniqueDrawableID&& other) noexcept
    :
    scene(other.scene),
    id(other.id)
{
    other.scene = nullptr;
    other.id = DrawableID::NONE;
}

auto trc::UniqueDrawableID::operator=(UniqueDrawableID&& other) noexcept -> UniqueDrawableID&
{
    this->scene = other.scene;
    this->id = other.id;
    other.scene = nullptr;
    other.id = DrawableID::NONE;

    return *this;
}

trc::UniqueDrawableID::~UniqueDrawableID() noexcept
{
    if (id != DrawableID::NONE && scene != nullptr)
    {
        scene->destroyDrawable(id);
    }
}

trc::UniqueDrawableID::operator bool() const
{
    return id != DrawableID::NONE;
}

trc::UniqueDrawableID::operator trc::DrawableID() const
{
    return id;
}

auto trc::UniqueDrawableID::operator*() const -> DrawableID
{
    return id;
}

auto trc::UniqueDrawableID::get() const -> DrawableID
{
    return id;
}



trc::DrawableComponentScene::DrawableComponentScene(SceneBase& base)
    :
    base(&base)
{
}

void trc::DrawableComponentScene::updateAnimations(const float timeDelta)
{
    for (auto& anim : storage.get<AnimationComponent>())
    {
        anim.engine.update(timeDelta);
    }
}

void trc::DrawableComponentScene::updateRayData()
{
    // const auto join = storage.get<rt::GeometryInstance>().join(storage.get<NodeComponent>());
    // for (const auto& [_, geoInstance, node] : join)
    // {
    //     geoInstance.setTransform(node.node.getGlobalTransform());
    // }

    const auto join = storage.get<rt::GeometryInstance>().join(storage.get<RayComponent>());
    for (const auto& [_, geoInstance, ray] : join)
    {
        geoInstance.setTransform(ray.modelMatrix.get());
    }
}

auto trc::DrawableComponentScene::getMaxRayDeviceDataSize() const -> size_t
{
    return sizeof(RayInstanceData) * storage.rayInstances.size();
}

auto trc::DrawableComponentScene::getMaxRayGeometryInstances() const -> ui32
{
    return storage.get<rt::GeometryInstance>().size();
}

auto trc::DrawableComponentScene::writeTlasInstances(
    rt::GeometryInstance* instanceBuf,
    const ui32 maxInstances) const
    -> ui32
{
    const size_t numInstances = glm::min(size_t{maxInstances},
                                         storage.get<rt::GeometryInstance>().size());
    const size_t size = numInstances * sizeof(rt::GeometryInstance);
    memcpy(instanceBuf, storage.get<rt::GeometryInstance>().data(), size);

    return numInstances;
}

auto trc::DrawableComponentScene::writeRayDeviceData(
    void* deviceDataBuf,
    size_t maxSize) const
    -> size_t
{
    const size_t size = glm::min(maxSize, storage.rayInstances.size() * sizeof(RayInstanceData));
    memcpy(deviceDataBuf, storage.rayInstances.data(), size);

    return size;
}

auto trc::DrawableComponentScene::makeDrawable() -> DrawableID
{
    return storage.createObject();
}

auto trc::DrawableComponentScene::makeDrawableUnique() -> UniqueDrawableID
{
    return { *this, storage.createObject() };
}

void trc::DrawableComponentScene::destroyDrawable(DrawableID drawable)
{
    storage.deleteObject(drawable);
}

void trc::DrawableComponentScene::makeRasterization(
    const DrawableID drawable,
    const RasterComponentCreateInfo& createInfo)
{
    auto geoHandle = createInfo.geo.getDeviceDataHandle();
    auto matHandle = createInfo.mat.getDeviceDataHandle();

    const DrawablePipelineInfo pipelineInfo{
        .animated=geoHandle.hasRig(),
        .transparent=matHandle.isTransparent(),
    };

    auto drawInfo = std::make_shared<DrawableRasterDrawInfo>(DrawableRasterDrawInfo{
        .geo=geoHandle,
        .mat=matHandle,
        .matRuntime=matHandle.getRuntime({
            .animated=geoHandle.hasRig(),
        }),
        .modelMatrixId=createInfo.modelMatrixId,
        .anim=createInfo.anim,
    });

    // Create a storage for the draw functions with automatic lifetime
    struct RasterRegistrations
    {
        std::vector<trc::SceneBase::UniqueRegistrationID> regs;
    };
    auto& drawFuncs = storage.add<RasterRegistrations>(drawable).regs;

    using SubPasses = GBufferPass::SubPasses;
    drawFuncs.emplace_back(
        base->registerDrawFunction(
            gBufferRenderStage,
            pipelineInfo.transparent ? SubPasses::transparency : SubPasses::gBuffer,
            drawInfo->matRuntime.getPipeline(),
            makeGBufferDrawFunction(drawInfo)
        )
    );
    drawFuncs.emplace_back(
        base->registerDrawFunction(
            shadowRenderStage,
            SubPass::ID(0),
            pipelineInfo.determineShadowPipeline(),
            makeShadowDrawFunction(drawInfo)
        )
    );
}

void trc::DrawableComponentScene::makeRaytracing(
    DrawableID drawable,
    const RayComponentCreateInfo& createInfo)
{
    auto geoId = createInfo.geo;
    auto geo = geoId.getDeviceDataHandle();
    if (!geo.hasAccelerationStructure())
    {
        log::warn << log::here() << ": Tried to create a ray tracing component for drawable "
                  << ui32{drawable} << ", but the associated geometry " << geoId.getMetadata().name
                  << " does not have an acceleration structure."
                  << " The ray tracing component will not be created.";
        return;
    }

    storage.add<RayComponent>(drawable, RayComponent{
        .modelMatrix = createInfo.transformation,
        .geo = geo,
        .materialIndex = 0,
        .instanceDataIndex = {} // Set by ComponentTraits::onCreate
    });
}

auto trc::DrawableComponentScene::makeAnimationEngine(DrawableID drawable, RigID rig)
    -> AnimationEngine&
{
    return storage.add<AnimationComponent>(drawable, rig.getDeviceDataHandle()).engine;
}

auto trc::DrawableComponentScene::makeNode(DrawableID drawable)
    -> Node&
{
    return storage.add<NodeComponent>(drawable).node;
}

bool trc::DrawableComponentScene::hasRaytracing(DrawableID drawable) const
{
    return storage.has<RayComponent>(drawable);
}

bool trc::DrawableComponentScene::hasAnimation(DrawableID drawable) const
{
    return storage.has<AnimationComponent>(drawable);
}

bool trc::DrawableComponentScene::hasNode(DrawableID drawable) const
{
    return storage.has<NodeComponent>(drawable);
}

auto trc::DrawableComponentScene::getNode(DrawableID drawable) -> Node&
{
    auto result = storage.tryGet<NodeComponent>(drawable);
    if (result.has_value()) {
        return result.value()->node;
    }

    throw std::out_of_range("[In DrawableComponentScene::getNode]: Drawable does not have a"
                            " node component!");
}

auto trc::DrawableComponentScene::getAnimationEngine(DrawableID drawable) -> AnimationEngine&
{
    auto result = storage.tryGet<AnimationComponent>(drawable);
    if (result.has_value()) {
        return result.value()->engine;
    }

    throw std::out_of_range("[In DrawableComponentScene::getAnimationEngine]: Drawable does not"
                            " have an animation component!");
}



auto trc::DrawableComponentScene::InternalStorage::allocateRayInstance(RayInstanceData data) -> ui32
{
    const ui32 index = rayInstanceIdPool.generate();
    rayInstances[index] = data;
    return index;
}

void trc::DrawableComponentScene::InternalStorage::freeRayInstance(ui32 index)
{
    rayInstanceIdPool.free(index);
}
