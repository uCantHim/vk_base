#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <variant>

#include <trc_util/Util.h>

#include "trc/assets/AssetPath.h"
#include "trc/assets/AssetManager.h"
#include "trc/assets/AssetSource.h"  // This is not strictly necessary, a forward declaration works as well

namespace trc
{
    /**
     * @brief Reference to an asset of a specific type
     *
     * References are always optional, i.e. they can point to no object.
     *
     * If the reference is set and points to an asset, it only requires to
     * have the referencee's unique path. The corresponding asset ID is
     * filled in when the asset is loaded by the AssetManager. It is
     * possible to query if this has already happened with `hasResolvedID()`.
     */
    template<AssetBaseType T>
    class AssetReference
    {
    public:
        AssetReference(const AssetReference&) = default;
        AssetReference(AssetReference&&) noexcept = default;
        auto operator=(const AssetReference&) -> AssetReference& = default;
        auto operator=(AssetReference&&) noexcept -> AssetReference& = default;
        ~AssetReference() = default;

        AssetReference() = default;
        AssetReference(AssetPath path);
        AssetReference(TypedAssetID<T> id);
        AssetReference(AssetData<T> data);
        AssetReference(u_ptr<AssetSource<T>> source);

        /**
         * A reference is empty if it points to neither an asset path nor
         * an asset ID.
         *
         * @return bool
         */
        bool empty() const;

        /**
         * Tests whether the reference points to an object that is
         * identified by a persistent asset path, or whether it is an
         * ad-hoc reference to an asset that exists only in memory.
         *
         * @return bool True if the referenced object is identified by an
         *              asset path.
         */
        bool hasAssetPath() const;
        auto getAssetPath() const -> const AssetPath&;

        /**
         * @return bool True if the reference points to an existing asset
         *              with an ID.
         */
        bool hasResolvedID() const;
        auto getID() const -> TypedAssetID<T>;

        /**
         * Queries the ID of the asset at the referenced path from the
         * asset manager and stores it the reference. Loads the referenced
         * asset if it has not been loaded before.
         */
        void resolve(AssetManager& manager);

    private:
        struct SharedData
        {
            std::variant<std::monostate, AssetPath, u_ptr<AssetSource<T>>> dataSource;
            std::optional<TypedAssetID<T>> id;
        };

        s_ptr<SharedData> data{ nullptr };
    };



    ///////////////////////////
    //    Implementations    //
    ///////////////////////////

    template<AssetBaseType T>
    AssetReference<T>::AssetReference(AssetPath path)
        : data(new SharedData{ .dataSource=std::move(path), .id={} })
    {
    }

    template<AssetBaseType T>
    AssetReference<T>::AssetReference(TypedAssetID<T> id)
        : data(new SharedData{ .dataSource={}, .id=id })
    {
    }

    template<AssetBaseType T>
    AssetReference<T>::AssetReference(AssetData<T> data)
        : AssetReference(std::make_unique<InMemorySource<T>>(std::move(data)))
    {
    }

    template<AssetBaseType T>
    AssetReference<T>::AssetReference(u_ptr<AssetSource<T>> source)
        : data(new SharedData{ .dataSource=std::move(source), .id={} })
    {
    }

    template<AssetBaseType T>
    bool AssetReference<T>::empty() const
    {
        return data == nullptr;
    }

    template<AssetBaseType T>
    bool AssetReference<T>::hasAssetPath() const
    {
        return data != nullptr && std::holds_alternative<AssetPath>(data->dataSource);
    }

    template<AssetBaseType T>
    auto AssetReference<T>::getAssetPath() const -> const AssetPath&
    {
        if (!hasAssetPath())
        {
            throw std::runtime_error("[In AssetReference::getAssetPath]: Reference does not"
                                     " contain an asset path!");
        }

        return std::get<AssetPath>(data->dataSource);
    }

    template<AssetBaseType T>
    void AssetReference<T>::resolve(AssetManager& manager)
    {
        if (!hasResolvedID())
        {
            if (empty())
            {
                throw std::runtime_error("[In AssetReference::resolve]: Tried to resolve an empty"
                                         " asset reference");
            }

            std::visit(
                util::VariantVisitor{
                    // We can move the source here because `resolve` won't be executed
                    // again once the ID has been set
                    [&](u_ptr<AssetSource<T>>& src) { data->id = manager.create(std::move(src)); },
                    [&](const AssetPath& path) { data->id = manager.create<T>(path); },
                    [](const std::monostate&) {}
                },
                data->dataSource
            );
        }
    }

    template<AssetBaseType T>
    bool AssetReference<T>::hasResolvedID() const
    {
        return data != nullptr && data->id.has_value();
    }

    template<AssetBaseType T>
    auto AssetReference<T>::getID() const -> TypedAssetID<T>
    {
        if (!hasResolvedID())
        {
            throw std::runtime_error("[In AssetReference::getID]: The reference does not point to"
                                     " a valid asset ID");
        }

        return *data->id;
    }
} // namespace trc
