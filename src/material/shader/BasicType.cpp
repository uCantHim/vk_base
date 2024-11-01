#include "trc/material/shader/BasicType.h"



namespace trc::shader
{

auto longTypename(BasicType::Type type) -> std::string
{
    switch (type)
    {
    case BasicType::Type::eBool:   return "bool";
    case BasicType::Type::eSint:   return "int";
    case BasicType::Type::eUint:   return "uint";
    case BasicType::Type::eFloat:  return "float";
    case BasicType::Type::eDouble: return "double";
    }

    throw std::logic_error("");
}

auto shortTypename(BasicType::Type type) -> std::string
{
    switch (type)
    {
    case BasicType::Type::eBool:   return "b";
    case BasicType::Type::eSint:   return "i";
    case BasicType::Type::eUint:   return "u";
    case BasicType::Type::eFloat:  return "";
    case BasicType::Type::eDouble: return "d";
    }

    throw std::logic_error("");
}



auto BasicType::to_string() const -> std::string
{
    assert(channels > 0);
    assert(channels <= 4 || (channels == 9 || channels == 16));

    switch (channels)
    {
    case 1: return longTypename(type);
    case 2: return shortTypename(type) + "vec2";
    case 3: return shortTypename(type) + "vec3";
    case 4: return shortTypename(type) + "vec4";
    case 9: return shortTypename(type) + "mat3";
    case 16: return shortTypename(type) + "mat4";
    }

    throw std::logic_error("");
}

auto BasicType::size() const -> ui32
{
    const ui32 baseSize = type == Type::eDouble ? 8 : 4;
    return channels * baseSize;
}

auto BasicType::locations() const -> ui32
{
    constexpr auto typeSize = [](Type type) {
        return type == Type::eDouble ? 2 : 1;
    };
    constexpr ui32 locationSize = 4;

    // The formula doesn't work here because I'd need to pad a matrix's columns to
    // 4 components. I want to have a channel count of 9 for the 3x3 matrix, so I
    // employ this hack instead of using 12 channels for 3x3.
    if (channels == 9) return 3 * typeSize(type);

    // upper(size * channels / 4)
    // where size is a single channel's size (either 'single' or 'double').
    return (typeSize(type) * channels + (locationSize - 1)) / locationSize;
}

auto operator<<(std::ostream& os, const BasicType& t) -> std::ostream&
{
    os << t.to_string();
    return os;
}

} // namespace trc::shader
