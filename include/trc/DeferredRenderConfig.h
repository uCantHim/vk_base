#pragma once

#include "Types.h"
#include "core/Instance.h"
#include "core/RenderConfiguration.h"
#include "core/RenderPass.h"
#include "core/SceneBase.h"

#include "DescriptorProviderWrapper.h"
#include "RenderPassDeferred.h"
#include "RenderPassShadow.h"
#include "RenderDataDescriptor.h"
#include "SceneDescriptor.h"
#include "ShadowPool.h"
#include "AssetRegistry.h"
#include "AnimationDataStorage.h"
#include "text/FontDataStorage.h"

namespace trc
{
    class Window;
    class AssetRegistry;

    /**
     * @brief
     */
    struct DeferredRenderCreateInfo
    {
        const Instance& instance;
        const Window& window;
        AssetRegistry* assetRegistry;

        ui32 maxTransparentFragsPerPixel{ 3 };
    };

    /**
     * @brief
     */
    class DeferredRenderConfig : public RenderConfigCrtpBase<DeferredRenderConfig>
    {
    public:
        /**
         * @brief
         */
        explicit DeferredRenderConfig(const DeferredRenderCreateInfo& info);

        void preDraw(const DrawConfig& draw) override;
        void postDraw(const DrawConfig& draw) override;

        auto getDeferredRenderPass() const -> const RenderPassDeferred&;
        auto getCompatibleShadowRenderPass() const -> vk::RenderPass;

        auto getGlobalDataDescriptorProvider() const -> const DescriptorProviderInterface&;
        auto getSceneDescriptorProvider() const -> const DescriptorProviderInterface&;
        auto getDeferredPassDescriptorProvider() const -> const DescriptorProviderInterface&;
        auto getShadowDescriptorProvider() const -> const DescriptorProviderInterface&;
        auto getAssetDescriptorProvider() const -> const DescriptorProviderInterface&;
        auto getAnimationDataDescriptorProvider() const -> const DescriptorProviderInterface&;

        auto getAssets() -> AssetRegistry&;
        auto getAssets() const -> const AssetRegistry&;
        auto getShadowPool() -> ShadowPool&;
        auto getShadowPool() const -> const ShadowPool&;

    private:
        // Default render passes
        RenderPassDeferred deferredPass;
        RenderPassShadow shadowPass;

        // Descriptors
        GlobalRenderDataDescriptor globalDataDescriptor;
        SceneDescriptor sceneDescriptor;

        // Data & Assets
        AssetRegistry* assetRegistry;
        ShadowPool shadowPool;

        // Final lighting pass stuff
        vkb::DeviceLocalBuffer fullscreenQuadVertexBuffer;
        DrawableExecutionRegistration::ID finalLightingFunc;
    };
} // namespace trc
