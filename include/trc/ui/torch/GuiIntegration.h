#pragma once

#include <mutex>
#include <future>

#include <vkb/Buffer.h>
#include <vkb/Image.h>
#include <vkb/util/Timer.h>

#include "RenderStage.h"
#include "RenderPass.h"
#include "Pipeline.h"
#include "ui/Window.h"
#include "ui/torch/DrawImplementations.h"

namespace trc
{
    class TorchWindowInformationProvider : public ui::WindowInformationProvider
    {
    public:
        explicit TorchWindowInformationProvider(const vkb::Swapchain& swapchain)
            : swapchain(swapchain)
        {}

        auto getSize() -> vec2 override
        {
            return { swapchain.getImageExtent().width, swapchain.getImageExtent().height };
        }

    private:
        const vkb::Swapchain& swapchain;
    };

    /**
     * Access to static render stage
     */
    extern auto getGuiRenderStage() -> RenderStageType::ID;

    /**
     * Render a GUI root to an image
     */
    class GuiRenderer
    {
    public:
        explicit GuiRenderer(ui::Window& window);

        void render();

        auto getRenderPass() const -> vk::RenderPass;
        auto getOutputImage() const -> vk::Image;

    private:
        ui::Window* window;

        vk::Queue renderQueue;
        vk::UniqueCommandPool cmdPool;
        vk::UniqueCommandBuffer cmdBuf;

        vk::UniqueRenderPass renderPass;
        vkb::Image outputImage;
        vk::UniqueImageView outputImageView;
        vk::UniqueFramebuffer framebuffer;

        ui_impl::DrawCollector collector;
    };

    /**
     * Render pass that integrates the gui into Torch's render pipeline.
     */
    class GuiRenderPass : public RenderPass
    {
    public:
        GuiRenderPass(ui::Window& window, vkb::FrameSpecificObject<vk::Image> renderTargets);
        ~GuiRenderPass();

        void begin(vk::CommandBuffer, vk::SubpassContents) override;
        void end(vk::CommandBuffer) override;

    private:
        GuiRenderer renderer;
        std::mutex renderLock;
        std::thread renderThread;
        bool stopRenderThread{ false };

        vkb::FrameSpecificObject<vk::Image> renderTargets;

    };
} // namespace trc
