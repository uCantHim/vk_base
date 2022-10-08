#pragma once

#include <mutex>

#include "trc/base/Device.h"

#include "trc/Types.h"
#include "trc/VulkanInclude.h"
#include "trc/core/DescriptorProvider.h"

namespace trc
{
    /**
     * The SharedDescriptorSet is intended to be used as a collection of
     * descriptor bindings that each 'belong' to a different service and
     * are logically managed in an independent, decentralized manner.
     *
     * Changes to individual bindings are collectively executed in the
     * `SharedDescriptorSet::update` method.
     *
     * Can only be created with the static `SharedDescriptorSet::build`
     * function.
     */
    class SharedDescriptorSet
    {
    private:
        friend class Builder;
        SharedDescriptorSet() = default;

    public:
        SharedDescriptorSet(const SharedDescriptorSet&) = delete;
        SharedDescriptorSet(SharedDescriptorSet&&) = delete;
        auto operator=(const SharedDescriptorSet&) -> SharedDescriptorSet&;
        auto operator=(SharedDescriptorSet&&) -> SharedDescriptorSet&;

        ~SharedDescriptorSet() = default;

        class Builder;

        /**
         * Build a descriptor set.
         */
        static auto build() -> Builder;

        auto getProvider() const -> const DescriptorProviderInterface&;

        /**
         * Execute necessary descriptor updates.
         *
         * Updates can be enqueued for individual bindings with the
         * `Binding::update` method.
         */
        void update(const Device& device);

        class Binding
        {
        public:
            Binding() = default;
            Binding(const Binding&) = default;
            Binding(Binding&&) = default;
            auto operator=(const Binding&) -> Binding& = default;
            auto operator=(Binding&&) -> Binding& = default;
            ~Binding() = default;

            void update(ui32 arrayElem, vk::DescriptorBufferInfo buffer);
            void update(ui32 firstArrayElem,
                        const vk::ArrayProxy<const vk::DescriptorBufferInfo>& buffers);

            void update(ui32 arrayElem, vk::DescriptorImageInfo image);
            void update(ui32 firstArrayElem,
                        const vk::ArrayProxy<const vk::DescriptorImageInfo>& images);

            void update(ui32 arrayElem, vk::BufferView view);
            void update(ui32 firstArrayElem,
                        const vk::ArrayProxy<const vk::BufferView>& views);

        private:
            friend Builder;
            Binding(SharedDescriptorSet& set, ui32 bindingIndex);

            SharedDescriptorSet* set{ nullptr };
            ui32 bindingIndex;
        };

        class Builder
        {
        public:
            Builder();

            void addLayoutFlag(vk::DescriptorSetLayoutCreateFlags flags);
            void addPoolFlag(vk::DescriptorPoolCreateFlags flags);
            auto addBinding(vk::DescriptorType type,
                            ui32 count,
                            vk::ShaderStageFlags stages,
                            vk::DescriptorBindingFlags flags = {})
                -> Binding;

            auto build(const Device& device) -> u_ptr<SharedDescriptorSet>;

        private:
            friend SharedDescriptorSet;

            u_ptr<SharedDescriptorSet> set;

            vk::DescriptorSetLayoutCreateFlags layoutFlags;
            vk::DescriptorPoolCreateFlags poolFlags;
            std::vector<vk::DescriptorBindingFlags> bindingFlags;
        };

    private:
        void build(const Device& device, const Builder& builder);


        /////////////////////////
        // Basic device resources

        vk::UniqueDescriptorSetLayout layout;
        vk::UniqueDescriptorPool pool;
        vk::UniqueDescriptorSet set;
        u_ptr<DescriptorProvider> provider{ new DescriptorProvider({}, {}) };

        std::vector<vk::DescriptorSetLayoutBinding> bindings;


        /////////////////////
        // Descriptor updates

        struct UpdateContainer
        {
            UpdateContainer(const UpdateContainer&) = delete;
            auto operator=(const UpdateContainer&) -> UpdateContainer& = delete;

            UpdateContainer() = default;
            UpdateContainer(UpdateContainer&&) = default;
            auto operator=(UpdateContainer&&) -> UpdateContainer& = default;

            UpdateContainer(const vk::ArrayProxy<const vk::DescriptorBufferInfo>& buffers)
                : bufferInfos(buffers.begin(), buffers.end()), imageInfos(), bufferViews() {}
            UpdateContainer(const vk::ArrayProxy<const vk::DescriptorImageInfo>& images)
                : bufferInfos(), imageInfos(images.begin(), images.end()), bufferViews() {}
            UpdateContainer(const vk::ArrayProxy<const vk::BufferView>& views)
                : bufferInfos(), imageInfos(), bufferViews(views.begin(), views.end()) {}

            std::vector<vk::DescriptorBufferInfo> bufferInfos;
            std::vector<vk::DescriptorImageInfo> imageInfos;
            std::vector<vk::BufferView> bufferViews;
        };

        void update(ui32 binding,
                    ui32 firstArrayElem,
                    const vk::ArrayProxy<const vk::DescriptorBufferInfo>& buffers);
        void update(ui32 binding,
                    ui32 firstArrayElem,
                    const vk::ArrayProxy<const vk::DescriptorImageInfo>& images);
        void update(ui32 binding,
                    ui32 firstArrayElem,
                    const vk::ArrayProxy<const vk::BufferView>& bufferViews);

        std::mutex descriptorUpdateLock;
        std::vector<UpdateContainer> updateStructs;
        std::vector<vk::WriteDescriptorSet> writes;
    };
} // namespace trc
