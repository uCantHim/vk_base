#include "trc/material/MaterialSpecialization.h"

#include "trc/material/TorchMaterialSettings.h"
#include "trc/material/VertexShader.h"



namespace trc
{

auto makeDeferredMaterialSpecialization(const shader::ShaderModule& fragmentModule,
                                        const MaterialSpecializationInfo& info)
    -> shader::ShaderProgramData
{
    auto vertexModule = VertexModule{ info.animated }.build(fragmentModule);
    return shader::linkShaderProgram(
        {
            { vk::ShaderStageFlagBits::eVertex,   std::move(vertexModule) },
            { vk::ShaderStageFlagBits::eFragment, fragmentModule },
        },
        makeProgramLinkerSettings()
    );
}

auto makeDeferredMaterialSpecialization(const MaterialBaseInfo& baseInfo,
                                        const MaterialSpecializationInfo& info)
    -> shader::ShaderProgramData
{
    return makeDeferredMaterialSpecialization(baseInfo.fragmentModule, info);
}



MaterialSpecializationCache::MaterialSpecializationCache(const MaterialBaseInfo& base)
    :
    base(base)
{
}

MaterialSpecializationCache::MaterialSpecializationCache(
    const serial::MaterialProgramSpecializations& serial,
    shader::ShaderRuntimeConstantDeserializer& des)
    :
    base(std::nullopt)
{
    for (const auto& [i, prog] : std::views::enumerate(serial.specializations())) {
        shaderPrograms.at(i)->deserialize(prog.shader_program(), des);
    }
}

auto MaterialSpecializationCache::getSpecialization(const MaterialKey& key)
    -> const shader::ShaderProgramData&
{
    return getOrCreateSpecialization(key);
}

void MaterialSpecializationCache::createAllSpecializations()
{
    for (auto&& _ : iterSpecializations()) {}
}

auto MaterialSpecializationCache::iterSpecializations()
    -> std::generator<std::pair<MaterialKey, const shader::ShaderProgramData&>>
{
    for (const auto& [i, prog] : std::views::enumerate(shaderPrograms))
    {
        const auto key = MaterialKey::fromUniqueIndex(i);
        co_yield { key, getOrCreateSpecialization(key) };
    }
}

auto MaterialSpecializationCache::serialize() const -> serial::MaterialProgramSpecializations
{
    serial::MaterialProgramSpecializations res;
    serialize(res);
    return res;
}

void MaterialSpecializationCache::serialize(serial::MaterialProgramSpecializations& out) const
{
    out.clear_specializations();
    for (const auto& [i, prog] : std::views::enumerate(shaderPrograms))
    {
        assert(prog || base);  // The constructor interface is designed in a way
                               // that this should always be true.

        const auto key = MaterialKey::fromUniqueIndex(i);
        auto newSpec = out.add_specializations();
        newSpec->set_animated(key.flags & MaterialKey::Flags::Animated::eTrue);

        if (prog) {
            *newSpec->mutable_shader_program() = prog->serialize();
        }
        else {
            assert(base);
            auto spec = createSpecialization(*base, key);
            *newSpec->mutable_shader_program() = spec.serialize();
        }
    }
}

auto MaterialSpecializationCache::getOrCreateSpecialization(const MaterialKey& key)
    -> shader::ShaderProgramData&
{
    auto& program = shaderPrograms[key.toUniqueIndex()];
    if (!program)
    {
        assert(base);  // The constructor interface is designed in a way that
                       // this should always be true.
        program = createSpecialization(*base, key);
    }

    return program.value();
}

auto MaterialSpecializationCache::createSpecialization(
    const MaterialBaseInfo& base,
    const MaterialKey& key)
    -> shader::ShaderProgramData
{
    return makeDeferredMaterialSpecialization(base.fragmentModule, key.toSpecializationInfo());
}

} // namespace trc
