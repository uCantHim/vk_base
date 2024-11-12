#include "trc/material/shader/ShaderModuleBuilder.h"

#include <fstream>
#include <ranges>

#include <shader_tools/ShaderDocument.h>

#include "trc/material/shader/ShaderCodeCompiler.h"



namespace trc::shader
{

ShaderFunction::ShaderFunction(const std::string& name, FunctionType type)
    :
    name(name),
    signature(std::move(type))
{
}

auto ShaderFunction::getName() const -> const std::string&
{
    return name;
}

auto ShaderFunction::getType() const -> const FunctionType&
{
    return signature;
}



auto ShaderModuleBuilder::makeCall(ShaderFunction& func, std::vector<Value> args) -> Value
{
    return ShaderCodeBuilder::makeCall(getOrMakeFunctionDef(func), std::move(args));
}

void ShaderModuleBuilder::makeCallStatement(ShaderFunction& func, std::vector<code::Value> args)
{
    makeStatement(code::FunctionCall{ getOrMakeFunctionDef(func), std::move(args) });
}

auto ShaderModuleBuilder::makeCapabilityAccess(Capability capability) -> Value
{
    return makeValue(code::CapabilityAccess{ capability });
}

auto ShaderModuleBuilder::makeSpecializationConstant(s_ptr<ShaderRuntimeConstant> constant)
    -> Value
{
    return makeValue(code::RuntimeConstant{ constant });
}

auto ShaderModuleBuilder::makeOutputLocation(ui32 location, BasicType type) -> Value
{
    const std::string name{ "_shaderOutput_" + std::to_string(location) };

    auto [it, success] = outputLocations.try_emplace(location, type, name);
    if (!success && it->second.first != type)
    {
        throw std::invalid_argument(
            "[In ShaderModuleBuilder::makeOutputLocation]: Output at location "
            + std::to_string(location) + " is already declared with the type "
            + it->second.first.to_string() + " (provided type was " + type.to_string() + ")"
        );
    }

    return makeExternalIdentifier(name);
}

void ShaderModuleBuilder::includeCode(
    fs::path path,
    std::unordered_map<std::string, Capability> varReplacements)
{
    auto it = std::ranges::find_if(includedFiles,
                                   [&path](auto& a){ return a.first == path; });

    // Only insert the pair if `path` does not yet exist in the list
    if (it == includedFiles.end())
    {
        includedFiles.push_back({ std::move(path), {} });
        auto& [_, map] = includedFiles.back();
        for (auto& [name, capability] : varReplacements) {
            map.try_emplace(std::move(name), makeCapabilityAccess(capability));
        }
    }
}

void ShaderModuleBuilder::setVersionString(std::string str)
{
    shaderSettings.versionString = std::move(str);
}

void ShaderModuleBuilder::enableEarlyFragmentTest()
{
    shaderSettings.earlyFragmentTests = true;
}

void ShaderModuleBuilder::disableEarlyFragmentTest()
{
    shaderSettings.earlyFragmentTests = false;
}

auto ShaderModuleBuilder::getSettings() const -> const Settings&
{
    return shaderSettings;
}

auto ShaderModuleBuilder::compileOutputLocations() const -> std::string
{
    std::string res;
    for (const auto& [location, pair] : outputLocations)
    {
        const auto& [type, name] = pair;
        res += "layout (location = " + std::to_string(location) + ")"
               " out " + type.to_string() + " " + name + ";\n";
    }

    return res;
}

auto ShaderModuleBuilder::compileIncludedCode(
    shaderc::CompileOptions::IncluderInterface& includer,
    ResourceResolver& resolver)
    -> std::string
{
    std::string result;
    for (const auto& [path, vars] : includedFiles)
    {
        const auto str = path.string();
        const auto include = includer.GetInclude(
            str.c_str(), shaderc_include_type_standard,
            "shader_module_dummy_include_name", 1);

        if (include->source_name_length == 0) {
            throw std::runtime_error("[In ShaderModuleBuilder::compileIncludedCode]: "
                                     + std::string(include->content));
        }

        std::ifstream file{ include->source_name };
        includer.ReleaseInclude(include);

        shader_edit::ShaderDocument doc(file);
        for (const auto& [name, value] : vars)
        {
            auto [id, code] = ShaderValueCompiler{ resolver, true }.compile(value);
            doc.set(name, id);
        }

        result += doc.compile();
    }

    return result;
}

auto ShaderModuleBuilder::getOrMakeFunctionDef(ShaderFunction& funcBuilder) -> Function
{
    if (auto func = getFunction(funcBuilder.getName())) {
        return *func;
    }

    auto func = makeOrGetFunction(funcBuilder.getName(), funcBuilder.getType());
    startBlock(func);
    funcBuilder.build(*this, func->getArgs());
    endBlock();

    return func;
}

}
