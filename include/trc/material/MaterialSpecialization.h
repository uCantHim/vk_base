#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "MaterialRuntime.h"
#include "MaterialShaderProgram.h"
#include "trc/FlagCombination.h"
#include "trc/Types.h"

namespace trc
{
    /**
     * The user-defined material information from which implementation-
     * specific specializations can be generated.
     */
    struct MaterialBaseInfo
    {
        ShaderModule fragmentModule;
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
     * flag combination type
     */
    struct MaterialKey
    {
        struct Flags
        {
            enum class Animated{ eFalse, eTrue, eMaxEnum };
        };

        using MaterialSpecializationFlags = FlagCombination<
            Flags::Animated
        >;

        struct Hash
        {
            constexpr auto operator()(const trc::MaterialKey& key) const -> size_t {
                return key.flags.toIndex();
            }
        };

        MaterialKey() = default;

        MaterialKey(const MaterialSpecializationInfo& info)
        {
            if (info.animated) flags |= Flags::Animated::eTrue;
        }

        bool operator==(const MaterialKey& rhs) const {
            return flags.toIndex() == rhs.flags.toIndex();
        }

        MaterialSpecializationFlags flags;
    };
} // namespace trc

template<>
struct std::hash<trc::MaterialKey> : trc::MaterialKey::Hash {};
