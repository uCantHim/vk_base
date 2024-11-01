#include "trc/material/shader/ShaderTypeChecker.h"

#include <trc_util/Util.h>

#include "trc/material/shader/ShaderRuntimeConstant.h"



namespace trc::shader
{

auto ShaderTypeChecker::TypeInferenceResult::to_string() const -> std::string
{
    return code::types::to_string(type);
}

bool ShaderTypeChecker::TypeInferenceResult::isBasicType() const
{
    return std::holds_alternative<BasicType>(type);
}

bool ShaderTypeChecker::TypeInferenceResult::isStructType() const
{
    return std::holds_alternative<StructType>(type);
}



auto ShaderTypeChecker::inferType(Value value) -> std::optional<TypeInferenceResult>
{
    if (value->typeAnnotation) {
        return *value->typeAnnotation;
    }

    return std::visit(*this, value->value);
}

auto ShaderTypeChecker::operator()(const code::Literal& v)
    -> std::optional<TypeInferenceResult>
{
    return v.value.getType();
}

auto ShaderTypeChecker::operator()(const code::Identifier&)
    -> std::optional<TypeInferenceResult>
{
    return std::nullopt;
}

auto ShaderTypeChecker::operator()(const code::FunctionCall& v)
    -> std::optional<TypeInferenceResult>
{
    return v.function->getType().returnType;
}

auto ShaderTypeChecker::operator()(const code::UnaryOperator& v)
    -> std::optional<TypeInferenceResult>
{
    return inferType(v.operand);
}

auto ShaderTypeChecker::operator()(const code::BinaryOperator& v)
    -> std::optional<TypeInferenceResult>
{
    if (v.opName == "<"
        || v.opName == "<="
        || v.opName == ">"
        || v.opName == ">="
        || v.opName == "=="
        || v.opName == "!=")
    {
        return BasicType{ bool{} };
    }

    const auto r = inferType(v.rhs);
    const auto l = inferType(v.lhs);

    // No type checking can be performed if either of the operands has an
    // undefined type
    if (!l || !r) {
        return std::nullopt;
    }

    // Applying structure type to an operator call should even be an error, but
    // we'll settle for a none-result.
    if (l->isStructType() || r->isStructType()) {
        return std::nullopt;
    }

    // If none of the types is a matrix, return the bigger of the two types
    const auto _l = std::get<BasicType>(l->type);
    const auto _r = std::get<BasicType>(r->type);
    if (_l.channels <= 4 && _r.channels <= 4)
    {
        if (_l.channels < _r.channels) {
            return r;
        }
        return l;
    }

    // Just get the right-hand-side operand's type because that's correct in
    // the case of matrix-vector multiplication:
    //     mat3(1.0f) * vec3(1, 2, 3) -> vec3
    return inferType(v.rhs);
}

auto ShaderTypeChecker::operator()(const code::MemberAccess& obj)
    -> std::optional<TypeInferenceResult>
{
    using StructType = TypeInferenceResult::StructType;

    if (auto lhsType = inferType(obj.lhs))
    {
        std::visit(
            util::VariantVisitor{
                [](BasicType)        -> std::optional<TypeInferenceResult> { return std::nullopt; },
                [&](StructType type) -> std::optional<TypeInferenceResult>
                {
                    auto pred = [&obj](const auto& field){ return field.second == obj.rhs.name; };
                    auto it = std::ranges::find_if(type->fields, pred);
                    if (it != type->fields.end()) {
                        return it->first;
                    }

                    // The struct type does not contain the field specified by the right hand
                    // side identifier.
                    // This case will result in an error when the shader code is compiled.
                    return std::nullopt;
                }
            },
            lhsType->type
        );
    }
    return std::nullopt;
}

auto ShaderTypeChecker::operator()(const code::ArrayAccess& v)
    -> std::optional<TypeInferenceResult>
{
    return inferType(v.lhs);
}

auto ShaderTypeChecker::operator()(const code::Conditional& cond)
    -> std::optional<TypeInferenceResult>
{
    // We should raise an error if the two cases of the conditional don't have
    // the same type, but we don't have such a mechanism right now.

    const auto l = inferType(cond.ifTrue);
    const auto r = inferType(cond.ifFalse);
    if (l && r && (*l == *r)) {
        return l;
    }

    return std::nullopt;
}

auto ShaderTypeChecker::operator()(const code::CapabilityAccess&)
    -> std::optional<TypeInferenceResult>
{
    return std::nullopt;
}

auto ShaderTypeChecker::operator()(const code::RuntimeConstant& v)
    -> std::optional<TypeInferenceResult>
{
    return v.runtimeValue->getType();
}

} // namespace trc::shader
