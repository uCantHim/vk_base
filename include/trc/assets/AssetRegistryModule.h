#pragma once

#include "trc/VulkanInclude.h"
#include "trc/assets/AssetBase.h"
#include "trc/assets/AssetSource.h"

namespace trc
{
    class AssetRegistry;
    class FrameRenderState;

    namespace internal
    {
        /**
         * @brief The type-agnostic part of an asset module's interface
         *
         * Used internally. To implement an asset module, derive from the CRTP
         * template `AssetRegistryModuleInterface<T>` instead.
         */
        class AssetRegistryModuleInterfaceBase
        {
        public:
            AssetRegistryModuleInterfaceBase() = default;
            virtual ~AssetRegistryModuleInterfaceBase() = default;

            virtual void init(AssetRegistry& /*parent*/) {}
            virtual void update(vk::CommandBuffer cmdBuf, FrameRenderState& state) = 0;

        protected:
            auto getAssetRegistry() -> AssetRegistry&
            {
                if (parent == nullptr) {
                    throw std::runtime_error("Asset module not initialized:"
                                             " parent registry is nullptr.");
                }
                return *parent;
            }

        private:
            friend AssetRegistry;
            AssetRegistry* parent{ nullptr };
        };
    } // namespace internal

    /**
     * @brief Type-specific part of an asset module's interface
     */
    template<AssetBaseType T>
    class AssetRegistryModuleInterface : public internal::AssetRegistryModuleInterfaceBase
    {
    public:
        using LocalID = typename AssetBaseTypeTraits<T>::LocalID;
        using Handle = typename AssetBaseTypeTraits<T>::Handle;

        /**
         * @brief Initialize the asset module
         *
         * Called once when the module is registered at an asset registry.
         *
         * @param AssetRegistry& parent The asset registry at which the module
         *        is being registered. Asset modules can always access this even
         *        outside of the `init` function via `getAssetRegistry()`.
         */
        virtual void init(AssetRegistry& /*parent*/) {}

        /**
         * @brief Perform updates on device resources
         *
         * Called once per frame in render stage `stages::resourceUpdate`.
         */
        virtual void update(vk::CommandBuffer cmdBuf, FrameRenderState& state) = 0;

        /**
         * @brief Register an asset at the asset module
         *
         * The asset module is free to load and process asset data from the
         * source object lazily at a later point in time.
         *
         * @param AssetSource source An object that can load asset data on
         *                           request. Is never `nullptr`.
         *
         * @return LocalID An ID that identifies the newly created asset.
         */
        virtual auto add(u_ptr<AssetSource<T>> source) -> LocalID = 0;

        /**
         * @brief Remove an asset from the asset module
         *
         * Destroys an asset's device data and removes the asset from the asset
         * module. The asset's ID does not point to any object afterwards and is
         * available to be reused for new assets.
         *
         * @param LocalID id The asset to remove from the asset module.
         */
        virtual void remove(LocalID id) = 0;

        /**
         * @brief Access an asset
         *
         * The returned handle should give access to all functionality of the
         * asset. The asset is expected to be loaded and to have its device
         * repesentation set up at the time this handle is used.
         *
         * @param LocalID id The asset to which a handle is requested.
         */
        virtual auto getHandle(LocalID id) -> Handle = 0;
    };
} // namespace trc
