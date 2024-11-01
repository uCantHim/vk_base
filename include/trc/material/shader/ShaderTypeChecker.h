#pragma once

#include <optional>

#include "ShaderCodeBuilder.h"

namespace trc::shader
{

class ShaderTypeChecker
{
public:
    using Value = ShaderCodeBuilder::Value;

    struct TypeInferenceResult
    {
        using Type = ShaderCodeBuilder::Type;
        using StructType = ShaderCodeBuilder::StructType;

        TypeInferenceResult(const Type& type) : type(type) {}
        TypeInferenceResult(BasicType type) : type(type) {}
        TypeInferenceResult(StructType type) : type(type) {}

        /** @return std::string The type's name as a string. */
        auto to_string() const -> std::string;

        bool isBasicType() const;
        bool isStructType() const;

        bool operator==(const TypeInferenceResult& other) const {
            return type == other.type;
        }

        Type type;
    };

    auto inferType(Value value) -> std::optional<TypeInferenceResult>;

    auto operator()(const code::Literal& v) -> std::optional<TypeInferenceResult>;
    auto operator()(const code::Identifier& v) -> std::optional<TypeInferenceResult>;
    auto operator()(const code::FunctionCall& v) -> std::optional<TypeInferenceResult>;
    auto operator()(const code::UnaryOperator& v) -> std::optional<TypeInferenceResult>;
    auto operator()(const code::BinaryOperator& v) -> std::optional<TypeInferenceResult>;
    auto operator()(const code::MemberAccess& v) -> std::optional<TypeInferenceResult>;
    auto operator()(const code::ArrayAccess& v) -> std::optional<TypeInferenceResult>;
    auto operator()(const code::Conditional& v) -> std::optional<TypeInferenceResult>;
    auto operator()(const code::CapabilityAccess& v) -> std::optional<TypeInferenceResult>;
    auto operator()(const code::RuntimeConstant& v) -> std::optional<TypeInferenceResult>;
};

} // namespace trc::shader
