#pragma once

#include <optional>
#include <utility>

#include "trc/Types.h"
#include "trc/assets/AssetPath.h"
#include "trc/assets/AssetSource.h"
#include "trc/assets/AssetType.h"
#include "trc/base/Logging.h"
#include "trc/util/DataStorage.h"
#include "trc/util/Pathlet.h"

namespace trc
{
    class AssetLoadError : Exception
    {
    public:
        AssetLoadError(const AssetPath& path, std::string reason)
            : Exception("Unable to load asset from \"" + path.string() + "\": "
                        + reason + ".")
        {}
    };

    /**
     * @brief
     */
    class AssetStorage
    {
    public:
        explicit AssetStorage(s_ptr<DataStorage> storage);

        auto getMetadata(const AssetPath& path) -> std::optional<AssetMetadata>;

        template<AssetBaseType T>
        auto load(const AssetPath& path) -> std::optional<AssetData<T>>;

        /**
         * @brief Create an asset source that can load an asset at a later
         *        time.
         *
         * The created asset source must outlive the AssetStorage by which
         * it was created.
         *
         * Temporarily loads the metadata at `path` into memory to check whether
         * asset exists and it has the requested type.
         *
         * @return optional<u_ptr<AssetSource<T>>> A source object that allows
         *         loading the asset at `path` at a later time.
         *         Returns nullopt if the storage does not contain an asset at
         *         `path` or if the asset at `path` is not of type `T`.
         */
        template<AssetBaseType T>
        auto loadDeferred(const AssetPath& path) -> std::optional<u_ptr<AssetSource<T>>>;

        template<AssetBaseType T>
        bool store(const AssetPath& path, const AssetData<T>& data);

        bool remove(const AssetPath& path);

        struct AssetIterator
        {
            using const_reference = const AssetPath&;
            using const_pointer = const AssetPath*;

            AssetIterator(DataStorage::iterator begin, DataStorage::iterator end);

            auto operator*() const -> const_reference;
            auto operator->() const -> const_pointer;

            auto operator++() -> AssetIterator&;

            bool operator==(const AssetIterator& other) const;
            bool operator!=(const AssetIterator& other) const = default;

        private:
            static bool isMetaFile(const util::Pathlet& path);
            void step();

            DataStorage::iterator iter;
            DataStorage::iterator end;
            std::optional<AssetPath> currentPath;
        };

        using iterator = AssetIterator;

        auto begin() -> iterator;
        auto end() -> iterator;

    private:
        static auto makeMetaPath(const AssetPath& path) -> util::Pathlet;
        static auto makeDataPath(const AssetPath& path) -> util::Pathlet;
        static void serializeMetadata(const AssetMetadata& meta, std::ostream& os);
        static auto deserializeMetadata(std::istream& is) -> AssetMetadata;

        s_ptr<DataStorage> storage;
    };

    /**
     * @brief Asset source that loads data from an AssetStorage
     */
    template<AssetBaseType T>
    class AssetStorageSource : public AssetSource<T>
    {
    public:
        AssetStorageSource(AssetPath path, AssetStorage& storage)
            : path(std::move(path)), storage(storage)
        {}

        auto load() -> AssetData<T> override
        {
            auto data = storage.load<T>(path);
            if (!data.has_value())
            {
                log::error << "Unable to load asset at " << path.string()
                           << ": path not found in the asset storage or the stored data is "
                           << "not of the requested type.\n";
                throw AssetLoadError(path, "Path is not in storage or stored data is not of type"
                                           + ("T [T = " + std::string(typeid(T).name()) + "]"));
            }
            return *data;
        }

        auto getMetadata() -> AssetMetadata override
        {
            auto meta = storage.getMetadata(path);
            if (!meta.has_value())
            {
                log::error << "Unable to load asset metadata from " << path.string()
                           << ": path not found in the asset storage.\n";
                throw AssetLoadError(path, "Metadata not found in storage.");
            }
            return std::move(*meta);
        }

    private:
        const AssetPath path;
        AssetStorage& storage;
    };

    template<AssetBaseType T>
    auto AssetStorage::load(const AssetPath& path) -> std::optional<AssetData<T>>
    {
        // Ensure that the correct type of asset is stored at `path`
        const auto meta = getMetadata(path);
        if (!meta || meta->type != AssetType::make<T>()) {
            return std::nullopt;
        }

        // Load and parse data
        auto dataStream = storage->read(makeDataPath(path));
        if (dataStream != nullptr)
        {
            AssetData<T> data;
            data.deserialize(*dataStream);
            return data;
        }

        return std::nullopt;
    }

    template<AssetBaseType T>
    auto AssetStorage::loadDeferred(const AssetPath& path) -> std::optional<u_ptr<AssetSource<T>>>
    {
        // Ensure that the correct type of asset is stored at `path`
        const auto meta = getMetadata(path);
        if (!meta || meta->type != AssetType::make<T>()) {
            return std::nullopt;
        }

        return std::make_unique<AssetStorageSource<T>>(path, *this);
    }

    template<AssetBaseType T>
    bool AssetStorage::store(const AssetPath& path, const AssetData<T>& data)
    {
        auto dataStream = storage->write(makeDataPath(path));
        auto metaStream = storage->write(makeMetaPath(path));
        if (dataStream == nullptr || metaStream == nullptr)
        {
            if (dataStream != metaStream)
            {
                log::debug << "[In AssetStorage::store]: If the data path of an asset does not"
                    " exist in the data storage, then the metadata path should not exist either,"
                    " and vice-versa. However, this is not the case. [For asset path "
                    << path.string() << std::boolalpha
                    << ": <meta-path> -> " << !!metaStream
                    << ", <data-path> -> " << !!dataStream << "]"
                    ". Investigate whether this is an issue.";
            }
            return false;
        }

        serializeMetadata(AssetMetadata{
            .name=path.getAssetName(),
            .type=AssetType::make<T>(),
            .path=path
        }, *metaStream);
        data.serialize(*dataStream);

        return true;
    }
} // namespace trc
