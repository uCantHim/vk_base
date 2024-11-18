#include "trc/assets/MaterialRegistry.h"

#include "material.pb.h"
#include "trc/DrawablePipelines.h"
#include "trc/assets/AssetManager.h"
#include "trc/drawable/DefaultDrawable.h"
#include "trc/material/TorchMaterialSettings.h"



trc::AssetData<trc::Material>::AssetData(const MaterialBaseInfo& createInfo)
    :
    shaderProgram(createInfo),
    transparent(createInfo.transparent)
{
    for (const auto& [_, program] : shaderProgram.iterSpecializations())
    {
        // Collect references to textures so we can resolve them at the asset
        // manager when a material is created from this data.
        for (auto& [stage, specConstants] : program.specConstants)
        {
            for (auto& [_, spec] : specConstants)
            {
                auto texture = std::dynamic_pointer_cast<RuntimeTextureIndex>(spec);
                if (!texture) {
                    throw std::runtime_error("Only texture references are allowed as"
                                             " specialization constants.");
                }
                requiredTextures.emplace_back(texture->getTextureReference());
            }
        }
    }
}

trc::AssetData<trc::Material>::AssetData(MaterialSpecializationCache specializations)
    :
    shaderProgram(std::move(specializations))
{
}

void trc::AssetSerializerTraits<trc::Material>::serialize(
    const trc::MaterialData& data,
    std::ostream& os)
{
    serial::Material mat;

    // Serialize shader programs
    data.shaderProgram.serialize(*mat.mutable_specializations());

    // Serialize default runtime values
    for (const auto& [pcId, data] : data.runtimeValueDefaults)
    {
        auto val = mat.add_runtime_values();
        val->set_push_constant_id(pcId);
        val->set_data(data.data(), data.size());
    }

    // Serialize pipeline settings
    auto settings = mat.mutable_settings();
    settings->set_transparent(data.transparent);
    if (data.polygonMode) settings->set_polygon_mode(serial::PolygonMode(*data.polygonMode));
    if (data.lineWidth) settings->set_line_width(*data.lineWidth);
    if (data.cullMode)
    {
        serial::CullMode cm = serial::CullMode::NONE;
        if (*data.cullMode == vk::CullModeFlagBits::eFrontAndBack) cm = serial::CullMode::FRONT_AND_BACK;
        else if (*data.cullMode == vk::CullModeFlagBits::eFront) cm = serial::CullMode::FRONT;
        else if (*data.cullMode == vk::CullModeFlagBits::eBack) cm = serial::CullMode::BACK;
        settings->set_cull_mode(cm);
    }
    if (data.frontFace) settings->set_front_face_clockwise(*data.frontFace == vk::FrontFace::eClockwise);

    if (data.depthWrite) settings->set_depth_write(*data.depthWrite);
    if (data.depthTest) settings->set_depth_test(*data.depthTest);
    if (data.depthBiasConstantFactor) settings->set_depth_bias_constant_factor(*data.depthBiasConstantFactor);
    if (data.depthBiasSlopeFactor) settings->set_depth_bias_slope_factor(*data.depthBiasSlopeFactor);

    mat.SerializeToOstream(&os);
}

auto trc::AssetSerializerTraits<trc::Material>::deserialize(std::istream& is)
    -> AssetParseResult<trc::Material>
{
    serial::Material mat;
    mat.ParseFromIstream(&is);

    // Create result data
    MaterialData::RuntimeConstantDeserializer runtimeConstants;
    MaterialData res{ MaterialSpecializationCache{ mat.specializations(), runtimeConstants } };

    // Parse settings
    const auto& settings = mat.settings();
    res.transparent = settings.transparent();
    if (settings.has_polygon_mode()) res.polygonMode = vk::PolygonMode(settings.polygon_mode());
    if (settings.has_line_width()) res.lineWidth = settings.line_width();
    if (settings.has_cull_mode()) res.cullMode = vk::CullModeFlags(settings.cull_mode());
    if (settings.has_front_face_clockwise())
    {
        res.frontFace = settings.front_face_clockwise() ? vk::FrontFace::eClockwise
                                                        : vk::FrontFace::eCounterClockwise;
    }
    if (settings.has_depth_write()) res.depthWrite = settings.depth_write();
    if (settings.has_depth_test()) res.depthTest = settings.depth_test();
    if (settings.has_depth_bias_constant_factor()) res.depthBiasConstantFactor = settings.depth_bias_constant_factor();
    if (settings.has_depth_bias_slope_factor()) res.depthBiasSlopeFactor = settings.depth_bias_slope_factor();

    // Parse default runtime values
    for (const auto& val : mat.runtime_values())
    {
        const auto& data = val.data();
        res.runtimeValueDefaults.emplace_back(
            val.push_constant_id(),
            std::vector<std::byte>{
                reinterpret_cast<const std::byte*>(data.c_str()),
                reinterpret_cast<const std::byte*>(data.c_str() + data.size()),
            }
        );
    }

    // Store runtime constants to resolve them later on
    res.requiredTextures = runtimeConstants.loadedTextures;

    return res;
}

void trc::AssetData<trc::Material>::resolveReferences(AssetManager& assetManager)
{
    for (auto& ref : requiredTextures) {
        ref.resolve(assetManager);
    }
}

auto trc::AssetData<trc::Material>::RuntimeConstantDeserializer::deserialize(const std::string& data)
    -> std::optional<s_ptr<shader::ShaderRuntimeConstant>>
{
    // For now, we only have texture references as runtime constants.
    if (auto val = RuntimeTextureIndex::deserialize(data))
    {
        loadedTextures.emplace_back(val->getTextureReference());
        return val;
    }
    return std::nullopt;
}



auto trc::makeMaterialProgram(
    MaterialData& data,
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
        data.shaderProgram.getSpecialization(key),
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
