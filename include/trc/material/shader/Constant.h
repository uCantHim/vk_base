#pragma once

#include <cstddef>

#include <array>
#include <ostream>
#include <string>

#include "BasicType.h"
#include "trc/Types.h"

namespace trc::shader
{
    struct Constant
    {
    public:
        using LargestType = glm::dvec4;
        static constexpr size_t kMaxSize{ sizeof(LargestType) };

        Constant(bool val);
        Constant(i32 val);
        Constant(ui32 val);
        Constant(float val);
        Constant(double val);

        template<int N, typename T> requires (N >= 1 && N <= 4)
        Constant(glm::vec<N, T> value);

        Constant(BasicType type, std::array<std::byte, kMaxSize> data);

        auto getType() const -> BasicType;
        auto datatype() const -> std::string;
        auto toString() const -> std::string;

        template<typename T> requires (sizeof(T) <= sizeof(Constant::LargestType))
        auto as() const -> T;

    private:
        BasicType type;
        std::array<std::byte, kMaxSize> value;
    };

    template<int N, typename T>
        requires (N >= 1 && N <= 4)
    Constant::Constant(glm::vec<N, T> val)
        :
        type(toBasicTypeEnum<T>, N)
    {
        *reinterpret_cast<decltype(val)*>(value.data()) = val;
    }

    template<typename T>
        requires (sizeof(T) <= sizeof(Constant::LargestType))
    auto Constant::as() const -> T
    {
        return *reinterpret_cast<const T*>(value.data());
    }
} // namespace trc::shader
