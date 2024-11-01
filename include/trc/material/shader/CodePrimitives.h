#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <trc_util/TypeUtils.h>

#include "BasicType.h"
#include "Capability.h"
#include "Constant.h"
#include "trc/Types.h"

namespace trc::shader
{
    class ShaderCodeBuilder;
    class ShaderRuntimeConstant;

    struct FunctionType
    {
        std::vector<BasicType> argTypes;
        std::optional<BasicType> returnType;
    };

    namespace code::types
    {
        struct StructType;

        /**
         * @brief Any type; either a basic type or a structure type
         */
        using TypeT = std::variant<
            BasicType,
            const StructType*
        >;

        struct StructType
        {
            std::string name;
            std::vector<std::pair<TypeT, std::string>> fields;

            auto to_string() const -> const std::string& { return name; }
            auto getName() const -> const std::string& { return name; }

            /**
             * @brief Calculate the type's size in bytes
             */
            auto size() const -> ui32
            {
                ui32 size{ 0 };
                for (const auto& [type, _] : fields)
                {
                    size += std::visit(util::VariantVisitor{
                        [](const BasicType& type)  { return type.size(); },
                        [](const StructType* type) { return type->size(); }
                    }, type);
                }

                return size;
            }
        };

        /**
         * @brief Get a type's name
         */
        inline auto to_string(const TypeT& type)
        {
            return std::visit(
                util::VariantVisitor{
                    [](BasicType type)         -> std::string { return type.to_string(); },
                    [](const StructType* type) -> std::string { return type->to_string(); }
                },
                type
            );
        }

        /**
         * @brief Get a type's size in bytes
         */
        inline auto getTypeSize(const TypeT& type)
        {
            return std::visit(
                util::VariantVisitor{
                    [](BasicType type)         { return type.size(); },
                    [](const StructType* type) { return type->size(); }
                },
                type
            );
        }
    } // namespace code::types

    namespace code
    {
        struct Literal;
        struct Identifier;
        struct FunctionCall;
        struct UnaryOperator;
        struct BinaryOperator;
        struct MemberAccess;
        struct ArrayAccess;
        struct Conditional;

        // vvv These are a bit special as they *should* live outside of the core
        //     shader code building functionality: They relate more to resources
        //     used by a shader module than to shader code itself. They still
        //     live here because I was not able to think of a better way to make
        //     the shader code tree independent of a specific capability config.
        struct CapabilityAccess;
        struct RuntimeConstant;
        // ^^^

        struct ValueT;

        struct Return;
        struct Assignment;
        struct IfStatement;

        using StmtT = std::variant<
            Return,
            Assignment,
            IfStatement,
            FunctionCall // Re-use the Value-type struct as a statement
        >;

        struct FunctionT;

        struct BlockT
        {
            std::vector<StmtT> statements;
        };

        using Function = s_ptr<const FunctionT>;
        using Block = s_ptr<BlockT>;
        using Value = s_ptr<const ValueT>;
        using Type = types::TypeT;


        // --- Value types --- //

        struct Literal
        {
            Constant value;
        };

        struct Identifier
        {
            std::string name;
        };

        struct FunctionCall
        {
            Function function;
            std::vector<Value> args;
        };

        struct UnaryOperator
        {
            std::string opName;
            Value operand;
        };

        struct BinaryOperator
        {
            std::string opName;
            Value lhs;
            Value rhs;
        };

        struct MemberAccess
        {
            Value lhs;
            Identifier rhs;
        };

        struct ArrayAccess
        {
            Value lhs;
            Value index;
        };

        struct Conditional
        {
            Value condition;
            Value ifTrue;
            Value ifFalse;
        };

        struct CapabilityAccess
        {
            Capability capability;
        };

        struct RuntimeConstant
        {
            s_ptr<ShaderRuntimeConstant> runtimeValue;
        };

        struct ValueT
        {
            std::variant<
                Literal,
                Identifier,
                FunctionCall,
                UnaryOperator,
                BinaryOperator,
                MemberAccess,
                ArrayAccess,
                Conditional,
                CapabilityAccess,
                RuntimeConstant
            > value;

            std::optional<Type> typeAnnotation;
        };


        // --- Statement types --- //

        struct Return
        {
            std::optional<Value> val;
        };

        struct Assignment
        {
            code::Value lhs;
            code::Value rhs;
        };

        struct IfStatement
        {
            code::Value condition;
            Block block;
        };


        // --- Function type --- //

        struct FunctionT
        {
            auto getName() const -> const std::string& {
                return name;
            }

            auto getType() const -> const FunctionType& {
                return type;
            }

            auto getArgs() const -> const std::vector<Value>& {
                return argumentRefs;
            }

            auto getBlock() const -> Block {
                return body;
            }

            std::string name;
            FunctionType type;

            Block body;
            std::vector<Value> argumentRefs;
        };
    } // namespace code
} // namespace trc::shader
