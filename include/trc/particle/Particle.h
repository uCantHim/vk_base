#pragma once

#include <vector>
#include <mutex>

#include <glm/gtc/quaternion.hpp>
#include "trc/base/Buffer.h"
#include "trc/base/MemoryPool.h"

#include "trc/Types.h"
#include "trc/RasterSceneBase.h"
#include "trc/core/PipelineTemplate.h"
#include "trc_util/Timer.h"
#include "trc_util/Util.h"
#include "trc_util/async/ThreadPool.h"
#include "trc/Node.h"

namespace trc
{
    class Instance;
    class TorchRenderConfig;

    struct ParticleMaterial
    {
        enum BlendingType : ui32
        {
            eDiscardZeroAlpha = 0,
            eAlphaBlend,

            NUM_BLENDING_TYPES
        };

        ui32 texture;
        BlendingType blending{ BlendingType::eDiscardZeroAlpha };
    };

    struct ParticlePhysical
    {
        vec3 position{ 0.0f };
        vec3 linearVelocity{ 0.0f };
        vec3 linearAcceleration{ 0.0f };

        quat orientation{ glm::angleAxis(0.0f, vec3{ 0, 1, 0 }) };
        vec3 rotationAxis{ 0, 1, 0 };
        float angularVelocity{ 0.0f };

        vec3 scaling{ 1.0f };

        float lifeTime{ 1000.0f };
        float timeLived{ 0.0f };
    };

    struct Particle
    {
        ParticlePhysical phys;
        ParticleMaterial material;
    };

    /**
     * @brief A collection of particle drawing data
     *
     * A huge pool of particles that draws all particles with one call.
     */
    class ParticleCollection
    {
    public:
        ParticleCollection(Instance& instance, ui32 maxParticles);

        void attachToScene(RasterSceneBase& scene);
        void removeFromScene();

        void addParticle(const Particle& particle);
        void addParticles(const std::vector<Particle>& particles);

        /**
         * @brief Simulate particles and update GPU data
         *
         * @param float timeDelta Simulation time in milliseconds
         */
        void update(float timeDelta);

    private:
        /**
         * @brief Per-instance device data
         */
        struct ParticleDeviceData
        {
            mat4 transform;
            ui32 textureIndex;
        };

        /**
         * @brief Simulate particle physics
         */
        void tickParticles(float timeDelta);

        const Instance& instance;
        const ui32 maxParticles;

        MemoryPool memoryPool;
        DeviceLocalBuffer vertexBuffer;

        // GPU resources
        std::vector<Particle> particles;
        Buffer particleDeviceDataStagingBuffer;
        DeviceLocalBuffer particleDeviceDataBuffer;

        ParticleDeviceData* persistentParticleDeviceDataBuf;

        // Per-blend type draw calls
        using Blend = ParticleMaterial::BlendingType;
        template<typename T>
        using PerBlendType = std::array<T, Blend::NUM_BLENDING_TYPES>;

        struct BlendTypeSize
        {
            ui32 offset{ 0 };
            ui32 count{ 0 };
        };
        PerBlendType<BlendTypeSize> blendTypeSizes;

        // Staging storage for new particles
        std::mutex newParticleListLock;
        std::vector<Particle> newParticles;

        // Updater
        ExclusiveQueue transferQueue;
        vk::UniqueFence transferFence;
        vk::UniqueCommandPool transferCmdPool;
        vk::UniqueCommandBuffer transferCmdBuf;

        // Drawable registrations
        PerBlendType<RasterSceneBase::UniqueRegistrationID> drawRegistrations;
        RasterSceneBase::UniqueRegistrationID shadowRegistration;
    };

    /**
     * @brief A spawn point for particles
     *
     * Creates particles at a ParticleCollection.
     */
    class ParticleSpawn : public Node
    {
    public:
        explicit ParticleSpawn(ParticleCollection& collection,
                               std::vector<Particle> particles = {});

        void addParticle(Particle particle);

        void spawnParticles();
        // void generateParticles();

    private:
        static inline async::ThreadPool threads;

        std::vector<Particle> particles;
        ParticleCollection* collection{ nullptr };
    };
}
