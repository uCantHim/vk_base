#include <string>

#include <gtest/gtest.h>
#include <trc/material/shader/BasicType.h>

using trc::shader::BasicType;

TEST(TestBasicType, ConstructorFromType)
{
    const BasicType basicTypes[]{
        { bool{} },
        { int32_t{} },
        { uint32_t{} },
        { float{} },
        { double{} },
        { glm::bvec2{} }, { glm::vec2{} }, { glm::ivec2{} }, { glm::uvec2{} }, { glm::dvec2{} },
        { glm::bvec3{} }, { glm::vec3{} }, { glm::ivec3{} }, { glm::uvec3{} }, { glm::dvec3{} },
        { glm::bvec4{} }, { glm::vec4{} }, { glm::ivec4{} }, { glm::uvec4{} }, { glm::dvec4{} },
        { glm::mat3{} }, { glm::dmat3{} },
        { glm::mat4{} }, { glm::dmat4{} },
    };

    const uint32_t channels[]{
        1, 1, 1, 1, 1,
        2, 2, 2, 2, 2,
        3, 3, 3, 3, 3,
        4, 4, 4, 4, 4,
        9, 9,
        16, 16,
    };

    using Type = BasicType::Type;
    const BasicType::Type types[]{
        Type::eBool, Type::eSint, Type::eUint, Type::eFloat, Type::eDouble,
        Type::eBool, Type::eFloat, Type::eSint, Type::eUint, Type::eDouble,
        Type::eBool, Type::eFloat, Type::eSint, Type::eUint, Type::eDouble,
        Type::eBool, Type::eFloat, Type::eSint, Type::eUint, Type::eDouble,
        Type::eFloat, Type::eDouble,
        Type::eFloat, Type::eDouble,
    };
    const std::string strings[]{
        "bool", "int", "uint", "float", "double",
        "bvec2", "vec2", "ivec2", "uvec2", "dvec2",
        "bvec3", "vec3", "ivec3", "uvec3", "dvec3",
        "bvec4", "vec4", "ivec4", "uvec4", "dvec4",
        "mat3", "dmat3",
        "mat4", "dmat4",
    };
    const uint32_t locations[]{
        1, 1, 1, 1, 1,
        1, 1, 1, 1, 1,
        1, 1, 1, 1, 2,
        1, 1, 1, 1, 2,
        3, 6,
        4, 8,
    };
    const uint32_t sizes[]{
        4, 4, 4, 4, 8,
        8, 8, 8, 8, 16,
        12, 12, 12, 12, 24,
        16, 16, 16, 16, 32,
        36, 72,
        64, 128,
    };

    for (int i = 0; i < 24; ++i)
    {
        ASSERT_EQ(basicTypes[i].channels, channels[i]);
        ASSERT_EQ(basicTypes[i].type, types[i]);
        ASSERT_EQ(basicTypes[i].to_string(), strings[i]);
        ASSERT_EQ(basicTypes[i].size(),      sizes[i]);
        ASSERT_EQ(basicTypes[i].locations(), locations[i]);
    }
}
