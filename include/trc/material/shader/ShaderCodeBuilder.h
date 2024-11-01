#pragma once

#include <concepts>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "CodePrimitives.h"
#include "Constant.h"
#include "trc/Types.h"

namespace trc::shader
{
    class ResourceResolver;

    class ShaderCodeBuilder
    {
    public:
        using Function = code::Function;
        using Block = code::Block;
        using Value = code::Value;
        using Type = code::Type;

        using StructType = const code::types::StructType*;

        ShaderCodeBuilder& operator=(const ShaderCodeBuilder&) = delete;

        ShaderCodeBuilder() = default;
        ShaderCodeBuilder(const ShaderCodeBuilder&) = default;
        ShaderCodeBuilder(ShaderCodeBuilder&&) noexcept = default;
        ~ShaderCodeBuilder() noexcept = default;

        ShaderCodeBuilder& operator=(ShaderCodeBuilder&&) noexcept = default;

        void startBlock(Function function);
        void startBlock(Block block);
        void endBlock();

        void makeReturn();
        void makeReturn(Value retValue);

        auto makeConstant(Constant c) -> Value;
        auto makeCall(Function func, std::vector<Value> args) -> Value;
        auto makeMemberAccess(Value val, const std::string& member) -> Value;
        auto makeArrayAccess(Value array, Value index) -> Value;

        /**
         * @brief Cast a value to a basic type
         *
         * @tparam CastTo A type to which to cast the value. `BasicType` must be
         *                constructible from an object of type `CastTo`.
         * @param Value from The value that is cast to `CastTo`.
         *
         * @return Value The value resulting from the cast expression.
         *
         * # Example
         *
         * ```cpp
         * auto myVec = builder.makeCast<vec3>(myFloat);
         * ```
         */
        template<typename CastTo>
        inline auto makeCast(Value from) -> Value {
            return makeCast(BasicType{ CastTo{} }, std::move(from));
        }

        /**
         * @brief Cast a value to a basic type
         *
         * @param Value     from The value to cast to another type.
         * @param BasicType to   The type to which to cast the value.
         *
         * @return Value The value resulting from the cast expression.
         */
        auto makeCast(BasicType to, Value from) -> Value;

        template<typename Constructed, typename ...Args>
            requires (std::same_as<Value, std::remove_cvref_t<Args>> && ...)
        auto makeConstructor(Args&&... args) -> Value {
            return makeConstructor(BasicType{ Constructed{} }, std::forward<Args>(args)...);
        }

        template<typename ...Args>
            requires (std::same_as<Value, std::remove_cvref_t<Args>> && ...)
        auto makeConstructor(Type type, Args&&... args) -> Value;

        template<typename Constructed>
        auto makeConstructor(const std::vector<Value>& args) -> Value {
            return makeConstructor(BasicType{ Constructed{} }, args);
        }

        auto makeConstructor(Type type, const std::vector<Value>& args) -> Value;

        auto makeExternalIdentifier(const std::string& id) -> Value;
        auto makeExternalCall(const std::string& funcName, std::vector<Value> args) -> Value;

        /** @brief Create a unary boolean NOT operation */
        auto makeNot(Value val) -> Value;
        auto makeAdd(Value lhs, Value rhs) -> Value;
        auto makeSub(Value lhs, Value rhs) -> Value;
        auto makeMul(Value lhs, Value rhs) -> Value;
        auto makeDiv(Value lhs, Value rhs) -> Value;

        auto makeAnd(Value lhs, Value rhs) -> Value;
        auto makeOr(Value lhs, Value rhs) -> Value;
        auto makeXor(Value lhs, Value rhs) -> Value;

        auto makeSmallerThan(Value lhs, Value rhs) -> Value;
        auto makeGreaterThan(Value lhs, Value rhs) -> Value;
        auto makeSmallerOrEqual(Value lhs, Value rhs) -> Value;
        auto makeGreaterOrEqual(Value lhs, Value rhs) -> Value;
        auto makeEqual(Value lhs, Value rhs) -> Value;
        auto makeNotEqual(Value lhs, Value rhs) -> Value;

        auto makeConditional(Value cond, Value ifTrue, Value ifFalse) -> Value;

        auto makeOrGetFunction(const std::string& name, FunctionType type) -> Function;
        auto getFunction(const std::string& name) const -> std::optional<Function>;

        /**
         * Only use this for assignments with side-effects, such as assigning
         * to gl_Position.
         */
        void makeAssignment(code::Value lhs, code::Value rhs);
        void makeCallStatement(Function func, std::vector<code::Value> args);
        void makeExternalCallStatement(const std::string& funcName, std::vector<code::Value> args);
        auto makeIfStatement(Value condition) -> Block;

        auto makeStructType(const std::string& name,
                            const std::vector<std::pair<Type, std::string>>& fields)
            -> StructType;

        void annotateType(Value val, Type type);

        auto compileTypeDecls() const -> std::string;
        auto compileFunctionDecls(ResourceResolver& resolver) const -> std::string;

        /**
         * @return pair [<identifier>, <code>] where <identifier> is a GLSL
         *         variable name that refers to the computed value, and
         *         <code> is GLSL code that declares some intermediate
         *         variables requried for the computation of <identfier>
         *         as well as <identfier> itself.
         *         <code> *must* precede any use of <identifier> in subsequent
         *         code.
         */
        static auto compile(Value value, ResourceResolver& resolver)
            -> std::pair<std::string, std::string>;
        static auto compile(Block block, ResourceResolver& resolver)
            -> std::string;

    protected:
        template<typename T>
        auto makeValue(T&& val) -> Value;
        void makeStatement(code::StmtT statement);
        auto makeFunction(code::FunctionT func) -> Function;
        auto makeOrGetBuiltinFunction(const std::string& funcName) -> Function;

    private:
        std::unordered_map<std::string, Function> functions;
        std::unordered_map<std::string, Function> builtinFunctions;

        std::unordered_map<std::string, s_ptr<code::types::StructType>> structTypes;

        std::vector<Block> blocks;

        /**
         * The block stack does not necessarily signify block nesting in
         * the code - it just remembers which block is currently being
         * operated on when creating statements.
         */
        std::stack<Block> blockStack;
    };



    template<typename ...Args>
        requires (std::same_as<ShaderCodeBuilder::Value, std::remove_cvref_t<Args>> && ...)
    inline auto ShaderCodeBuilder::makeConstructor(Type type, Args&&... args) -> Value
    {
        return makeConstructor(type, { std::forward<Args>(args)... });
    }

    template<typename T>
    inline auto ShaderCodeBuilder::makeValue(T&& val) -> Value
    {
        return std::make_shared<code::ValueT>(
            code::ValueT{ .value=std::forward<T>(val), .typeAnnotation=std::nullopt }
        );
    }
} // namespace trc::shader
