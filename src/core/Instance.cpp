#include "trc/core/Instance.h"

#include "trc/base/Device.h"
#include "trc/base/Logging.h"
#include "trc/base/VulkanInstance.h"
#include "trc/core/Window.h"



auto trc::makeDefaultTorchVulkanInstance(const std::string& appName, ui32 appVersion)
    -> u_ptr<VulkanInstance>
{
    return std::make_unique<VulkanInstance>(
        VulkanInstanceCreateInfo{
            .appName          = appName,
            .appVersion       = appVersion,
            .engineName       = "Torch",
            .engineVersion    = VK_MAKE_VERSION(0, 0, 1),
            .vulkanApiVersion = VK_API_VERSION_1_3,
            .enabledValidationFeatures={}
        }
    );
}



trc::Instance::Instance(const InstanceCreateInfo& info)
    :
    optionalLocalInstance(makeDefaultTorchVulkanInstance()),
    instance(**optionalLocalInstance),
    physicalDevice([instance=this->instance] {
        Surface surface(instance, { .hidden=true });
        return new PhysicalDevice(instance, surface.getVulkanSurface());
    }())
{
    auto [dev, rayFeatures] = makeDevice(info, *physicalDevice);
    device = std::move(dev);
    hasRayTracingFeatures = rayFeatures;

    dynamicLoader = { instance, vkGetInstanceProcAddr, **device, vkGetDeviceProcAddr };
}

trc::Instance::Instance(const InstanceCreateInfo& info, vk::Instance _instance)
    :
    // Create a new VkInstance if the _instance argument is VK_NULL_HANDLE
    optionalLocalInstance(nullptr),
    instance(_instance),
    physicalDevice([instance=this->instance] {
        Surface surface(instance, { .hidden=true });
        return new PhysicalDevice(instance, surface.getVulkanSurface());
    }())
{
    auto [dev, rayFeatures] = makeDevice(info, *physicalDevice);
    device = std::move(dev);
    hasRayTracingFeatures = rayFeatures;

    dynamicLoader = { instance, vkGetInstanceProcAddr, **device, vkGetDeviceProcAddr };
}

trc::Instance::~Instance() noexcept
{
    try {
        getDevice()->waitIdle();
    }
    catch (const std::runtime_error& err) {
        log::warn << log::here() << ": " << err.what();
    }
}

auto trc::Instance::getVulkanInstance() const -> vk::Instance
{
    return instance;
}

auto trc::Instance::getPhysicalDevice() -> PhysicalDevice&
{
    return *physicalDevice;
}

auto trc::Instance::getPhysicalDevice() const -> const PhysicalDevice&
{
    return *physicalDevice;
}

auto trc::Instance::getDevice() -> Device&
{
    return *device;
}

auto trc::Instance::getDevice() const -> const Device&
{
    return *device;
}

auto trc::Instance::getDL() -> VulkanDispatchLoaderDynamic&
{
    return dynamicLoader;
}

auto trc::Instance::getDL() const -> const VulkanDispatchLoaderDynamic&
{
    return dynamicLoader;
}

auto trc::Instance::makeWindow() -> u_ptr<Window>
{
    return makeWindow({});
}

auto trc::Instance::makeWindow(const WindowCreateInfo& info) -> u_ptr<Window>
{
    return std::make_unique<Window>(*this, info);
}

bool trc::Instance::hasRayTracing() const
{
    return hasRayTracingFeatures;
}

auto trc::Instance::makeDevice(
    const InstanceCreateInfo& info,
    const PhysicalDevice& physicalDevice)
    -> std::pair<u_ptr<Device>, bool>
{

    void* deviceFeatureChain{ nullptr };
    auto extensions = info.deviceExtensions;

    // Ray tracing device features
    auto features = physicalDevice.physicalDevice.getFeatures2<
        vk::PhysicalDeviceFeatures2,

        // Misc
        vk::PhysicalDeviceTimelineSemaphoreFeatures,  // Vulkan 1.2

        // Ray tracing
        vk::PhysicalDeviceBufferDeviceAddressFeatures,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR
    >();

    // Append user provided features to the end
    void* userPtr = info.deviceFeatures.getPNext();
    features.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>().setPNext(userPtr);

    ///////////////////////////////
    // Test for ray tracing support
    auto as = features.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
    auto ray = features.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();

    // Logging
    {
        log::info << "Querying ray tracing support:" << std::boolalpha;
        log::info << "   Acceleration structure: " << (bool)as.accelerationStructure;
        log::info << "   Acceleration structure host commands: " << (bool)as.accelerationStructureHostCommands;
        log::info << "   Ray Tracing pipeline: " << (bool)ray.rayTracingPipeline;
        log::info << "   Trace rays indirect: " << (bool)ray.rayTracingPipelineTraceRaysIndirect;
        log::info << "   Ray traversal primitive culling: " << (bool)ray.rayTraversalPrimitiveCulling;
    }

    const bool rayTracingSupported = as.accelerationStructure && ray.rayTracingPipeline;
    const bool enableRayTracing = info.enableRayTracing && rayTracingSupported;
    if (!enableRayTracing)
    {
        features.unlink<vk::PhysicalDeviceBufferDeviceAddressFeatures>();
        features.unlink<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
        features.unlink<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();
    }
    else
    {
        // Add device extensions
        extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }

    // Require remaining features from device
    deviceFeatureChain = features.get<vk::PhysicalDeviceFeatures2>().pNext;

    return {
        std::make_unique<Device>(physicalDevice, extensions, deviceFeatureChain),
        enableRayTracing
    };
}
