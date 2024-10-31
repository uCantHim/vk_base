#pragma once

#include <cassert>

#include <any>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <trc_util/data/IdPool.h>
#include <trc_util/data/SafeVector.h>

#include "trc/assets/AssetBase.h"
#include "trc/assets/AssetRegistry.h"
#include "trc/assets/AssetSource.h"
#include "trc/assets/AssetType.h"

namespace trc
{
    class AssetManagerBase;

    namespace internal {
        struct _AssetIdTypeTag {};
    }

    /**
     * @brief ID for basic metadata that is shared by all asset types
     */
    using AssetID = data::HardTypesafeID<internal::_AssetIdTypeTag, AssetManagerBase, ui32>;

    /**
     * @brief ID for a specific asset type
     */
    template<AssetBaseType T>
    struct TypedAssetID
    {
    public:
        using LocalID = typename AssetBaseTypeTraits<T>::LocalID;
        using Handle = typename AssetBaseTypeTraits<T>::Handle;

        /** Construct a valid ID */
        TypedAssetID(AssetID base, LocalID type, AssetManagerBase& man)
            : uniqueId(base), id(type), manager(&man)
        {}

        operator AssetID() const;

        bool operator==(const TypedAssetID&) const noexcept;
        bool operator!=(const TypedAssetID&) const noexcept = default;

        /**
         * This ID is unique among different asset types.
         *
         * @return AssetID Global ID for type-agnostic asset data
         */
        auto getAssetID() const -> AssetID;

        /**
         * This ID is NOT unique among different asset types.
         *
         * @return AssetID Local ID for type-specific asset data
         */
        auto getDeviceID() const -> LocalID;

        auto get() const -> Handle;
        auto getDeviceDataHandle() const -> Handle;
        auto getMetadata() const -> const AssetMetadata&;

        auto getAssetManager() -> AssetManagerBase&;
        auto getModule() -> AssetRegistryModule<T>&;

    private:
        /** ID that references metadata common to all assets */
        AssetID uniqueId;

        /** ID that references data in the asset type's specific asset registry */
        LocalID id;

        AssetManagerBase* manager;
    };

    /**
     * @brief An exception that is thrown if an AssetID is used in a context
     *        where it is invalid.
     *
     * It is generally not easy to make an AssetID invalid because only the
     * AssetManagerBase can create them, and it only ever creates valid ones.
     * The only case where an AssetID can be invalid is if it is used after the
     * asset which it references was destroyed.
     *
     * # Example
     * ```cpp
     *
     * auto id = assetManager.create<Geometry>(myDataSource);
     * assetManager.destroy(id);
     * try {
     *     assetManager.getHandle(id);
     *     assert(false);
     * }
     * catch (const InvalidAssetIdError&) {
     *     assert(true);
     * }
     * ```
     */
    class InvalidAssetIdError : public Exception
    {
    public:
        explicit InvalidAssetIdError(ui32 id, std::string_view reason = "");
    };

    /**
     * @brief Manages existence of assets at a very basic level
     *
     * Does not deal with asset data or device representations. Only tracks the
     * plain existence of assets and their associated data sources.
     *
     * Enforces strongly typed interfaces where required. Abstractions can be
     * built that handle typing automatically (e.g. via `AssetType`).
     *
     * Any function that takes an `AssetID` or a `TypedAssetID<>` may throw
     * `InvalidAssetIdError`. This can only happen if an asset ID is used after
     * it has been freed via `AssetManagerBase::destroy(id)`.
     */
    class AssetManagerBase
    {
    public:
        /**
         * @brief Access to all relevant information about an asset
         */
        struct AssetInfo
        {
            template<AssetBaseType T>
            AssetInfo(AssetMetadata meta, TypedAssetID<T> id)
                : metadata(std::move(meta)), baseId(id.getAssetID()), typedId(id)
            {}

            template<AssetBaseType T>
            auto asType() const -> std::optional<TypedAssetID<T>>
            {
                try {
                    return std::any_cast<TypedAssetID<T>>(typedId);
                }
                catch (const std::bad_any_cast&) {
                    return std::nullopt;
                }
            }

            inline auto getAssetID() const -> AssetID {
                return baseId;
            }

            inline auto getMetadata() const -> const AssetMetadata& {
                return metadata;
            }

        private:
            AssetMetadata metadata;
            AssetID baseId;
            std::any typedId;
        };

        using const_iterator = util::SafeVector<AssetInfo>::const_iterator;

        AssetManagerBase(const AssetManagerBase&) = delete;
        AssetManagerBase(AssetManagerBase&&) noexcept = delete;
        AssetManagerBase& operator=(const AssetManagerBase&) = delete;
        AssetManagerBase& operator=(AssetManagerBase&&) noexcept = delete;

        AssetManagerBase() = default;
        ~AssetManagerBase() = default;

        /**
         * @brief Create an asset
         *
         * Registers an asset at the asset manager.
         *
         * An asset is defined by an asset source object, which is a mechanism
         * to load an asset's data and metadata lazily.
         *
         * @tparam T The type of asset to create.
         * @param u_ptr<AssetSource<T>> A source for the new asset's data. Must
         *        not be nullptr.
         *
         * @throw std::invalid_argument if `dataSource == nullptr`.
         * @throw std::out_of_range if no asset registry module is registered
         *                          for asset type `T`.
         */
        template<AssetBaseType T>
        auto create(u_ptr<AssetSource<T>> dataSource) -> TypedAssetID<T>;

        /**
         * @brief Remove an asset from the asset manager
         *
         * Remove an asset and destroy all associated resources, particularly
         * the asset's device data.
         *
         * This function takes a typeless AssetID and an explicit type
         * specification via template parameter. This is an unsafe interface
         * intended for internal use.
         *
         * @throw std::invalid_argument if the existing asset with ID `id` is
         *                              not of type `T`.
         * @throw InvalidAssetIdError if `id` is invalid.
         */
        template<AssetBaseType T>
        void destroy(AssetID id);

        /**
         * @brief Remove an asset from the asset manager
         *
         * Remove an asset and destroy all associated resources, particularly
         * the asset's device data.
         *
         * @throw InvalidAssetIdError if `id` is invalid.
         */
        template<AssetBaseType T>
        void destroy(TypedAssetID<T> id);

        /**
         * @brief Try to cast a typeless asset ID to a typed ID
         *
         * @return optional<TypedAssetID<T>> A TypedAssetID if the asset at `id`
         *         is indeed of the specified type `T`, or nullopt otherwise.
         * @throw InvalidAssetIdError if `id` is invalid.
         */
        template<AssetBaseType T>
        auto getAs(AssetID id) const -> std::optional<TypedAssetID<T>>;

        /**
         * @brief Get a handle to an asset's device resources
         *
         * Forces the underlying `AssetRegistryModule` of asset type `T` to load
         * the asset's data and create a device representation if the asset is
         * loaded lazily.
         *
         * @return AssetHandle<T> the respective device implementation of asset
         *         type `T`.
         * @throw InvalidAssetIdError if `id` is invalid.
         */
        template<AssetBaseType T>
        auto getHandle(TypedAssetID<T> id) -> AssetHandle<T>;

        /**
         * @brief Query metadata for an asset
         *
         * @throw InvalidAssetIdError if `id` is invalid.
         */
        auto getMetadata(AssetID id) const -> const AssetMetadata&;

        /**
         * @brief Retrieve an asset's dynamic type information
         *
         * A shortcut for `assetManager.getMetadata(id).type`.
         *
         * @throw InvalidAssetIdError if `id` is invalid.
         */
        auto getAssetType(AssetID id) const -> const AssetType&;

        auto begin() const -> const_iterator;
        auto end() const -> const_iterator;

        /**
         * @throw std::out_of_range if no module for asset type `T` is
         *        registered.
         */
        template<AssetBaseType T>
        auto getModule() -> AssetRegistryModule<T>&;

        /**
         * @brief Access the device-data registry
         *
         * One should normally not access the device registry directly. Use the
         * asset manager's interface to manipulate assets instead.
         */
        auto getDeviceRegistry() -> AssetRegistry&;

    protected:
        /**
         * This is a quick and dirty solution for the problem that the
         * AssetManager has to remove auxiliary data in the case that an
         * asset was added via `create(AssetPath)`, but destroyed with
         * `destroy(AssetID)`.
         *
         * I found two alternatives:
         *  1. Use a general `ComponentStorage` object in `AssetManager` to
         *  store a component for each asset with a custom destructor that
         *  removes the remaining meta information.
         *  2. Overload all `AssetManagerBase::destroy` functions statically
         *  in `AssetManager` in the same fashion as I did in
         *  `AssetManager::create<T>(u_ptr<AssetSource<T>>)`. This is kind of
         *  tedious with the multiple overloads in `AssetManagerBase`.
         *
         * See `AssetManager::beforeAssetDestroy` for details.
         */
        virtual void beforeAssetDestroy(AssetID /*asset*/) {}

    protected:
        static constexpr auto toIndex(AssetID id) noexcept -> ui32 {
            return ui32{id};
        }

        /**
         * Assert that an asset ID exists in the storage. Failing the test means
         * that `id` has become invalid, and `InvalidAssetIdError` is thrown.
         *
         * @throw InvalidAssetIdError
         */
        void assertExists(AssetID id, std::string_view hint) const;

    private:
        data::IdPool<ui32> assetIdPool;
        util::SafeVector<AssetInfo> assetInformation;

        AssetRegistry deviceRegistry;
    };



    template<AssetBaseType T>
    TypedAssetID<T>::operator AssetID() const
    {
        return uniqueId;
    }

    template<AssetBaseType T>
    bool TypedAssetID<T>::operator==(const TypedAssetID& other) const noexcept
    {
        assert(manager != nullptr);
        return uniqueId == other.uniqueId && manager == other.manager;
    }

    template<AssetBaseType T>
    auto TypedAssetID<T>::getAssetID() const -> AssetID
    {
        return uniqueId;
    }

    template<AssetBaseType T>
    auto TypedAssetID<T>::getDeviceID() const -> LocalID
    {
        return id;
    }

    template<AssetBaseType T>
    auto TypedAssetID<T>::get() const -> Handle
    {
        return getDeviceDataHandle();
    }

    template<AssetBaseType T>
    auto TypedAssetID<T>::getDeviceDataHandle() const -> Handle
    {
        assert(manager != nullptr);
        return manager->getHandle(*this);
    }

    template<AssetBaseType T>
    auto TypedAssetID<T>::getMetadata() const -> const AssetMetadata&
    {
        return manager->getMetadata(uniqueId);
    }

    template<AssetBaseType T>
    auto TypedAssetID<T>::getAssetManager() -> AssetManagerBase&
    {
        assert(manager != nullptr);
        return *manager;
    }

    template<AssetBaseType T>
    auto TypedAssetID<T>::getModule() -> AssetRegistryModule<T>&
    {
        assert(manager != nullptr);
        return manager->getModule<T>();
    }



    template<AssetBaseType T>
    auto AssetManagerBase::create(u_ptr<AssetSource<T>> source) -> TypedAssetID<T>
    {
        using LocalID = typename AssetBaseTypeTraits<T>::LocalID;

        // Assert correctness of resources
        if (source == nullptr) {
            throw std::invalid_argument("[In AssetManagerBase::create]: Argument `dataSource` must"
                                        " not be nullptr!");
        }

        // Assert that a device module for `T` exists
        if (!deviceRegistry.hasModule<T>())
        {
            throw std::out_of_range(
                "[In AssetManagerBase::create]: No device registry module is registered for"
                " asset type " + AssetType::make<T>().getName()
            );
        }

        // Test for the correct asset type. This is ok because we need to load
        // the metadata anyway.
        AssetMetadata meta = source->getMetadata();
        if (meta.type != AssetType::make<T>())
        {
            throw std::invalid_argument(
                "[In AssetManagerBase::create]: Expected asset source to point to an asset of"
                " type" + AssetType::make<T>().getName() + ", but metadata specified the type "
                + meta.type.getName() + "."
            );
        }

        const LocalID localId = deviceRegistry.add(std::move(source));
        const AssetID id{ assetIdPool.generate() };
        const TypedAssetID<T> typedId{ id, localId, *this };

        assert(!assetInformation.contains(ui32{id}));
        assetInformation.emplace(ui32{id}, AssetInfo{ std::move(meta), typedId });

        return typedId;
    }

    template<AssetBaseType T>
    void AssetManagerBase::destroy(AssetID id)
    {
        assertExists(id, "possible double free?");
        if (!getAs<T>(id))
        {
            throw std::invalid_argument(
                "[In AssetManagerBase]: Tried to remove asset " + std::to_string(ui32{id})
                + " with type " + AssetType::make<T>().getName() + ", but the existing asset has"
                " type " + getAssetType(id).getName() + "."
            );
        }

        beforeAssetDestroy(id);

        const auto typedId = *getAs<T>(id);
        deviceRegistry.remove<T>(typedId.getDeviceID());
        assetInformation.erase(ui32{id});
        assetIdPool.free(ui32{id});
    }

    template<AssetBaseType T>
    void AssetManagerBase::destroy(TypedAssetID<T> id)
    {
        destroy<T>(id.getAssetID());
    }

    template<AssetBaseType T>
    auto AssetManagerBase::getAs(AssetID id) const -> std::optional<TypedAssetID<T>>
    {
        assertExists(id, "has the asset already been destroyed?");
        return assetInformation.at(ui32{id}).asType<T>();
    }

    template<AssetBaseType T>
    auto AssetManagerBase::getHandle(TypedAssetID<T> id) -> AssetHandle<T>
    {
        assertExists(id, "has the asset already been destroyed?");
        return deviceRegistry.get<T>(id.getDeviceID());
    }

    template<AssetBaseType T>
    auto AssetManagerBase::getModule() -> AssetRegistryModule<T>&
    {
        return dynamic_cast<AssetRegistryModule<T>&>(getDeviceRegistry().getModule<T>());
    }
} // namespace trc
