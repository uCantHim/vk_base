#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <trc/RasterSceneBase.h>
using namespace trc;

/**
 * Count the number of elements in a range. Usable with generators.
 */
auto size(std::ranges::range auto&& r)
{
    size_t n{ 0 };
    for (auto&& _ : r) ++n;
    return n;
}

/**
 * Call all draw functions in a scene for a specific set of render stage,
 * subpass, and pipeline.
 *
 * Passes `nullptr` as `DrawEnvironment::currentPipeline`!
 */
void invokeDrawFunctions(RasterSceneBase& scene,
                         RenderStage::ID stage,
                         SubPass::ID subpass,
                         Pipeline::ID pipeline)
{
    const DrawEnvironment env{ .currentPipeline=nullptr };
    for (const DrawableFunction& func : scene.iterDrawFunctions(stage, subpass, pipeline)) {
        func(env, vk::CommandBuffer{});
    }
}



TEST(RasterSceneBaseTest, ConstructDestruct)
{
    RasterSceneBase scene;
}

TEST(RasterSceneBaseTest, DrawFunctionExecution)
{
    RasterSceneBase scene;

    std::vector<RenderStage::ID> s{ RenderStage::ID(0), RenderStage::ID(1), RenderStage::ID(2) };
    std::vector<SubPass::ID>     u{ SubPass::ID(0), SubPass::ID(1), SubPass::ID(2) };
    std::vector<Pipeline::ID>    p{ Pipeline::ID(0), Pipeline::ID(5), Pipeline::ID(42),
                                    Pipeline::ID(7777), Pipeline::ID(108) };

    std::vector<RasterSceneBase::RegistrationID> regs;
    regs.reserve(1000);

    size_t numExecuted{ 0 };
    auto func = [&](auto&&, auto&&) {
        ++numExecuted;
    };

    for (int i = 0; i < 9; ++i)
    {
        for (int j = 0; j < 9; ++j)
        {
            for (int k = 0; k < 1000; ++k)
            {
                auto id = scene.registerDrawFunction(s[i % 3], u[j % 3], p[k % 5], func);
                regs.emplace_back(std::move(id));
            }
        }
    }

    ASSERT_EQ(size(scene.iterPipelines(s[0], u[0])), 5);
    ASSERT_EQ(size(scene.iterPipelines(s[0], u[1])), 5);
    ASSERT_EQ(size(scene.iterPipelines(s[0], u[2])), 5);
    ASSERT_EQ(size(scene.iterPipelines(s[1], u[0])), 5);
    ASSERT_EQ(size(scene.iterPipelines(s[1], u[1])), 5);
    ASSERT_EQ(size(scene.iterPipelines(s[1], u[2])), 5);
    ASSERT_EQ(size(scene.iterPipelines(s[2], u[0])), 5);
    ASSERT_EQ(size(scene.iterPipelines(s[2], u[1])), 5);
    ASSERT_EQ(size(scene.iterPipelines(s[2], u[2])), 5);

    invokeDrawFunctions(scene, s[1], u[0], p[3]);
    ASSERT_EQ(numExecuted, (3 * 3 * 1000) / 5);

    numExecuted = 0;
    invokeDrawFunctions(scene, s[0], u[2], p[1]);
    ASSERT_EQ(numExecuted, (3 * 3 * 1000) / 5);

    ASSERT_THROW(
        invokeDrawFunctions(scene, RenderStage::ID(200), u[2], p[0]),
        std::out_of_range
    );

    for (auto id : regs) {
        scene.unregisterDrawFunction(id);
    }

    ASSERT_TRUE(size(scene.iterPipelines(s[0], u[0])) == 0);
    ASSERT_TRUE(size(scene.iterPipelines(s[0], u[1])) == 0);
    ASSERT_TRUE(size(scene.iterPipelines(s[0], u[2])) == 0);
    ASSERT_TRUE(size(scene.iterPipelines(s[1], u[0])) == 0);
    ASSERT_TRUE(size(scene.iterPipelines(s[1], u[1])) == 0);
    ASSERT_TRUE(size(scene.iterPipelines(s[1], u[2])) == 0);
    ASSERT_TRUE(size(scene.iterPipelines(s[2], u[0])) == 0);
    ASSERT_TRUE(size(scene.iterPipelines(s[2], u[1])) == 0);
    ASSERT_TRUE(size(scene.iterPipelines(s[2], u[2])) == 0);

    numExecuted = 0;
    invokeDrawFunctions(scene, s[0], u[2], p[1]);
    ASSERT_EQ(numExecuted, 0);
}

TEST(RasterSceneBaseTest, ThreadSafeRegistration)
{
    RasterSceneBase scene;

    std::vector<RenderStage::ID> s{ RenderStage::ID(0), RenderStage::ID(2), RenderStage::ID(4) };
    std::vector<SubPass::ID>     u{ SubPass::ID(0), SubPass::ID(1), SubPass::ID(2) };
    std::vector<Pipeline::ID>    p{ Pipeline::ID(0), Pipeline::ID(1), Pipeline::ID(2),
                                    Pipeline::ID(3), Pipeline::ID(4) };

    size_t numInvocations{ 0 };
    auto func = [&](auto&&, auto&&){ ++numInvocations; };

    // Register draw functions from multiple threads
    std::vector<std::thread> threads;
    for (size_t i = 0; i < 100; ++i)
    {
        threads.emplace_back([&]{
            for (auto s_ : s) {
                for (auto u_ : u) {
                    for (auto p_ : p) {
                        scene.registerDrawFunction(s_, u_, p_, func);
                    }
                }
            }
        });
    }
    for (auto& t : threads) t.join();

    ASSERT_EQ(size(scene.iterPipelines(s[0], u[0])), 5);

    invokeDrawFunctions(scene, s[0], u[0], p[0]);
    ASSERT_EQ(numInvocations, 100);
}
