#include "trc/material/shader/Constant.h"

#include <sstream>



namespace trc::shader
{

Constant::Constant(bool val)
    :
    type(BasicType::Type::eBool, 1)
{
    *reinterpret_cast<bool*>(value.data()) = val;
}

Constant::Constant(i32 val)
    :
    type(BasicType::Type::eSint, 1)
{
    *reinterpret_cast<i32*>(value.data()) = val;
}

Constant::Constant(ui32 val)
    :
    type(BasicType::Type::eUint, 1)
{
    *reinterpret_cast<ui32*>(value.data()) = val;
}

Constant::Constant(float val)
    :
    type(BasicType::Type::eFloat, 1)
{
    *reinterpret_cast<float*>(value.data()) = val;
}

Constant::Constant(double val)
    :
    type(BasicType::Type::eDouble, 1)
{
    *reinterpret_cast<double*>(value.data()) = val;
}

Constant::Constant(BasicType type, std::array<std::byte, kMaxSize> data)
    :
    type(type),
    value(data)
{
}

auto Constant::getType() const -> BasicType
{
    return type;
}

auto Constant::datatype() const -> std::string
{
    return type.to_string();
}

auto Constant::toString() const -> std::string
{
    std::string res;
    const size_t minCapacity = 4 + 2 + getType().channels * 3;
    res.reserve(minCapacity);

    res += datatype() + "(";
    for (ui8 i = 0; i < getType().channels; ++i)
    {
        switch (getType().type)
        {
        case BasicType::Type::eBool:   res += std::to_string(as<glm::bvec4>()[i]); break;
        case BasicType::Type::eSint:   res += std::to_string(as<ivec4>()[i]); break;
        case BasicType::Type::eUint:   res += std::to_string(as<uvec4>()[i]); break;
        case BasicType::Type::eFloat:  res += std::to_string(as<vec4>()[i]); break;
        case BasicType::Type::eDouble: res += std::to_string(as<glm::dvec4>()[i]); break;
        }
        if (i < getType().channels - 1) res += ", ";
    }
    res += ")";

    return res;
}

} // namespace trc::shader
