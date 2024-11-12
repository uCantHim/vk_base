#include "trc/ShaderLoader.h"

#include <cstring>

#include <fstream>

#include <nlohmann/json.hpp>
#include <shader_tools/ShaderDocument.h>
#include <spirv/FileIncluder.h>
#include <trc_util/Util.h>

#include "trc/Types.h"
#include "trc/base/Logging.h"
#include "trc/base/ShaderProgram.h"



namespace trc
{

namespace nl = nlohmann;

ShaderLoader::ShaderLoader(
    std::vector<fs::path> _includePaths,
    fs::path binaryPath,
    std::optional<fs::path> shaderDbFile,
    shaderc::CompileOptions opts)
    :
    includePaths(std::move(_includePaths)),
    outDir(std::move(binaryPath)),
    compileOpts(std::move(opts))
{
    if (shaderDbFile.has_value() && fs::is_regular_file(*shaderDbFile))
    {
        std::ifstream file(*shaderDbFile);
        if (!file.is_open()) {
            throw std::invalid_argument("[In ShaderLoader::ShaderLoader]: Unable to open shader"
                                        " database " + shaderDbFile->string());
        }

        try {
            shaderDatabase = ShaderDB(nl::json::parse(file));
        }
        catch (const nl::json::parse_error& err) {
            throw std::invalid_argument("[In ShaderLoader::ShaderLoader]: Unable to load shader"
                                        " database: " + std::string(err.what()));
        }
    }

    if (fs::exists(outDir) && !fs::is_directory(outDir)) {
        throw std::invalid_argument("[In ShaderLoader::ShaderLoader]: Object at binary directory"
                                    " path " + outDir.string() + " exists but is not a directory!");
    }
    fs::create_directories(outDir);

    compileOpts.SetIncluder(std::make_unique<spirv::FileIncluder>(includePaths));
}

auto ShaderLoader::makeDefaultOptions() -> shaderc::CompileOptions
{
    shaderc::CompileOptions opts;

#ifdef TRC_FLIP_Y_PROJECTION
    opts.AddMacroDefinition("TRC_FLIP_Y_AXIS");
#endif
    opts.SetTargetSpirv(shaderc_spirv_version_1_5);
    opts.SetTargetEnvironment(shaderc_target_env::shaderc_target_env_vulkan,
                              shaderc_env_version_vulkan_1_3);
    opts.SetOptimizationLevel(shaderc_optimization_level_performance);

    return opts;
}

auto ShaderLoader::load(ShaderPath shaderPath) const -> std::vector<ui32>
{
    /**
     * The longest possible dependency chain is:
     *
     *     file  -->  GLSL source  -->  SPIRV code
     *
     * where `file` contains unset variables. This intermediate 'true' GLSL
     * source is generated in `findFile` if it is outdated.
     */

    const auto srcPathOpt = findShaderSource(shaderPath.getSourceName());
    if (srcPathOpt)
    {
        assert(fs::is_regular_file(*srcPathOpt));

        const auto& srcPath = *srcPathOpt;
        const auto binPath = outDir / shaderPath.getBinaryName();

        if (binaryDirty(srcPath, binPath))
        {
            return compile(srcPath, binPath);
        }

        return readSpirvFile(binPath);
    }

    throw std::out_of_range("[In ShaderLoader::load]: Shader source "
                            + shaderPath.getSourceName().string() + " not found.");
}

bool ShaderLoader::binaryDirty(const fs::path& srcPath, const fs::path& binPath)
{
    return !fs::is_regular_file(binPath)
        || fs::last_write_time(srcPath) > fs::last_write_time(binPath)
        || fs::file_size(binPath) == 0;
}

auto ShaderLoader::findShaderSource(const util::Pathlet& filePath) const -> std::optional<fs::path>
{
    // Helper that searches all include paths for a file
    auto find = [this](const util::Pathlet& filename) -> std::optional<fs::path> {
        for (const auto& includePath : includePaths)
        {
            const auto file = includePath / filename;
            if (fs::is_regular_file(file)) {
                return file;
            }
        }
        return std::nullopt;
    };

    // Try to find the file in the shader database. This uses additional
    // information to regenerate the shader source if necessary.
    if (shaderDatabase)
    {
        if (auto shader = shaderDatabase->get(filePath.string()))
        {
            auto rawSourcePath = find(shader->source);

            // Regenerate the shader source if it does not exist or is outdated
            auto dstFile = find(filePath);
            if (rawSourcePath && (!dstFile || binaryDirty(*rawSourcePath, *dstFile)))
            {
                log::info << "Regenerate shader source " << filePath.string()
                          << " from " << *rawSourcePath << "\n";

                std::ifstream rawSource(*rawSourcePath);
                shader_edit::ShaderDocument doc(rawSource);
                for (const auto& [key, val] : shader->variables) {
                    doc.set(key, val);
                }

                const auto outPath = outDir / filePath;
                fs::create_directories(outPath.parent_path());
                std::ofstream outFile(outPath);
                outFile << doc.compile();

                return outPath;
            }
            else if (dstFile) {
                // File exists and is up-to-date. It might just be considered
                // up-to-date because no raw source was found for comparison.
                return *dstFile;
            }
            else {
                // File does not exist and there is no way to regenerate it
                return std::nullopt;
            }
        }
    }

    // There is no shader database, so just try to find the file. Return
    // nothing if it does not exist.
    if (auto res = find(filePath)) {
        return res;
    }

    return std::nullopt;
}

auto ShaderLoader::compile(const fs::path& srcPath, const fs::path& dstPath) const
    -> std::vector<ui32>
{
    assert(fs::is_regular_file(srcPath));

    log::info << "Compiling shader " << srcPath << " to " << dstPath;

    auto result = spirv::generateSpirv(util::readFile(srcPath), srcPath, compileOpts);
    if (result.GetCompilationStatus()
        != shaderc_compilation_status::shaderc_compilation_status_success)
    {
        throw std::runtime_error("[In ShaderLoader::load]: Compile error when compiling shader"
                                 " source " + srcPath.string() + " to SPIRV: "
                                 + result.GetErrorMessage());
    }

    std::vector<ui32> code{ result.cbegin(), result.cend() };

    fs::create_directories(dstPath.parent_path());
    std::ofstream file(dstPath, std::ios::binary);
    file.write(reinterpret_cast<char*>(code.data()), code.size() * sizeof(ui32));

    return code;
}


ShaderLoader::ShaderDB::ShaderDB(nl::json json)
    :
    db(std::move(json))
{
}

auto ShaderLoader::ShaderDB::get(std::string_view path) const -> std::optional<ShaderInfo>
{
    auto it = db.find(path);
    if (it != db.end())
    {
        return ShaderInfo{
            .source=util::Pathlet(it->at("source").get_ref<const std::string&>()),
            .target=util::Pathlet(it->at("target").get_ref<const std::string&>()),
            .variables=it->at("variables").get<std::unordered_map<std::string, std::string>>()
        };
    }

    return std::nullopt;
}

} // namespace trc
