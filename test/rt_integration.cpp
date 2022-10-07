#include <future>
#include <iostream>

#include <vkb/Barriers.h>
#include <vkb/ImageUtils.h>
#include <trc_util/Timer.h>
#include <trc/DescriptorSetUtils.h>
#include <trc/PipelineDefinitions.h>
#include <trc/TopLevelAccelerationStructureBuildPass.h>
#include <trc/Torch.h>
#include <trc/TorchResources.h>
#include <trc/ray_tracing/RayTracing.h>
#include <trc/ray_tracing/RaygenDescriptor.h>
using namespace trc::basic_types;

using trc::rt::BLAS;
using trc::rt::TLAS;

void run()
{
    auto torch = trc::initFull(
        trc::InstanceCreateInfo{ .enableRayTracing = true },
        trc::WindowCreateInfo{
            .swapchainCreateInfo={ .imageUsage=vk::ImageUsageFlagBits::eTransferDst }
        }
    );
    auto& instance = torch->getInstance();
    auto& device = instance.getDevice();
    auto& window = torch->getWindow();
    auto& assets = torch->getAssetManager();

    auto scene = std::make_unique<trc::Scene>();

    // Camera
    trc::Camera camera;
    camera.lookAt({ 0, 2, 4 }, { 0, 0, 0 }, { 0, 1, 0 });
    auto size = window.getImageExtent();
    camera.makePerspective(float(size.width) / float(size.height), 45.0f, 0.1f, 100.0f);

    // Create some objects
    trc::Drawable sphere(
        trc::DrawableCreateInfo{
            assets.create(trc::makeSphereGeo()),
            assets.create(trc::MaterialData{
                .color=vec4(0.8f, 0.3f, 0.6f, 1),
                .specularKoefficient=vec4(0.3f, 0.3f, 0.3f, 1),
                .reflectivity=1.0f,
            }),
            false, true, true, true
        },
        *scene
    );
    sphere.translate(-1.5f, 0.5f, 1.0f).setScale(0.2f);
    trc::Node sphereNode;
    sphereNode.attach(sphere);
    scene->getRoot().attach(sphereNode);

    trc::Drawable plane({
        assets.create(trc::makePlaneGeo()),
        assets.create(trc::MaterialData{ .reflectivity=0.9f }),
        false, true, true, true
    }, *scene);
    plane.rotate(glm::radians(90.0f), glm::radians(-15.0f), 0.0f)
         .translate(0.5f, 0.5f, -1.0f)
         .setScale(3.0f, 1.0f, 1.7f);

    trc::GeometryID treeGeo = assets.create(trc::loadGeometry(TRC_TEST_ASSET_DIR"/tree_lowpoly.fbx"));
    trc::MaterialID treeMat = assets.create(trc::MaterialData{ .color=vec4(0, 1, 0, 1) });
    trc::Drawable tree({ treeGeo, treeMat, false, true, true, true }, *scene);

    tree.rotateX(-glm::half_pi<float>()).setScale(0.1f);

    constexpr float kFloorReflectivity{ 0.2f };
    trc::Drawable floor({
        assets.create(trc::makePlaneGeo(50.0f, 50.0f, 60, 60)),
        assets.create(trc::MaterialData{
            .specularKoefficient=vec4(0.2f),
            .reflectivity=kFloorReflectivity,
            .albedoTexture=assets.create(
                trc::loadTexture(TRC_TEST_ASSET_DIR"/rough_stone_wall.tif")
            ),
            .normalTexture=assets.create(
                trc::loadTexture(TRC_TEST_ASSET_DIR"/rough_stone_wall_normal.tif")
            ),
        }),
        false, true, true, true
    }, *scene);

    auto sun = scene->getLights().makeSunLight(vec3(1.0f), vec3(1, -1, -1), 0.5f);
    auto& shadow = scene->enableShadow(
        sun,
        { .shadowMapResolution=uvec2(2048, 2048) },
        torch->getShadowPool()
    );
    shadow.setProjectionMatrix(glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -20.0f, 10.0f));


    // --- Create the TLAS --- //

    vkb::Buffer instanceBuf(
        device, 500 * sizeof(trc::rt::GeometryInstance),
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
        vkb::DefaultDeviceMemoryAllocator{ vk::MemoryAllocateFlagBits::eDeviceAddress }
    );
    auto data = instanceBuf.map<trc::rt::GeometryInstance*>();
    const size_t numInstances = scene->writeTlasInstances(data);
    instanceBuf.unmap();

    trc::rt::TLAS tlas(instance, numInstances);
    tlas.build(*instanceBuf, numInstances);


    // --- TLAS Update Pass --- //

    auto& renderLayout = torch->getRenderConfig().getLayout();

    trc::TopLevelAccelerationStructureBuildPass tlasBuildPass(instance, tlas);
    tlasBuildPass.setScene(*scene);
    renderLayout.addPass(trc::resourceUpdateStage, tlasBuildPass);


    // --- Ray Tracing Pass --- //

    vkb::FrameSpecific<trc::rt::RayBuffer> rayBuffer{
        window,
        [&](ui32) {
            return trc::rt::RayBuffer(
                device,
                { window.getSize(), vk::ImageUsageFlagBits::eTransferSrc }
            );
        }
    };

    trc::RayTracingPass rayPass(
        instance,
        torch->getRenderConfig(),
        tlas,
        std::move(rayBuffer),
        torch->getRenderTarget()
    );
    renderLayout.addPass(trc::rt::rayTracingRenderStage, rayPass);


    // --- Run the Application --- //

    vkb::on<vkb::KeyPressEvent>([&](auto& e) {
        static bool count{ false };
        if (e.key == vkb::Key::r)
        {
            assets.getModule<trc::Material>().modify(
                floor.getMaterial().getDeviceID(),
                [](auto& mat) {
                    mat.reflectivity = kFloorReflectivity * float(count);
                }
            );
            count = !count;
        }
    });

    trc::Timer timer;
    trc::Timer frameTimer;
    int frames{ 0 };
    while (window.isOpen())
    {
        vkb::pollEvents();

        const float time = timer.reset();
        sphereNode.rotateY(time / 1000.0f * 0.5f);

        scene->update(time);

        window.drawFrame(torch->makeDrawConfig(*scene, camera));

        frames++;
        if (frameTimer.duration() >= 1000.0f)
        {
            std::cout << frames << " FPS\n";
            frames = 0;
            frameTimer.reset();
        }
    }

    window.getRenderer().waitForAllFrames();
}

int main()
{
    run();

    trc::terminate();

    return 0;
}
