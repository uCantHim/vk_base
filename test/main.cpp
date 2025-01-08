#include <iostream>
#include <thread>

#include <glm/gtc/random.hpp>

#include <trc/Torch.h>
#include <trc/base/ImageUtils.h>
#include <trc/base/event/Event.h>
#include <trc/drawable/DrawableScene.h>
#include <trc/particle/Particle.h>
#include <trc/text/Text.h>

using namespace std::chrono_literals;
using namespace trc::basic_types;

void run()
{
    trc::Keyboard::init();
    trc::Mouse::init();

    auto torch = trc::initFull();
    auto& ar = torch->getAssetManager();
    auto& instance = torch->getInstance();
    const auto& device = instance.getDevice();

    auto camera = std::make_shared<trc::Camera>(1.0f, 45.0f, 0.1f, 100.0f);
    camera->lookAt({ 0.0f, 2.0f, 5.0f }, vec3(0, 0.5f, -1.0f ), { 0, 1, 0 });
    torch->getWindow().addCallbackOnResize([&camera](trc::Swapchain& swapchain) {
        camera->setAspect(swapchain.getAspectRatio());
    });
    camera->setAspect(torch->getWindow().getAspectRatio());

    // ------------------
    // Random test things

    auto grassGeoIndex = ar.create(trc::loadGeometry(TRC_TEST_ASSET_DIR"/grass_lowpoly.fbx"));
    auto treeGeoIndex = ar.create(trc::loadGeometry(TRC_TEST_ASSET_DIR"/tree_lowpoly.fbx"));

    auto skeletonGeoIndex = ar.create(trc::loadGeometry(TRC_TEST_ASSET_DIR"/skeleton.fbx"));
    auto hoodedBoiGeoIndex = ar.create(trc::loadGeometry(TRC_TEST_ASSET_DIR"/hooded_boi.fbx"));
    auto lindaGeoIndex = ar.create(trc::loadGeometry(TRC_TEST_ASSET_DIR"/Female_Character.fbx"));

    auto lindaDiffTexIdx = ar.create(
        trc::loadTexture(TRC_TEST_ASSET_DIR"/Female_Character.png")
    );

    auto grassImgIdx = ar.create(
        trc::loadTexture(TRC_TEST_ASSET_DIR"/grass_billboard_001.png")
    );
    auto stoneTexIdx = ar.create(
        trc::loadTexture(TRC_TEST_ASSET_DIR"/rough_stone_wall.tif")
    );
    auto stoneNormalTexIdx = ar.create(
        trc::loadTexture(TRC_TEST_ASSET_DIR"/rough_stone_wall_normal.tif")
    );

    auto matIdx = ar.create(trc::makeMaterial(trc::SimpleMaterialData{
        .specularCoefficient = 1.0f,
        .roughness = 0.05f,
        .albedoTexture = grassImgIdx,
        .normalTexture = stoneNormalTexIdx,
    }));

    auto mapImport = trc::loadAssets(TRC_TEST_ASSET_DIR"/map.fbx");
    auto mapMat = mapImport.meshes[0].materials[0].data;
    mapMat.specularCoefficient = 1.0f;
    mapMat.albedoTexture = stoneTexIdx;
    mapMat.normalTexture = stoneNormalTexIdx;
    mapMat.roughness = 0.4f;
    mapMat.opacity = 1.0f;
    auto mapMatIndex = ar.create(trc::makeMaterial(mapMat));

    trc::SimpleMaterialData treeMat{
        .color=vec4(0, 1, 0, 1),
        .roughness=0.85f,
    };
    auto treeMatIdx = ar.create(trc::makeMaterial(treeMat));

    // ------------------

    auto scene = std::make_shared<trc::DrawableScene>();

    auto grass = scene->makeDrawable({ grassGeoIndex, matIdx });
    grass->setScale(0.1f).rotateX(glm::radians(-90.0f)).translateX(0.5f);

    // Animated skeleton
    trc::Node skeletonNode;
    skeletonNode.scale(0.02f).translateZ(1.2f)
                .translate(1.0f, 0.0f, 0.0f)
                .rotateY(-glm::half_pi<float>());
    scene->getRoot().attach(skeletonNode);

    const trc::DrawableCreateInfo skelCreateInfo{ skeletonGeoIndex, matIdx };
    std::vector<s_ptr<trc::DrawableObj>> skeletons;
    for (int i = 0; i < 50; i++)
    {
        auto& inst = *skeletons.emplace_back(scene->makeDrawable(skelCreateInfo));
        const float angle = glm::two_pi<float>() / 50 * i;
        inst.scale(0.02f).translateZ(1.2f)
            .translate(glm::cos(angle), 0.0f, glm::sin(angle))
            .rotateY(-glm::half_pi<float>() - angle);
        scene->getRoot().attach(inst);
        if (auto anim = inst.getAnimationEngine()) {
            anim.value()->playAnimation(0);
        }
    }

    // Hooded boi
    auto hoodedBoi = scene->makeDrawable({ hoodedBoiGeoIndex, mapMatIndex });
    hoodedBoi->setScale(0.2f).translate(1.0f, 0.6f, -7.0f);
    if (auto anim = hoodedBoi->getAnimationEngine()) {
        anim.value()->playAnimation(0);
    }

    // Linda
    auto lindaMatIdx = ar.create(trc::makeMaterial(trc::SimpleMaterialData{
        .specularCoefficient = 0.0f,
        .albedoTexture = lindaDiffTexIdx
    }));

    auto linda = scene->makeDrawable({ lindaGeoIndex, lindaMatIdx });
    linda->setScale(0.3f).translateX(-1.0f);
    if (auto anim = linda->getAnimationEngine()) {
        anim.value()->playAnimation(0);
    }

    // Images
    auto planeGeo = ar.create(trc::makePlaneGeo());
    auto transparentImg = ar.create(trc::makeMaterial(trc::SimpleMaterialData{
        .opacity=0.0f,  // Enable transparency
        .albedoTexture=ar.create(trc::loadTexture(TRC_TEST_ASSET_DIR"/standard_model.png")),
    }));
    auto opaqueImg = ar.create(trc::makeMaterial(trc::SimpleMaterialData{
        .albedoTexture=ar.create(trc::loadTexture(TRC_TEST_ASSET_DIR"/lena.png"))
    }));
    auto img = scene->makeDrawable({ planeGeo, transparentImg });
    img->translate(-5, 1, -3).rotate(glm::radians(90.0f), glm::radians(30.0f), 0.0f).scale(2);
    auto img2 = scene->makeDrawable({ planeGeo, opaqueImg });
    img2->translate(-5.001f, 1, -3.001f).rotate(glm::radians(90.0f), glm::radians(30.0f), 0.0f).scale(2);

    // Generated plane geo
    auto myPlaneGeoIndex = ar.create(trc::makePlaneGeo(20.0f, 20.0f, 20, 20));
    auto plane = scene->makeDrawable({ myPlaneGeoIndex, mapMatIndex });

    auto sunLight = scene->getLights().makeSunLight(vec3(1.0f), vec3(1.0f, -1.0f, -1.5f));
    [[maybe_unused]]
    auto ambientLight = scene->getLights().makeAmbientLight(vec3(0.15f));
    [[maybe_unused]]
    auto pointLight = scene->getLights().makePointLight(vec3(1, 1, 0), vec3(2, 0.5f, 0.5f), 0.4f);

    // Sun light
    auto sunShadow = scene->getLights().enableShadow(sunLight, uvec2(2048, 2048));
    sunShadow->getCamera().makeOrthogonal(-5.0f, 5.0f, -5.0f, 5.0f, -50.0f, 5.0f);
    scene->getRoot().attach(sunShadow->getCamera());

    // Instanced trees
    constexpr trc::ui32 NUM_TREES = 200;
    std::vector<s_ptr<trc::DrawableObj>> trees;
    for (ui32 i = 0; i < NUM_TREES; i++)
    {
        auto& tree = *trees.emplace_back(scene->makeDrawable({ treeGeoIndex, treeMatIdx }));
        tree.setScale(0.1f).rotateX(glm::radians(-90.0f))
            .setTranslationX(-3.0f + static_cast<float>(i % 14) * 0.5f)
            .setTranslationZ(-1.0f - (static_cast<float>(i) / 14.0f) * 0.4f);
    }

    // Particles
    trc::ParticleCollection particleCollection(instance, 10000);
    particleCollection.attachToScene(scene->getRasterModule());
    for (int i = 0; i < 1000; i++)
    {
        trc::Particle particle;
        particle.phys.position = glm::linearRand(vec3(-2, 0, -2), vec3(2, 2, 2));
        particle.phys.linearVelocity = vec3(0, 0.05f, 0);
        particle.phys.linearAcceleration = vec3(0, 0.01f, 0);
        particle.phys.angularVelocity = glm::radians(30.0f);
        particle.phys.scaling = vec3(0.15f);
        particle.phys.lifeTime = glm::linearRand(1000.0f, 6000.0f);
        particle.material.texture = grassImgIdx.getDeviceDataHandle().getDeviceIndex();
        particleCollection.addParticle(particle);
    }

    auto particleImg = ar.create(trc::loadTexture(TRC_TEST_ASSET_DIR"/yellowlight.png")).getDeviceDataHandle();
    trc::ParticleSpawn spawn(particleCollection);
    for (int i = 0; i < 50; i++)
    {
        trc::Particle p;
        p.phys.linearVelocity = glm::linearRand(vec3(0.2, 0.2, 0.2), vec3(1.5f, 1.5f, 1.0f));
        p.phys.linearAcceleration = vec3(0, -2.0f, 0);
        p.phys.scaling = vec3(0.2f);
        p.phys.lifeTime = 3000.0f;
        p.material.texture = particleImg.getDeviceIndex();
        p.material.blending = trc::ParticleMaterial::BlendingType::eAlphaBlend;
        spawn.addParticle(p);
    }
    spawn.spawnParticles();


    bool running{ true };
    trc::EventHandler<trc::SwapchainCloseEvent>::addListener(
        [&running](const trc::SwapchainCloseEvent&) { running = false; }
    );

    std::thread particleUpdateThread([&]() {
        trc::Timer timer;
        while (running) {
            particleCollection.update(timer.reset());
        }
    });
    std::thread particleSpawnThread([&]() {
        while (running)
        {
            spawn.spawnParticles();
            std::this_thread::sleep_for(1s);
        }
    });


    // Generated cube geo
    auto cubeGeoIdx = ar.create<trc::Geometry>({ trc::makeCubeGeo() });
    auto cubeMatIdx = ar.create<trc::Material>(
        trc::makeMaterial({ .color={ 0.3, 0.3, 1 }, .opacity=0.5f })
    );
    auto cube = scene->makeDrawable({ cubeGeoIdx, cubeMatIdx });
    cube->translate(1.5f, 0.7f, 1.5f).setScale(0.3f);

    std::thread cubeRotateThread([&cube, &running]() {
        while (running)
        {
            std::this_thread::sleep_for(10ms);
            cube->rotateY(glm::radians(0.5f));
        }
    });

    // Thing at cursor
    auto cursorCubeMat = ar.create(trc::makeMaterial(
        trc::SimpleMaterialData{ .color=vec3(1, 1, 0), .opacity=0.3f }
    ));
    auto cursor = scene->makeDrawable({ ar.create(trc::makeSphereGeo(16, 8)), cursorCubeMat });
    cursor->scale(0.15f);

    // Text
    auto font = ar.create(trc::loadFont(TRC_TEST_FONT_DIR"/gil.ttf", 64));
    trc::Text text{ instance, font.getDeviceDataHandle() };
    text.rotateY(0.5f).translate(-1.3f, 0.0f, -0.1f);
    text.print("Hello World!");
    text.attachToScene(scene->getRasterModule());

    // The renderable viewport
    auto vp = torch->makeFullscreenViewport(camera, scene);

    trc::Timer timer;
    trc::Timer frameTimer;
    uint32_t frames{ 0 };
    while (running)
    {
        trc::pollEvents();

        const float frameTime = frameTimer.reset();
        scene->update(frameTime);
        //cursor->setTranslation(torch->getRenderConfig().getMouseWorldPos(camera));

        torch->drawFrame(vp);

        frames++;
        if (timer.duration() >= 1000)
        {
            std::cout << frames << " frames in one second\n";
            frames = 0;
            timer.reset();
        }
    }

    torch->waitForAllFrames();

    particleUpdateThread.join();
    particleSpawnThread.join();
    cubeRotateThread.join();
}

int main()
{
    // A function for extra scope
    run();

    trc::terminate();

    std::cout << " --- Done\n";
    return 0;
}
