#pragma once

#include <optional>
#include <utility>

#include <trc_util/Exception.h>

#include "trc/Types.h"
#include "trc/assets/AssetPath.h"
#include "trc/assets/AssetSource.h"
#include "trc/assets/AssetType.h"
#include "trc/assets/Serializer.h"
#include "trc/base/Logging.h"
#include "trc/util/DataStorage.h"
#include "trc/util/Pathlet.h"

namespace trc
{
    class AssetLoadError : Exception
    {
    public:
        AssetLoadError(const AssetPath& path, const std::string& reason)
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
        auto load(const AssetPath& path) -> AssetParseResult<T>;

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

        /**
         * @brief Delete an item from storage.
         *
         * Note that this may be a permanent action, depending on the underlying
         * data storage implementation. For example, a filesystem storage may
         * delete the respective data file.
         */
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
        AssetStorageSource(AssetPath path, const s_ptr<DataStorage>& storage)
            : path(std::move(path)), storage(storage)
        {}

        auto load() -> AssetData<T> override
        {
            auto data = AssetStorage{storage}.load<T>(path);
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
            auto meta = AssetStorage{storage}.getMetadata(path);
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
        s_ptr<DataStorage> storage;
    };

    template<AssetBaseType T>
    auto AssetStorage::load(const AssetPath& path) -> AssetParseResult<T>
    {
        // Ensure that the correct type of asset is stored at `path`
        const auto meta = getMetadata(path);
        if (!meta || meta->type != AssetType::make<T>())
        {
            return std::unexpected(AssetParseError{
                AssetParseError::Code::eSemanticError,
                "Asset at " + path.string() + " does not exist or data at that"
                " path is not of the requested type " + std::string{T::name()} + "."
            });
        }

        // Load and parse data
        auto dataStream = storage->read(makeDataPath(path));
        if (dataStream != nullptr) {
            return AssetSerializerTraits<T>::deserialize(*dataStream);
        }

        return std::unexpected(AssetParseError{
            AssetParseError::Code::eSystemError,
            "Unable to read data at path " + makeDataPath(path).string() + "."
        });
    }

    template<AssetBaseType T>
    auto AssetStorage::loadDeferred(const AssetPath& path) -> std::optional<u_ptr<AssetSource<T>>>
    {
        // Ensure that the correct type of asset is stored at `path`
        const auto meta = getMetadata(path);
        if (!meta || meta->type != AssetType::make<T>()) {
            return std::nullopt;
        }

        return std::make_unique<AssetStorageSource<T>>(path, this->storage);
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
        AssetSerializerTraits<T>::serialize(data, *dataStream);

        return true;
    }
} // namespace trc
