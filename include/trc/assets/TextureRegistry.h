#pragma once

#include <istream>
#include <ostream>
#include <shared_mutex>
#include <string_view>
#include <vector>

#include <componentlib/Table.h>
#include <trc_util/data/IndexMap.h>
#include <trc_util/data/IdPool.h>

#include "trc/assets/AssetManagerBase.h"  // For asset ID types
#include "trc/assets/AssetRegistryModule.h"
#include "trc/assets/AssetSource.h"
#include "trc/assets/DeviceDataCache.h"
#include "trc/assets/SharedDescriptorSet.h"
#include "trc/base/Image.h"
#include "trc/base/MemoryPool.h"
#include "trc/util/DeviceLocalDataWriter.h"

namespace trc
{
    class TextureRegistry;

    struct Texture
    {
        using Registry = TextureRegistry;

        static consteval auto name() -> std::string_view {
            return "torch_tex";
        }
    };

    template<>
    struct AssetData<Texture>
    {
        uvec2 size;
        std::vector<glm::u8vec4> pixels;

        void serialize(std::ostream& os) const;
        void deserialize(std::istream& is);
    };

    struct TextureRegistryCreateInfo
    {
        const Device& device;
        SharedDescriptorSet::Binding textureDescBinding;
    };

    class TextureRegistry : public AssetRegistryModuleInterface<Texture>
    {
        friend class AssetHandle<Texture>;
        struct InternalStorage;

    public:
        explicit TextureRegistry(const TextureRegistryCreateInfo& info);

        void update(vk::CommandBuffer cmdBuf, FrameRenderState&) final;

        auto add(u_ptr<AssetSource<Texture>> source) -> LocalID override;
        void remove(LocalID id) override;

        auto getHandle(LocalID id) -> AssetHandle<Texture> override;

    private:
        struct DeviceData
        {
            ui32 deviceIndex;

            Image image;
            vk::UniqueImageView imageView;
        };

        template<typename T>
        using Table = componentlib::Table<T, LocalID::IndexType>;
        using DataCache = DeviceDataCache<DeviceData>;

        static constexpr ui32 MAX_TEXTURE_COUNT = 2000;  // For static descriptor size
        static constexpr ui32 MEMORY_POOL_CHUNK_SIZE = 512 * 512 * 4 * 30;  // 30 512x512 images

        auto loadDeviceData(LocalID id) -> DeviceData;
        void freeDeviceData(LocalID id, DeviceData data);

        const Device& device;
        MemoryPool memoryPool;
        DeviceLocalDataWriter dataWriter;

        data::IdPool<ui64> idPool;
        std::shared_mutex sourceStorageLock;
        Table<u_ptr<AssetSource<Texture>>> dataSources;
        DataCache deviceDataStorage;

        SharedDescriptorSet::Binding descBinding;
    };

    /**
     * @brief Engine-internal representation of a texture resource
     */
    template<>
    class AssetHandle<Texture>
    {
    public:
        auto getDeviceIndex() const -> ui32;

    private:
        friend TextureRegistry;
        using DataHandle = TextureRegistry::DataCache::CacheEntryHandle;

        explicit AssetHandle(DataHandle handle);

        DataHandle cacheRef;
        ui32 deviceIndex;
    };

    using TextureHandle = AssetHandle<Texture>;
    using TextureData = AssetData<Texture>;
    using TextureID = TypedAssetID<Texture>;
} // namespace trc
