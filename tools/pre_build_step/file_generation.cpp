#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#include "Compiler.h"
#include "ConfigParserJson.h"

using namespace shader_edit;

void generateAssetRegistryDescriptorFile();

int main()
{
    generateAssetRegistryDescriptorFile();

    return 0;
}



void generateAssetRegistryDescriptorFile()
{
    constexpr const char* VAR_SEP{ "-" };
    constexpr const char* NAME_TAG_SEP{ ":" };

    const fs::path configDir{ CONFIG_DIR };
    const fs::path shaderDir{ SHADER_OUT_DIR };

    std::ifstream configFile(configDir / "shader_compile_config.json");
    assert(configFile.is_open());

    CompileConfiguration compileConfig = parseConfigJson(configFile);
    compileConfig.meta.basePath = configDir;
    compileConfig.meta.outDir = shaderDir;

    CompileResult result;
    try {
        result = Compiler::compile(compileConfig);
    }
    catch (const std::exception& err) {
        std::cout << "-- Error during shader generation: " << err.what() << "\n\n";
        throw err;
    }

    for (auto& shader : result.shaderFiles)
    {
        std::stringstream ss;
        for (int i = 0; const auto& [name, var] : shader.variablesToValues)
        {
            if (!var.tag.empty())
            {
                if (i++ != 0) ss << VAR_SEP;
                ss << name << NAME_TAG_SEP << var.tag;
            }
        }

        const std::string ext = shader.filePath.extension().string();
        shader.filePath.replace_extension(ss.str() + ext);

        std::ofstream out(shader.filePath);
        out << shader.code;
    }
}
