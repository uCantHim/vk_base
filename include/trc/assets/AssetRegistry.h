#pragma once

#include <concepts>

#include "trc/Types.h"
#include "trc/assets/AssetBase.h"
#include "trc/assets/AssetRegistryModuleStorage.h"
#include "trc/assets/AssetSource.h"

namespace trc
{
    class AssetRegistry
    {
    public:
        template<AssetBaseType T>
        using LocalID = typename AssetBaseTypeTraits<T>::LocalID;
        template<AssetBaseType T>
        using Handle = typename AssetBaseTypeTraits<T>::Handle;

        AssetRegistry() = default;

        /**
         * @brief Add an asset to the registry
         */
        template<AssetBaseType T>
        auto add(u_ptr<AssetSource<T>> source) -> LocalID<T>;

        /**
         * @brief Retrive a handle to an asset's device data
         *
         * @throw std::out_of_range if no module for `T` has been registered
         *        previously.
         */
        template<AssetBaseType T>
        auto get(LocalID<T> key) -> Handle<T>
        {
            return getModule<T>().getHandle(key);
        }

        /**
         * @brief Remove an asset from the registry
         *
         * @throw std::out_of_range if no module for `T` has been registered
         *                          previously.
         */
        template<AssetBaseType T>
        void remove(LocalID<T> id);

        /**
         * @brief Register a module at the asset registry
         *
         * A module is an object that implements the device representation for
         * a single type of asset. An example is the GeometryRegistry for
         * geometries.
         *
         * Asset management is delegated to single modules, while the asset
         * registry manages the modules and provides central access to them.
         *
         * @throw std::out_of_range if a module for `T` has already been
         *                          registered.
         */
        template<AssetBaseType T>
            requires std::derived_from<AssetRegistryModule<T>, AssetRegistryModuleInterface<T>>
        void addModule(u_ptr<AssetRegistryModule<T>> assetModuleImpl);

        /**
         * @throw std::out_of_range
         */
        template<AssetBaseType T>
        auto getModule() -> AssetRegistryModule<T>&;

        /**
         * @throw std::out_of_range
         */
        template<AssetBaseType T>
        auto getModule() const -> const AssetRegistryModule<T>&;

        /**
         * @return bool True if a module for asset type `T` is currently
         *              registered at the AssetRegistry, false otherwise.
         */
        template<AssetBaseType T>
        bool hasModule() const noexcept;

        /**
         * Allows registered modules to perform updates to device data as needed.
         */
        void updateDeviceResources(vk::CommandBuffer cmdBuf, FrameRenderState& frameState) {
            modules.update(cmdBuf, frameState);
        }

    private:
        AssetRegistryModuleStorage modules;
    };



    template<AssetBaseType T>
    inline auto AssetRegistry::add(u_ptr<AssetSource<T>> source) -> LocalID<T>
    {
        return getModule<T>().add(std::move(source));
    }

    template<AssetBaseType T>
    inline void AssetRegistry::remove(LocalID<T> id)
    {
        getModule<T>().remove(id);
    }

    template<AssetBaseType T>
        requires std::derived_from<AssetRegistryModule<T>, AssetRegistryModuleInterface<T>>
    inline void AssetRegistry::addModule(u_ptr<AssetRegistryModule<T>> assetModuleImpl)
    {
        // Initialize module
        assetModuleImpl->parent = this;
        assetModuleImpl->init(*this);

        // Store module
        modules.addModule<T>(std::move(assetModuleImpl));
    }

    template<AssetBaseType T>
    inline auto AssetRegistry::getModule() -> AssetRegistryModule<T>&
    {
        return modules.getModule<T>();
    }

    template<AssetBaseType T>
    inline auto AssetRegistry::getModule() const -> const AssetRegistryModule<T>&
    {
        return modules.getModule<T>();
    }

    template<AssetBaseType T>
    inline bool AssetRegistry::hasModule() const noexcept
    {
        return modules.hasModule<T>();
    }
} // namespace trc
