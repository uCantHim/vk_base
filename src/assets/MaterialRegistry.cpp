#include "trc/assets/MaterialRegistry.h"

#include "material.pb.h"
#include "trc/DrawablePipelines.h"
#include "trc/assets/AssetManager.h"
#include "trc/drawable/DefaultDrawable.h"
#include "trc/material/TorchMaterialSettings.h"
#include "trc/material/VertexShader.h"



trc::AssetData<trc::Material>::AssetData(
    const shader::ShaderModule& fragModule,
    bool transparent)
    :
    transparent(transparent)
{
    auto specialize = [this, &fragModule](const MaterialSpecializationInfo& info)
    {
        auto vertexModule = VertexModule{ info.animated }.build(fragModule);
        auto [it, _] = programs.try_emplace(
            MaterialKey{ info },
            shader::linkShaderProgram(
                {
                    { vk::ShaderStageFlagBits::eVertex,   std::move(vertexModule) },
                    { vk::ShaderStageFlagBits::eFragment, fragModule },
                },
                makeProgramLinkerSettings()
            )
        );

        // Collect references to textures so we can resolve them at the asset
        // manager when a material is created from this data.
        for (auto& [stage, specConstants] : it->second.specConstants)
        {
            for (auto& [_, spec] : specConstants)
            {
                auto texture = std::dynamic_pointer_cast<RuntimeTextureIndex>(spec);
                if (!texture) {
                    throw std::runtime_error("Only texture references are allowed as"
                                             " specialization constants.");
                }
                textures.emplace_back(texture->getTextureReference());
            }
        }
    };

    specialize({ .animated=false });
    specialize({ .animated=true });
}

void trc::AssetData<trc::Material>::serialize(std::ostream& os) const
{
    serial::Material mat;

    // Serialize shader programs
    for (const auto& [key, program] : programs)
    {
        auto newSpec = mat.add_specializations();
        *newSpec->mutable_shader_program() = program.serialize();
        newSpec->set_animated(key.flags.has(MaterialKey::Flags::Animated::eTrue));
    }

    // Serialize default runtime values
    for (const auto& [pcId, data] : runtimeValueDefaults)
    {
        auto val = mat.add_runtime_values();
        val->set_push_constant_id(pcId);
        val->set_data(data.data(), data.size());
    }

    // Serialize pipeline settings
    auto settings = mat.mutable_settings();
    settings->set_transparent(transparent);
    if (polygonMode) settings->set_polygon_mode(serial::PolygonMode(*polygonMode));
    if (lineWidth) settings->set_line_width(*lineWidth);
    if (cullMode)
    {
        serial::CullMode cm = serial::CullMode::NONE;
        if (*cullMode == vk::CullModeFlagBits::eFrontAndBack) cm = serial::CullMode::FRONT_AND_BACK;
        else if (*cullMode == vk::CullModeFlagBits::eFront) cm = serial::CullMode::FRONT;
        else if (*cullMode == vk::CullModeFlagBits::eBack) cm = serial::CullMode::BACK;
        settings->set_cull_mode(cm);
    }
    if (frontFace) settings->set_front_face_clockwise(*frontFace == vk::FrontFace::eClockwise);

    if (depthWrite) settings->set_depth_write(*depthWrite);
    if (depthTest) settings->set_depth_test(*depthTest);
    if (depthBiasConstantFactor) settings->set_depth_bias_constant_factor(*depthBiasConstantFactor);
    if (depthBiasSlopeFactor) settings->set_depth_bias_slope_factor(*depthBiasSlopeFactor);

    mat.SerializeToOstream(&os);
}

void trc::AssetData<trc::Material>::deserialize(std::istream& is)
{
    serial::Material mat;
    mat.ParseFromIstream(&is);

    // Parse settings
    const auto& settings = mat.settings();
    transparent = settings.transparent();
    if (settings.has_polygon_mode()) polygonMode = vk::PolygonMode(settings.polygon_mode());
    if (settings.has_line_width()) lineWidth = settings.line_width();
    if (settings.has_cull_mode()) cullMode = vk::CullModeFlags(settings.cull_mode());
    if (settings.has_front_face_clockwise())
    {
        frontFace = settings.front_face_clockwise() ? vk::FrontFace::eClockwise
                                                    : vk::FrontFace::eCounterClockwise;
    }
    if (settings.has_depth_write()) depthWrite = settings.depth_write();
    if (settings.has_depth_test()) depthTest = settings.depth_test();
    if (settings.has_depth_bias_constant_factor()) depthBiasConstantFactor = settings.depth_bias_constant_factor();
    if (settings.has_depth_bias_slope_factor()) depthBiasSlopeFactor = settings.depth_bias_slope_factor();

    // Parse specializations
    programs.clear();
    for (const auto& spec : mat.specializations())
    {
        shader::ShaderProgramData program;
        program.deserialize(spec.shader_program(), *this);
        programs.try_emplace(
            MaterialSpecializationInfo{ .animated=spec.animated() },
            std::move(program)
        );
    }

    // Parse default runtime values
    for (const auto& val : mat.runtime_values())
    {
        const auto& data = val.data();
        runtimeValueDefaults.emplace_back(
            val.push_constant_id(),
            std::vector<std::byte>{
                reinterpret_cast<const std::byte*>(data.c_str()),
                reinterpret_cast<const std::byte*>(data.c_str() + data.size()),
            }
        );
    }
}

auto trc::AssetData<trc::Material>::deserialize(const std::string& data)
    -> std::optional<s_ptr<shader::ShaderRuntimeConstant>>
{
    // For now, we only have texture references as runtime constants.
    if (auto val = RuntimeTextureIndex::deserialize(data))
    {
        textures.emplace_back(val->getTextureReference());
        return val;
    }
    return std::nullopt;
}

void trc::AssetData<trc::Material>::resolveReferences(AssetManager& assetManager)
{
    for (auto& ref : textures) {
        ref.resolve(assetManager);
    }
}



auto trc::makeMaterialProgram(
    const MaterialData& data,
    const MaterialSpecializationInfo& specialization)
    -> u_ptr<MaterialProgram>
{
    const MaterialKey key{ specialization };

    // Use a pre-defined pipeline skeleton as a base for the material
    // specialization.
    const DrawablePipelineInfo info{
        .animated=key.flags.has(MaterialKey::Flags::Animated::eTrue),
        .transparent=data.transparent
    };
    Pipeline::ID basePipeline = pipelines::getDrawableBasePipeline(info.toPipelineFlags());

    auto [tmpl, renderPass] = PipelineRegistry::cloneGraphicsPipeline(basePipeline);
    auto pipelineData = tmpl.getPipelineData();

    { // Set pipeline parameters to the material's defaults
        auto& r = pipelineData.rasterization;
        auto& ds = pipelineData.depthStencil;
        if (data.polygonMode) r.setPolygonMode(*data.polygonMode);
        if (data.lineWidth) r.setLineWidth(*data.lineWidth);
        if (data.cullMode) r.setCullMode(*data.cullMode);
        if (data.frontFace) r.setFrontFace(*data.frontFace);
        if (data.depthWrite) ds.setDepthWriteEnable(*data.depthWrite);
        if (data.depthTest) ds.setDepthTestEnable(*data.depthTest);
        if (data.depthBiasConstantFactor) r.setDepthBiasConstantFactor(*data.depthBiasConstantFactor);
        if (data.depthBiasSlopeFactor) r.setDepthBiasSlopeFactor(*data.depthBiasSlopeFactor);
    }

    // Create the runtime program
    return std::make_unique<MaterialProgram>(
        data.programs.at(key),
        pipelineData,
        renderPass
    );
}



void trc::MaterialRegistry::update(vk::CommandBuffer, FrameRenderState&)
{
}

auto trc::MaterialRegistry::add(u_ptr<AssetSource<Material>> source) -> LocalID
{
    const LocalID id{ localIdPool.generate() };
    storage.emplace(id, SpecializationStorage{
        .data=source->load(),
        .shaderPrograms={ nullptr },
        .runtimes{},
    });

    return id;
}

void trc::MaterialRegistry::remove(LocalID id)
{
    assert(storage.contains(id));

    storage.erase(id);
    localIdPool.free(id);
}

auto trc::MaterialRegistry::getHandle(LocalID id) -> Handle
{
    assert(storage.contains(id));

    return Handle{ storage.at(id) };
}

auto trc::MaterialRegistry::SpecializationStorage::getSpecialization(const MaterialKey& key)
    -> MaterialRuntime
{
    auto& runtime = runtimes.at(key.flags.toIndex());
    if (!runtime)
    {
        assert(shaderPrograms.at(key.flags.toIndex()) == nullptr);
        auto& prog = shaderPrograms.at(key.flags.toIndex());
        prog = makeMaterialProgram(
            data,
            MaterialSpecializationInfo{
                .animated=key.flags.has(MaterialKey::Flags::Animated::eTrue)
            }
        );
        runtime = prog->cloneRuntime();
        for (const auto& [id, data] : data.runtimeValueDefaults) {
            runtime->setPushConstantDefaultValue(id, std::span{data});
        }
    }

    return *runtime;
}
