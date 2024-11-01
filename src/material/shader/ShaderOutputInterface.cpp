#include "trc/material/shader/ShaderOutputInterface.h"

#include "trc/material/shader/ShaderCodeBuilder.h"



namespace trc::shader
{

void ShaderOutputInterface::makeStore(code::Value dst, code::Value value)
{
    stores.emplace_back(dst, value);
}

void ShaderOutputInterface::makeFunctionCall(code::Function func, std::vector<code::Value> args)
{
    functionCalls.emplace_back(std::move(func), std::move(args));
}

void ShaderOutputInterface::makeBuiltinCall(std::string funcName, std::vector<code::Value> args)
{
    externalCalls.emplace_back(std::move(funcName), std::move(args));
}

void ShaderOutputInterface::buildStatements(ShaderCodeBuilder& builder) const
{
    for (auto [dst, value] : stores) {
        builder.makeAssignment(dst, value);
    }
    for (const auto& [func, args] : functionCalls) {
        builder.makeCallStatement(func, args);
    }
    for (const auto& [name, args] : externalCalls) {
        builder.makeExternalCallStatement(name, args);
    }
}

} // namespace trc::shader
