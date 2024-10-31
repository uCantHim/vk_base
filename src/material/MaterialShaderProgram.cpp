#include "trc/material/MaterialShaderProgram.h"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include <shader_tools/ShaderDocument.h>
#include <spirv/CompileSpirv.h>
#include <trc_util/Timer.h>
#include <trc_util/algorithm/VectorTransform.h>

#include "material_shader_program.pb.h"
#include "trc/ShaderLoader.h"
#include "trc/VulkanInclude.h"
#include "trc/assets/import/InternalFormat.h"
#include "trc/base/Logging.h"
#include "trc/core/PipelineRegistry.h"
#include "trc/util/TorchDirectories.h"



auto shaderStageToExtension(vk::ShaderStageFlagBits stage) -> std::string
{
    switch (stage)
    {
    case vk::ShaderStageFlagBits::eVertex: return ".vert";
    case vk::ShaderStageFlagBits::eGeometry: return ".geom";
    case vk::ShaderStageFlagBits::eTessellationControl: return ".tese";
    case vk::ShaderStageFlagBits::eTessellationEvaluation: return ".tesc";
    case vk::ShaderStageFlagBits::eFragment: return ".frag";

    case vk::ShaderStageFlagBits::eTaskEXT: return ".task";
    case vk::ShaderStageFlagBits::eMeshEXT: return ".mesh";

    case vk::ShaderStageFlagBits::eRaygenKHR: return ".rgen";
    case vk::ShaderStageFlagBits::eIntersectionKHR: return ".rint";
    case vk::ShaderStageFlagBits::eMissKHR: return ".rmiss";
    case vk::ShaderStageFlagBits::eAnyHitKHR: return ".rahit";
    case vk::ShaderStageFlagBits::eClosestHitKHR: return ".rchit";
    case vk::ShaderStageFlagBits::eCallableKHR: return ".rcall";
    default:
        throw std::runtime_error("[In shaderStageToExtension]: Shader stage "
                                 + vk::to_string(stage) + " is not implemented.");
    }

    assert(false);
    throw std::logic_error("");
}

namespace trc
{

using ShaderStageMap = std::unordered_map<vk::ShaderStageFlagBits, ShaderModule>;

auto compileShader(
    vk::ShaderStageFlagBits shaderStage,
    const std::string& glslCode)
    -> std::vector<ui32>
{
    // Set up environment
    auto includer = std::make_unique<spirv::FileIncluder>(
        util::getInternalShaderStorageDirectory(),
        std::vector<fs::path>{ util::getInternalShaderBinaryDirectory() }
    );
    shaderc::CompileOptions opts{ ShaderLoader::makeDefaultOptions() };
    opts.SetIncluder(std::move(includer));

    // Compile source
    const auto result = spirv::generateSpirv(
        glslCode,
        "foo" + shaderStageToExtension(shaderStage),
        opts
    );
    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        throw std::runtime_error("[In MaterialShaderProgram::compileShader]: Compile error when"
                                 " compiling source of shader stage " + vk::to_string(shaderStage)
                                 + " to SPIRV: " + result.GetErrorMessage());
    }

    return { result.begin(), result.end() };
}

auto compileProgram(
    const ShaderStageMap& stages,
    const std::vector<PipelineLayoutTemplate::Descriptor>& descriptors,
    const std::vector<MaterialProgramData::PushConstantRange>& pushConstants)
    -> std::unordered_map<vk::ShaderStageFlagBits, std::vector<ui32>>
{
    std::unordered_map<vk::ShaderStageFlagBits, std::vector<ui32>> result;
    for (const auto& [stage, mod] : stages)
    {
        Timer timer;

        // Set descriptor indices in the shader code
        shader_edit::ShaderDocument doc(mod.getGlslCode());
        for (ui32 i = 0; const auto& desc : descriptors)
        {
            const auto& descName = desc.name.identifier;
            if (auto varName = mod.getDescriptorIndexPlaceholder(descName)) {
                doc.set(*varName, i);
            }
            ++i;
        }

        // Set push constant offsets in the shader code
        for (const auto& pc : pushConstants)
        {
            if (auto varName = mod.getPushConstantOffsetPlaceholder(pc.userId)) {
                doc.set(*varName, pc.offset);
            }
        }

        // Try to compile to SPIRV
        try {
            result.emplace(stage, compileShader(stage, doc.compile()));
            log::info << "Compiling GLSL code for " << vk::to_string(stage) << " stage to SPIRV"
                      << " (" << timer.reset() << " ms)";
        }
        catch (const shader_edit::CompileError& err)
        {
            log::info << "Compiling GLSL code for " << vk::to_string(stage) << " stage to SPIRV.";
            log::error << "[In linkMaterialProgram]: Unable to finalize shader code for"
                       << " SPIRV conversion (shader stage " << vk::to_string(stage) << ")"
                       << ": " << err.what();
            throw err;
        }
        catch (const std::runtime_error& err)
        {
            log::info << "Compiling GLSL code for " << vk::to_string(stage) << " stage to SPIRV"
                      << " -- ERROR!";
            log::error << "[In linkMaterialProgram]: Unable to compile shader code for stage "
                       << vk::to_string(stage) << " to SPIRV: " << err.what() << "\n"
                       << "  >>> Tried to compile the following shader code:\n\n"
                       << doc.compile()
                       << "\n+++++ END SHADER CODE +++++\n";
        }
    }

    return result;
}

auto collectDescriptorSets(const ShaderStageMap& stages, const ShaderDescriptorConfig& descConfig)
    -> std::vector<PipelineLayoutTemplate::Descriptor>
{
    // Collect required descriptor sets
    std::unordered_set<std::string> requiredDescriptorSets;
    for (const auto& [_, shader] : stages)
    {
        const auto& sets = shader.getRequiredDescriptorSets();
        requiredDescriptorSets.insert(sets.begin(), sets.end());
    }

    // Check that all required descriptor sets are defined
    for (auto it = requiredDescriptorSets.begin(); it != requiredDescriptorSets.end(); /**/)
    {
        const std::string& setName = *it;
        if (!descConfig.descriptorInfos.contains(setName))
        {
            log::error << log::here() << ": Shader program requires the descriptor set \""
                       << setName << "\", but no descriptor set with that name is defined in the"
                       << " ShaderDescriptorConfig object passed to makeMaterialProgram.";
            it = requiredDescriptorSets.erase(it);
        }
        else {
            ++it;
        }
    }

    // Assign descriptor sets to their respective indices
    std::vector<PipelineLayoutTemplate::Descriptor> result;
    for (const auto& name : requiredDescriptorSets)
    {
        const bool isStatic = descConfig.descriptorInfos.at(name).isStatic;
        result.push_back({ DescriptorName{ std::move(name) }, isStatic });
    }

    // Compare descriptor sets based on their preferred index
    auto compDescriptorSetIndex = [&](auto& a, auto& b) -> bool {
        const auto& infos = descConfig.descriptorInfos;
        return infos.at(a.name.identifier).index < infos.at(b.name.identifier).index;
    };
    std::ranges::sort(result, compDescriptorSetIndex);

    return result;
}

// TODO: This is a quick hack. I have to implement much more sophisticated merging
// of push constant ranges for all possible shader stages.
auto collectPushConstants(const ShaderStageMap& stages)
    -> std::vector<MaterialProgramData::PushConstantRange>
{
    using Range = MaterialProgramData::PushConstantRange;

    std::vector<Range> pcRanges;
    ui32 totalOffset{ 0 };
    for (const auto& [stage, mod] : stages)
    {
        if (mod.getPushConstantSize() <= 0) continue;

        for (const auto& pc : mod.getPushConstants())
        {
            pcRanges.push_back(Range{
                .offset=pc.offset + totalOffset,
                .size=pc.size,
                .shaderStage=stage,
                .userId=pc.userId,
            });
        }

        /**
         * The push constants of each stage have their offsets specified with
         * respect to all preceding ranges of the same stage. To each range of
         * subsequent stages, add the total offset of all previous stages.
         *
         * The 16-byte padding is, strictly speaking, slightly over-secure.
         * Theoretically, each member must be offset by a multiple of its own
         * alignment. However, I don't want to figure out the the first member
         * in the next push constant range and deduce its alignment from its
         * type right now. 16 bytes are the largest possible alignment (e.g. of
         * 4x4 matrices) and it always works.
         */
        totalOffset += util::pad_16(mod.getPushConstantSize());
    }

    return pcRanges;
}

auto combinePushConstantsPerStage(
    const std::vector<MaterialProgramData::PushConstantRange>& pushConstants)
    -> std::unordered_map<vk::ShaderStageFlagBits, vk::PushConstantRange>
{
    // Combine push constant ranges into a single one for each shader stage
    std::unordered_map<vk::ShaderStageFlagBits, vk::PushConstantRange> perStage;
    for (const auto& range : pushConstants)
    {
        constexpr ui32 _off = std::numeric_limits<ui32>::max();
        auto [it, _] = perStage.try_emplace(range.shaderStage,
                                            vk::PushConstantRange(range.shaderStage, _off, 0));
        vk::PushConstantRange& totalRange = it->second;
        totalRange.size += range.size;
        totalRange.offset = std::min(totalRange.offset, range.offset);
    }

    return perStage;
}

auto linkMaterialProgram(
    std::unordered_map<vk::ShaderStageFlagBits, ShaderModule> stages,
    const ShaderDescriptorConfig& descConfig)
    -> MaterialProgramData
{
    MaterialProgramData data;

    // Collect specialization constants
    for (const auto& [stage, mod] : stages)
    {
        auto& specs = data.specConstants.try_emplace(stage).first->second;
        for (const auto& spec : mod.getSpecializationConstants()) {
            specs.emplace_back(spec.specializationConstantIndex, spec.value);
        }
    }
    data.pushConstants = collectPushConstants(stages);
    data.pcRangesPerStage = combinePushConstantsPerStage(data.pushConstants);
    data.descriptorSets = collectDescriptorSets(stages, descConfig);

    // Compile each shader module to SPIR-V
    data.spirvCode = compileProgram(stages, data.descriptorSets, data.pushConstants);

    return data;
}

auto MaterialProgramData::makeLayout() const -> PipelineLayoutTemplate
{
    // Convert to PipelineLayoutTemplate's format
    std::vector<PipelineLayoutTemplate::PushConstant> pushConstants;
    for (const auto& [stage, range] : pcRangesPerStage)
    {
        pushConstants.push_back({
            .range=range,
            .defaultValue=std::nullopt
        });
    }

    return PipelineLayoutTemplate{ descriptorSets, std::move(pushConstants) };
}

auto MaterialProgramData::serialize() const -> serial::ShaderProgram
{
    serial::ShaderProgram prog;
    for (const auto& [stage, mod] : spirvCode)
    {
        auto newModule = prog.add_shader_modules();
        newModule->set_spirv_code(mod.data(), mod.size() * sizeof(ui32));
        newModule->set_stage(static_cast<serial::ShaderStageBit>(stage));

        if (specConstants.contains(stage))
        {
            for (const auto& [idx, val] : specConstants.at(stage)) {
                newModule->mutable_specialization_constants()->emplace(idx, val->serialize());
            }
        }
    }

    for (const auto& range : pushConstants)
    {
        auto pc = prog.add_push_constants();
        pc->set_offset(range.offset);
        pc->set_size(range.size);
        pc->set_shader_stage_flags(static_cast<ui32>(range.shaderStage));
        pc->set_user_id(range.userId);
    }

    for (const auto& desc : descriptorSets)
    {
        auto newSet = prog.add_descriptor_sets();
        newSet->set_name(desc.name.identifier);
        newSet->set_is_static(desc.isStatic);
    }

    return prog;
}

void MaterialProgramData::deserialize(
    const serial::ShaderProgram& prog,
    ShaderRuntimeConstantDeserializer& deserializer)
{
    *this = {};  // Clear all data

    for (const auto& mod : prog.shader_modules())
    {
        auto [it, _] = spirvCode.try_emplace(static_cast<vk::ShaderStageFlagBits>(mod.stage()));
        auto& code = it->second;

        assert(mod.spirv_code().size() % sizeof(ui32) == 0);
        code.resize(mod.spirv_code().size() / sizeof(ui32));
        memcpy(code.data(), mod.spirv_code().data(), mod.spirv_code().size());

        if (!mod.specialization_constants().empty())
        {
            auto& specs = specConstants.try_emplace(it->first).first->second;
            for (const auto& [idx, value] : mod.specialization_constants())
            {
                if (auto runtimeConst = deserializer.deserialize(value)) {
                    specs.emplace_back(idx, runtimeConst.value());
                }
                else {
                    log::warn << log::here()
                        << ": Deserialization of shader runtime value at specialization constant"
                        << " index " << idx << " failed: deserializer returned std::nullopt.";
                }
            }
        }
    }

    for (const auto& range : prog.push_constants())
    {
        pushConstants.push_back({
            .offset=range.offset(),
            .size=range.size(),
            .shaderStage=vk::ShaderStageFlagBits(range.shader_stage_flags()),
            .userId=range.user_id()
        });
    }

    for (const auto& desc : prog.descriptor_sets())
    {
        descriptorSets.push_back(PipelineLayoutTemplate::Descriptor{
            .name=DescriptorName{ desc.name() },
            .isStatic=desc.is_static()
        });
    }
}

void MaterialProgramData::serialize(std::ostream& os) const
{
    serialize().SerializeToOstream(&os);
}

void MaterialProgramData::deserialize(
    std::istream& is,
    ShaderRuntimeConstantDeserializer& deserializer)
{
    serial::ShaderProgram prog;
    prog.ParseFromIstream(&is);
    deserialize(prog, deserializer);
}



MaterialShaderProgram::MaterialShaderProgram(
    const MaterialProgramData& data,
    const PipelineDefinitionData& pipelineConfig,
    const RenderPassDefinition& renderPass)
    :
    layoutTemplate(data.makeLayout()),
    layout(PipelineRegistry::registerPipelineLayout(layoutTemplate))
{
    assert(layout != PipelineLayout::ID::NONE);
    assert(runtimePcOffsets != nullptr);

    // Create shader program
    ProgramDefinitionData program;
    for (const auto& [stage, code] : data.spirvCode) {
        program.stages.emplace(stage, ProgramDefinitionData::ShaderStage{ code });
    }

    // Load and set specialization constants
    for (const auto& [stageType, specs] : data.specConstants)
    {
        auto& stage = program.stages.at(stageType);
        for (const auto& [specIdx, specValue] : specs)
        {
            // Store the value provider (in case it wants to keep some data alive)
            runtimeValues.emplace_back(specValue);

            // Set specialization constant
            const auto data = specValue->loadData();
            assert(data.size() == specValue->getType().size());

            stage.specConstants.set(specIdx, data.data(), data.size());
        }
    }

    // Create pipeline
    pipeline = PipelineRegistry::registerPipeline(
        PipelineTemplate{ program, pipelineConfig },
        layout,
        renderPass
    );

    // Create runtime push constant offsets
    for (auto [offset, size, stages, userId] : data.pushConstants)
    {
        constexpr ui32 alloc = std::numeric_limits<ui32>::max();
        runtimePcOffsets->resize(glm::max(size_t{userId + 1}, runtimePcOffsets->size()), alloc);
        runtimePcStages->resize(glm::max(size_t{userId + 1}, runtimePcOffsets->size()));

        runtimePcOffsets->at(userId) = offset;
        runtimePcStages->at(userId) = stages;
    }
}

auto MaterialShaderProgram::getLayout() const -> const PipelineLayoutTemplate&
{
    return layoutTemplate;
}

auto MaterialShaderProgram::makeRuntime() const -> MaterialRuntime
{
    assert(pipeline != Pipeline::ID::NONE);
    assert(runtimePcOffsets != nullptr);

    return MaterialRuntime(pipeline, runtimePcOffsets, runtimePcStages);
}

} // namespace trc
