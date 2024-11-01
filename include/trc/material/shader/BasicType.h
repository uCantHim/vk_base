#pragma once

#include <cassert>

#include <ostream>
#include <string>
#include <type_traits>

#include "trc/Types.h"

namespace trc::shader
{
    struct BasicType
    {
        /**
         * Is ordered in a way that numerical comparison of the enum values
         * reflects supertype-relations among the types.
         */
        enum class Type : ui8
        {
            eBool,
            eUint,
            eSint,
            eFloat,
            eDouble,
        };

        constexpr BasicType(bool val);
        constexpr BasicType(i32 val);
        constexpr BasicType(ui32 val);
        constexpr BasicType(float val);
        constexpr BasicType(double val);

        template<i32 N, typename T>
        constexpr BasicType(glm::vec<N, T>);

        template<i32 N, typename T>
            requires ((N == 3 || N == 4) && (std::is_same_v<T, float> || std::is_same_v<T, double>))
        constexpr BasicType(glm::mat<N, N, T>);

        constexpr BasicType(Type t, ui8 channels);

        auto operator<=>(const BasicType&) const = default;

        auto to_string() const -> std::string;

        /** @return ui32 Size of the type in bytes */
        auto size() const -> ui32;

        /** @return ui32 The number of shader locations the type occupies */
        auto locations() const -> ui32;

        Type type;
        ui8 channels;
    };

    auto operator<<(std::ostream& os, const BasicType& t) -> std::ostream&;

    template<typename T>
    BasicType::Type toBasicTypeEnum;

    template<> inline constexpr BasicType::Type toBasicTypeEnum<bool>   = BasicType::Type::eBool;
    template<> inline constexpr BasicType::Type toBasicTypeEnum<i32>    = BasicType::Type::eSint;
    template<> inline constexpr BasicType::Type toBasicTypeEnum<ui32>   = BasicType::Type::eUint;
    template<> inline constexpr BasicType::Type toBasicTypeEnum<float>  = BasicType::Type::eFloat;
    template<> inline constexpr BasicType::Type toBasicTypeEnum<double> = BasicType::Type::eDouble;

    constexpr BasicType::BasicType(bool)
        : type(Type::eBool), channels(1)
    {}

    constexpr BasicType::BasicType(i32)
        : type(Type::eSint), channels(1)
    {}

    constexpr BasicType::BasicType(ui32)
        : type(Type::eUint), channels(1)
    {}

    constexpr BasicType::BasicType(float)
        : type(Type::eFloat), channels(1)
    {}

    constexpr BasicType::BasicType(double)
        : type(Type::eDouble), channels(1)
    {}

    template<i32 N, typename T>
    constexpr BasicType::BasicType(glm::vec<N, T>)
        : type(toBasicTypeEnum<T>), channels(N)
    {
        assert(channels > 0);
    }

    template<i32 N, typename T>
        requires ((N == 3 || N == 4) && (std::is_same_v<T, float> || std::is_same_v<T, double>))
    constexpr BasicType::BasicType(glm::mat<N, N, T>)
        : type(toBasicTypeEnum<T>), channels(N * N)
    {
        assert(channels > 0);
    }

    constexpr BasicType::BasicType(Type t, ui8 channels)
        :
        type(t),
        channels(channels)
    {
        assert(channels > 0);
    }
} // namespace trc::shader
