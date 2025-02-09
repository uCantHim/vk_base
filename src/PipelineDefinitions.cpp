#include "trc/PipelineDefinitions.h"

#include "trc/base/ShaderProgram.h"

#include "trc/util/TorchDirectories.h"
#include "trc/ShaderLoader.h"
#include "trc/ShaderPath.h"



auto trc::internal::getShaderLoader() -> const ShaderLoader&
{
    static ShaderLoader loader(
        {
            util::getInternalShaderStorageDirectory(),
            util::getInternalShaderBinaryDirectory()
        },
        // Binary output directory:
        util::getInternalShaderBinaryDirectory(),
        fs::path{ TRC_SHADER_DB }
    );

    return loader;
}

auto trc::internal::loadShader(const ShaderPath& path) -> std::vector<ui32>
{
    return getShaderLoader().load(path);
}
