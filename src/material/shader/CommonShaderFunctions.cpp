#include "trc/material/shader/CommonShaderFunctions.h"



namespace trc::shader
{
    TangentToWorldspace::TangentToWorldspace()
        : ShaderFunction("TangentspaceToWorldspace", FunctionType{ { vec3{} }, vec3{} })
    {
    }

    void TangentToWorldspace::build(
        ShaderModuleBuilder& builder,
        const std::vector<code::Value>& args)
} // namespace trc::shader
