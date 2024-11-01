#include "trc/material/shader/ShaderCodeBuilder.h"

#include <source_location>
#include <sstream>

#include "trc/material/shader/ShaderCodeCompiler.h"
#include "trc/material/shader/ShaderTypeChecker.h"



namespace trc::shader
{

using namespace code;

void ShaderCodeBuilder::startBlock(Function func)
{
    blockStack.push(func->getBlock());
}

void ShaderCodeBuilder::startBlock(Block block)
{
    blockStack.push(block);
}

void ShaderCodeBuilder::endBlock()
{
    assert(!blockStack.empty());
    blockStack.pop();
}

void ShaderCodeBuilder::makeReturn()
{
    makeStatement(Return{});
}

void ShaderCodeBuilder::makeReturn(Value retValue)
{
    makeStatement(Return{ retValue });
}

auto ShaderCodeBuilder::makeConstant(Constant c) -> Value
{
    return makeValue(Literal{ .value=c });
}

auto ShaderCodeBuilder::makeCall(Function func, std::vector<Value> args) -> Value
{
    return makeValue(FunctionCall{ .function=func, .args=args });
}

auto ShaderCodeBuilder::makeMemberAccess(Value val, const std::string& member) -> Value
{
    return makeValue(MemberAccess{ .lhs=val, .rhs=Identifier{ member } });
}

auto ShaderCodeBuilder::makeArrayAccess(Value array, Value index) -> Value
{
    return makeValue(ArrayAccess{ .lhs=array, .index=index });
}

auto ShaderCodeBuilder::makeCast(BasicType to, Value from) -> Value
{
    return makeConstructor(to, { from });
}

auto ShaderCodeBuilder::makeConstructor(Type type, const std::vector<Value>& args) -> Value
{
    auto res = makeExternalCall(code::types::to_string(type), args);
    annotateType(res, type);
    return res;
}

auto ShaderCodeBuilder::makeExternalCall(const std::string& funcName, std::vector<Value> args)
    -> Value
{
    return makeCall(makeOrGetBuiltinFunction(funcName), std::move(args));
}

auto ShaderCodeBuilder::makeExternalIdentifier(const std::string& id) -> Value
{
    return makeValue(Identifier{ id });
}

auto ShaderCodeBuilder::makeNot(Value val) -> Value
{
    return makeValue(UnaryOperator{ "!", val });
}

auto ShaderCodeBuilder::makeAdd(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="+", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeSub(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="-", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeMul(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="*", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeDiv(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="/", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeAnd(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="&&", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeOr(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="||", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeXor(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="^^", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeSmallerThan(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="<", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeGreaterThan(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName=">", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeSmallerOrEqual(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="<=", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeGreaterOrEqual(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName=">=", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeEqual(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="==", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeNotEqual(Value lhs, Value rhs) -> Value
{
    return makeValue(BinaryOperator{ .opName="!=", .lhs=lhs, .rhs=rhs });
}

auto ShaderCodeBuilder::makeConditional(Value cond, Value ifTrue, Value ifFalse) -> Value
{
    return makeValue(Conditional{ .condition=cond, .ifTrue=ifTrue, .ifFalse=ifFalse });
}

auto ShaderCodeBuilder::makeOrGetFunction(
    const std::string& name,
    FunctionType type) -> Function
{
    if (functions.contains(name)) {
        return functions.at(name);
    }

    auto block = blocks.emplace_back(std::make_shared<BlockT>());

    // Create argument identifiers
    std::vector<Value> argumentRefs;
    for (ui32 i = 0; i < type.argTypes.size(); ++i)
    {
        std::string argName = "_arg_" + std::to_string(i);
        argumentRefs.emplace_back(makeExternalIdentifier(argName));
    }

    return makeFunction(FunctionT{ name, std::move(type), block, std::move(argumentRefs) });
}

auto ShaderCodeBuilder::getFunction(const std::string& funcName) const -> std::optional<Function>
{
    auto it = functions.find(funcName);
    if (it != functions.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ShaderCodeBuilder::makeAssignment(code::Value lhs, code::Value rhs)
{
    makeStatement(code::Assignment{ .lhs=lhs, .rhs=rhs });
}

void ShaderCodeBuilder::makeCallStatement(Function func, std::vector<code::Value> args)
{
    makeStatement(code::FunctionCall{ func, std::move(args) });
}

void ShaderCodeBuilder::makeExternalCallStatement(
    const std::string& funcName,
    std::vector<code::Value> args)
{
    Function func = makeOrGetBuiltinFunction(funcName);
    makeStatement(code::FunctionCall{ func, std::move(args) });
}

auto ShaderCodeBuilder::makeIfStatement(Value condition) -> Block
{
    Block block = blocks.emplace_back(std::make_shared<BlockT>());
    makeStatement(IfStatement{ condition, block });

    return block;
}

void ShaderCodeBuilder::makeStatement(StmtT statement)
{
    if (blockStack.empty()) {
        throw std::runtime_error("[In ShaderCodeBuilder]: Cannot create a statement when no"
                                 " block is active. Create a block with `startBlock` first.");
    }

    blockStack.top()->statements.emplace_back(std::move(statement));
}

auto ShaderCodeBuilder::makeFunction(FunctionT func) -> Function
{
    auto [it, _] = functions.try_emplace(func.getName(), std::make_shared<FunctionT>(func));
    return it->second;
}

auto ShaderCodeBuilder::makeOrGetBuiltinFunction(const std::string& funcName) -> Function
{
    auto [it, _] = builtinFunctions.try_emplace(
        funcName,
        std::make_shared<FunctionT>(FunctionT{ funcName, {}, nullptr, {} })
    );
    return it->second;
}

auto ShaderCodeBuilder::makeStructType(
    const std::string& name,
    const std::vector<std::pair<Type, std::string>>& fields)
    -> StructType
{
    auto [it, success] = structTypes.try_emplace(
        name,
        new types::StructType{ .name=name, .fields=fields }
    );

    if (!success) {
        throw std::runtime_error(
            "[In " + std::string(std::source_location::current().function_name()) + "]: "
            "Tried to create a struct type \"" + name + "\", but a type with this name"
            " already exists."
        );
    }

    return it->second.get();
}

void ShaderCodeBuilder::annotateType(Value val, Type type)
{
    ((ValueT*)val.get())->typeAnnotation = type;
}

auto ShaderCodeBuilder::compileTypeDecls() const -> std::string
{
    std::string res;
    for (const auto& [name, type] : structTypes)
    {
        res += "struct " + name + "{\n";
        for (const auto& [fieldType, fieldName] : type->fields) {
            res += "    " + types::to_string(fieldType) + " " + fieldName + ";\n";
        }
        res += "};\n";
    }

    return res;
}

auto ShaderCodeBuilder::compileFunctionDecls(ResourceResolver& resolver) const -> std::string
{
    std::string forwardDecls;
    std::string res;
    for (const auto& [name, func] : functions)
    {
        auto& type = func->getType();
        auto funcHead = (type.returnType ? type.returnType->to_string() : "void")
                         + " " + func->getName() + "(";

        for (ui32 i = 0; const auto& argType : type.argTypes)
        {
            Value arg = func->getArgs()[i];
            assert(std::holds_alternative<Identifier>(arg->value));
            funcHead += argType.to_string() + " " + std::get<Identifier>(arg->value).name;
            if (++i < type.argTypes.size()) {
                funcHead += ", ";
            }
        }
        funcHead += ")";

        forwardDecls += funcHead + ";\n";
        res += funcHead + "\n{\n" + compile(func->body, resolver) + "}\n";
    }

    return forwardDecls + res;
}

auto ShaderCodeBuilder::compile(Value value, ResourceResolver& resolver)
    -> std::pair<std::string, std::string>
{
    return ShaderValueCompiler{ resolver }.compile(std::move(value));
}

auto ShaderCodeBuilder::compile(Block block, ResourceResolver& resolver) -> std::string
{
    return ShaderBlockCompiler{ resolver }.compile(std::move(block));
}

} // namespace trc::shader
