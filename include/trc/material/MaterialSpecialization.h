#pragma once

#include <functional>
#include <generator>

#include "material.pb.h"

#include "trc/FlagCombination.h"
#include "trc/material/shader/ShaderProgram.h"

namespace trc
{
    /**
     * The user-defined material information from which implementation-
     * specific specializations can be generated.
     */
    struct MaterialBaseInfo
    {
        shader::ShaderModule fragmentModule;
        bool transparent;
    };

    /**
     * Information for *internal* specialization of materials that is not
     * exposed to the user, but performed automatically.
     */
    struct MaterialSpecializationInfo
    {
        bool animated;
    };

    /**
     * Can be created from MaterialSpecializationInfo or directly from its
     * flag combination type.
     */
    struct MaterialKey
    {
        struct Flags
        {
            enum class Animated{ eFalse, eTrue, eMaxEnum };
        };

        using MaterialSpecializationFlags = FlagCombination<Flags::Animated>;

        struct Hash
        {
            constexpr auto operator()(const trc::MaterialKey& key) const -> size_t {
                return key.toUniqueIndex();
            }
        };

        constexpr MaterialKey(const MaterialSpecializationInfo& info)
        {
            if (info.animated) flags |= Flags::Animated::eTrue;
        }

        explicit
        constexpr MaterialKey(const MaterialSpecializationFlags& flags) : flags(flags) {}

        constexpr bool operator==(const MaterialKey& rhs) const {
            return flags.toIndex() == rhs.flags.toIndex();
        }

        constexpr auto toUniqueIndex() const -> ui32 {
            return flags.toIndex();
        }

        constexpr auto toSpecializationInfo() const -> MaterialSpecializationInfo
        {
            return {
                .animated=flags & Flags::Animated::eTrue
            };
        }

        static constexpr auto fromUniqueIndex(ui32 index) -> MaterialKey {
            return MaterialKey{ MaterialSpecializationFlags::fromIndex(index) };
        }

        static constexpr auto fromSpecializationInfo(const MaterialSpecializationInfo& info)
            -> MaterialKey
        {
            return MaterialKey{ info };
        }

        MaterialSpecializationFlags flags;
    };

    /**
     * Create a full shader program for Torch's render algorithm from a fragment
     * shader and additional specialization information.
     */
    auto makeDeferredMaterialSpecialization(const shader::ShaderModule& fragmentModule,
                                            const MaterialSpecializationInfo& info)
        -> shader::ShaderProgramData;

    /**
     * @brief Manages specializations for a material.
     *
     * Lazily creates material specializations (shader programs and
     * runtimes) when they're requested.
     */
    struct MaterialSpecializationCache
    {
    public:
        MaterialSpecializationCache(const MaterialSpecializationCache&) = default;
        MaterialSpecializationCache(MaterialSpecializationCache&&) noexcept = default;
        MaterialSpecializationCache& operator=(const MaterialSpecializationCache&) = default;
        MaterialSpecializationCache& operator=(MaterialSpecializationCache&&) noexcept = default;
        ~MaterialSpecializationCache() noexcept = default;

        explicit MaterialSpecializationCache(const MaterialBaseInfo& base);
        explicit MaterialSpecializationCache(const serial::MaterialProgramSpecializations& serial,
                                             shader::ShaderRuntimeConstantDeserializer& des);

        auto getSpecialization(const MaterialKey& key) -> const shader::ShaderProgramData&;

        /**
         * Force all specializations to be computed immediately.
         */
        void createAllSpecializations();

        /**
         * Will force-create all lazy specializations.
         */
        auto iterSpecializations()
            -> std::generator<std::pair<MaterialKey, const shader::ShaderProgramData&>>;

        auto serialize() const -> serial::MaterialProgramSpecializations;
        void serialize(serial::MaterialProgramSpecializations& out) const;

    private:
        static constexpr size_t kNumSpecializations
            = MaterialKey::MaterialSpecializationFlags::size();

        template<std::default_initializable T>
        using PerSpecialization = std::array<T, kNumSpecializations>;

        /** @return `nullptr` if the specialization cannot be created. */
        auto getOrCreateSpecialization(const MaterialKey& key)
            -> shader::ShaderProgramData&;

        std::optional<MaterialBaseInfo> base;
        PerSpecialization<std::optional<shader::ShaderProgramData>> shaderPrograms;
    };
} // namespace trc

template<>
struct std::hash<::trc::MaterialKey> : ::trc::MaterialKey::Hash {};
