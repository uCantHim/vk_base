#pragma once

#include <vkb/Buffer.h>
#include <trc_util/Padding.h>
#include <trc_util/data/IndexMap.h>
#include <trc_util/data/ObjectId.h>

#include "Types.h"
#include "AssetIds.h"
#include "Material.h"
#include "AssetRegistryModule.h"
#include "assets/RawData.h"

namespace trc
{
    class MaterialRegistry : public AssetRegistryModuleInterface
    {
    public:
        using LocalID = MaterialID::LocalID;
        using Handle = MaterialDeviceHandle;

        MaterialRegistry(const AssetRegistryModuleCreateInfo& info);

        void update(vk::CommandBuffer cmdBuf) final;

        auto getDescriptorLayoutBindings() -> std::vector<DescriptorLayoutBindingInfo> final;
        auto getDescriptorUpdates() -> std::vector<vk::WriteDescriptorSet> final;

        auto add(const MaterialData& data) -> LocalID;
        void remove(LocalID id);

        auto getHandle(LocalID id) -> MaterialDeviceHandle;

        template<std::invocable<MaterialData&> F>
        void modify(LocalID id, F&& func)
        {
            func(materials.at(id).matData);
            // Material buffer needs updating now
        }

    private:
        static constexpr ui32 NO_TEXTURE = UINT32_MAX;

        struct MaterialDeviceData
        {
            MaterialDeviceData() = default;
            MaterialDeviceData(const MaterialData& data);

            vec4 color{ 0.0f, 0.0f, 0.0f, 1.0f };

            vec4 kAmbient{ 1.0f };
            vec4 kDiffuse{ 1.0f };
            vec4 kSpecular{ 1.0f };

            float shininess{ 1.0f };
            float reflectivity{ 0.0f };

            ui32 diffuseTexture{ NO_TEXTURE };
            ui32 specularTexture{ NO_TEXTURE };
            ui32 bumpTexture{ NO_TEXTURE };

            bool32 performLighting{ true };

            ui32 __padding[2]{ 0, 0 };
        };

        static_assert(util::sizeof_pad_16_v<MaterialDeviceData> == sizeof(MaterialDeviceData),
                      "MaterialDeviceData struct must be padded to 16 bytes for std430!");

        struct InternalStorage
        {
            operator MaterialDeviceHandle() const {
                return MaterialDeviceHandle(bufferIndex);
            }

            ui32 bufferIndex;
            MaterialData matData;
        };

        static constexpr ui32 MATERIAL_BUFFER_DEFAULT_SIZE = sizeof(MaterialDeviceData) * 100;

        const AssetRegistryModuleCreateInfo config;

        data::IdPool idPool;
        data::IndexMap<LocalID::IndexType, InternalStorage> materials;
        vkb::Buffer materialBuffer;

        vk::DescriptorBufferInfo matBufferDescInfo;
    };
} // namespace trc