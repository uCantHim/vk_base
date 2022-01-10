#include "Window.h"

#include <vkb/Device.h>



trc::Window::Window(Instance& instance, WindowCreateInfo info)
    :
    vkb::Swapchain(
        instance.getDevice(),
        vkb::makeSurface(
            instance.getVulkanInstance(),
            { .windowSize={ info.size.x, info.size.y }, .windowTitle=info.title }
        ),
        // Swapchain create info
        [&] {
            // Always specify the storage bit
            info.swapchainCreateInfo.imageUsage |= vk::ImageUsageFlagBits::eStorage;

            // Additional flags currently only used for ray tracing
            if (instance.hasRayTracing()) {
                info.swapchainCreateInfo.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
            }
            return info.swapchainCreateInfo;
        }()
    ),
    instance(&instance),
    renderer(new Renderer(*this)),
    recreateListener(
        vkb::on<vkb::SwapchainRecreateEvent>([this](auto&) {
            // Create a new renderer to avoid the still mysterious crash on recreate
            renderer->waitForAllFrames();
            renderer.reset(new Renderer(*this));
        })
    )
{
}

void trc::Window::drawFrame(const DrawConfig& drawConfig)
{
    renderer->drawFrame(drawConfig);
}

auto trc::Window::getInstance() -> Instance&
{
    return *instance;
}

auto trc::Window::getInstance() const -> const Instance&
{
    return *instance;
}

auto trc::Window::getDevice() -> vkb::Device&
{
    return instance->getDevice();
}

auto trc::Window::getDevice() const -> const vkb::Device&
{
    return instance->getDevice();
}

auto trc::Window::getSwapchain() -> vkb::Swapchain&
{
    return *this;
}

auto trc::Window::getSwapchain() const -> const vkb::Swapchain&
{
    return *this;
}

auto trc::Window::getRenderer() -> Renderer&
{
    return *renderer;
}
