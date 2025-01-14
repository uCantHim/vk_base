#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <variant>

using TypeName = std::string;

static const TypeName stringTypeName{ "String" };
static const TypeName floatTypeName{ "Float" };
static const TypeName intTypeName{ "Int" };
static const TypeName globalObjectTypeName{ "__global" };
static const TypeName undefinedObjectType{ "<undefined object>" };

struct StringType
{
    const TypeName typeName{ stringTypeName };
};

struct FloatType
{
    const TypeName typeName{ floatTypeName };
};

struct IntType
{
    const TypeName typeName{ intTypeName };
};

enum class FieldType
{
    eSingleValue,
    eList,
    eMap
};

struct ObjectType
{
    struct Field
    {
        TypeName storedType;
        FieldType fieldType{ FieldType::eSingleValue };
    };

    TypeName typeName;
    std::unordered_map<std::string, Field> fields;

    bool hasField(const std::string& fieldName) const {
        return fields.contains(fieldName);
    }

    auto getRequiredType(const std::string& fieldName) const -> const TypeName& {
        return fields.at(fieldName).storedType;
    }

    auto getFieldType(const std::string& fieldName) const -> FieldType {
        return fields.at(fieldName).fieldType;
    }
};

struct EnumType
{
    TypeName typeName;
    std::unordered_set<std::string> options;

    bool hasOption(const std::string& opt) const {
        return options.contains(opt);
    }
};

using TypeType = std::variant<
    StringType,
    FloatType,
    IntType,
    ObjectType,
    EnumType
>;

inline auto getTypeName(const TypeType& type) -> const TypeName&
{
    return std::visit([](auto&& v) -> const TypeName& { return v.typeName; }, type);
}

struct TypeConfiguration
{
    TypeConfiguration(std::unordered_map<TypeName, TypeType> types)
        : types(std::move(types))
    {}

    std::unordered_map<TypeName, TypeType> types;
};

auto makeDefaultTypeConfig() -> TypeConfiguration;
