#pragma once

#include <cassert>

#include <atomic>
#include <iomanip>
#include <optional>
#include <string>
#include <sstream>

#include "trc/Types.h"
#include "trc/assets/AssetBase.h"
#include "trc/assets/AssetPath.h"
#include "trc/assets/AssetType.h"

namespace trc
{
    /**
     * @brief General data stored for every type of asset
     */
    struct AssetMetadata
    {
        std::string name;
        AssetType type;
        std::optional<AssetPath> path{ std::nullopt };
    };

    /**
     * @brief An interface that creates/loads data for a specific asset type
     */
    template<AssetBaseType T>
    class AssetSource
    {
    public:
        virtual ~AssetSource() = default;

        virtual auto load() -> AssetData<T> = 0;
        virtual auto getMetadata() -> AssetMetadata = 0;
    };

    template<AssetBaseType T>
    class InMemorySource : public AssetSource<T>
    {
    public:
        explicit InMemorySource(AssetData<T> data)
            : data(std::move(data)), name(generateName())
        {}

        InMemorySource(AssetData<T> data, std::string assetName)
            : data(std::move(data)), name(std::move(assetName))
        {}

        auto load() -> AssetData<T> override {
            return data;
        }

        auto getMetadata() -> AssetMetadata override {
            return { .name=name, .type=AssetType::make<T>(), .path=std::nullopt };
        }

    private:
        static inline std::atomic<ui32> uniqueNameIndex{ 0 };

        static auto generateName() -> std::string
        {
            std::stringstream ss;
            ss.fill('0');
            ss << "generated_asset_name_" << std::setw(5) << ++uniqueNameIndex;
            return ss.str();
        }

        const AssetData<T> data;
        const std::string name;
    };
} // namespace trc
