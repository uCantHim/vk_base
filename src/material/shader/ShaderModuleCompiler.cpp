#include "trc/material/shader/ShaderModuleCompiler.h"

#include "trc/material/shader/DefaultResourceResolver.h"
#include "trc/material/shader/ShaderCodeCompiler.h"
#include "trc/util/TorchDirectories.h"



namespace trc::shader
{

ShaderModule::ShaderModule(
    std::string shaderCode,
    ShaderResourceInterface resourceInfo)
    :
    ShaderResourceInterface(std::move(resourceInfo)),
    shaderGlslCode(std::move(shaderCode))
{
}

auto ShaderModule::getShaderCode() const -> const std::string&
{
    return shaderGlslCode;
}



auto ShaderModuleCompiler::compile(
    const ShaderOutputInterface& outputs,
    ShaderModuleBuilder builder,
    const CapabilityConfig& caps)
    -> ShaderModule
{
    // Create and build the main function
    auto main = builder.makeOrGetFunction("main", FunctionType{ {}, std::nullopt });
    builder.startBlock(main);
    outputs.buildStatements(builder);
    builder.endBlock();

    // Generate resource and function declarations
    spirv::FileIncluder includer{
        {
            util::getInternalShaderBinaryDirectory(),
            util::getInternalShaderStorageDirectory(),
        }
    };
    CapabilityConfigResourceResolver resolver{ caps, builder };

    const auto includedCode = builder.compileIncludedCode(includer, resolver);
    const auto typeDeclCode = builder.compileTypeDecls();
    const auto functionDeclCode = builder.compileFunctionDecls(resolver); // Compiles all code.
    const auto resources = resolver.buildResources();  // Finalizes definitions of resources
                                                       // required by the compiled shader code.

    // Build the shader file
    std::stringstream ss;

    // Write module settings and version
    ss << compileSettings(builder.getSettings()) << "\n";

    // Write type definitions
    ss << typeDeclCode << "\n";

    // Write resources
    ss << resources.getGlslCode() << "\n";
    ss << builder.compileOutputLocations() << "\n";

    // Write additional includes
    ss << includedCode << "\n";

    // Write function definitions
    // This also writes the main function.
    ss << functionDeclCode;

    return { ss.str(), resources };
}

auto ShaderModuleCompiler::compileSettings(const ShaderModuleBuilder::Settings& settings)
    -> std::string
{
    std::string result;
    result += "#version " + settings.versionString + "\n";
    if (settings.earlyFragmentTests) {
        result += "layout (early_fragment_tests) in;\n";
    }

    return result;
}

} // namespace trc::shader
