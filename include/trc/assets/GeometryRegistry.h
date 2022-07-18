#pragma once

#include <optional>
#include <mutex>

#include <vkb/Buffer.h>
#include <vkb/MemoryPool.h>
#include <trc_util/data/IndexMap.h>
#include <trc_util/data/ObjectId.h>

#include "AssetBaseTypes.h"
#include "AssetRegistryModule.h"
#include "AssetSource.h"
#include "Rig.h"
#include "util/DeviceLocalDataWriter.h"

namespace trc
{
    /**
     * @brief Handle to a geometry stored in the asset registry
     */
    template<>
    class AssetHandle<Geometry>
    {
    public:
        enum class VertexType : ui8
        {
            eMesh     = 1 << 0,
            eSkeletal = 1 << 1,
        };

        AssetHandle() = default;
        AssetHandle(const AssetHandle&) = default;
        AssetHandle(AssetHandle&&) noexcept = default;
        ~AssetHandle() = default;

        auto operator=(const AssetHandle&) -> AssetHandle& = default;
        auto operator=(AssetHandle&&) noexcept -> AssetHandle& = default;

        /**
         * @brief Bind vertex and index buffer
         *
         * @param vk::CommandBuffer cmdBuf
         * @param ui32 binding Binding index to which the vertex buffer will
         *                     be bound.
         */
        void bindVertices(vk::CommandBuffer cmdBuf, ui32 binding) const;

        auto getIndexBuffer() const noexcept -> vk::Buffer;
        auto getVertexBuffer() const noexcept -> vk::Buffer;
        auto getIndexCount() const noexcept -> ui32;

        auto getIndexType() const noexcept -> vk::IndexType;
        auto getVertexType() const noexcept -> VertexType;
        auto getVertexSize() const noexcept -> size_t;

        bool hasRig() const;
        auto getRig() -> RigID;

    private:
        friend class GeometryRegistry;

        AssetHandle(vk::Buffer indices, ui32 numIndices, vk::IndexType indexType,
                       vk::Buffer verts, VertexType vertexType,
                       std::optional<RigID> rig = std::nullopt);

        vk::Buffer indexBuffer;
        vk::Buffer vertexBuffer;

        ui32 numIndices{ 0 };
        vk::IndexType indexType;
        VertexType vertexType;

        std::optional<RigID> rig;
    };

    /**
     * @brief
     */
    class GeometryRegistry : public AssetRegistryModuleCacheCrtpBase<Geometry>
    {
    public:
        using LocalID = TypedAssetID<Geometry>::LocalID;

        explicit GeometryRegistry(const AssetRegistryModuleCreateInfo& info);

        void update(vk::CommandBuffer cmdBuf, FrameRenderState& state) final;

        auto add(u_ptr<AssetSource<Geometry>> source) -> LocalID override;
        void remove(LocalID id) override;

        auto getHandle(LocalID id) -> GeometryHandle override;

        void load(LocalID id) override;
        void unload(LocalID id) override;

    private:
        struct Config
        {
            vk::BufferUsageFlags geometryBufferUsage;
            bool enableRayTracing;
        };

        /**
         * GPU resources for geometry data
         */
        struct InternalStorage
        {
            using VertexType = GeometryHandle::VertexType;

            struct DeviceData
            {
                vkb::Buffer indexBuf;
                vkb::Buffer vertexBuf;
                ui32 numIndices{ 0 };
                ui32 numVertices{ 0 };

                VertexType vertexType;
            };

            ui32 deviceIndex;
            u_ptr<AssetSource<Geometry>> source;
            u_ptr<DeviceData> deviceData;
            std::optional<RigID> rig;

            u_ptr<CacheRefCounter> refCounter;
        };

        static constexpr ui32 MEMORY_POOL_CHUNK_SIZE = 200000000;  // 200 MiB
        static constexpr ui32 MAX_GEOMETRY_COUNT = 5000;

        const vkb::Device& device;
        const Config config;

        data::IdPool idPool;
        vkb::MemoryPool memoryPool;
        DeviceLocalDataWriter dataWriter;

        std::mutex storageLock;
        data::IndexMap<LocalID::IndexType, InternalStorage> storage;

        SharedDescriptorSet::Binding indexDescriptorBinding;
        SharedDescriptorSet::Binding vertexDescriptorBinding;
    };
} // namespace trc
