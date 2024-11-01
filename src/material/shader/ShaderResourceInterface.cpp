#include "trc/material/shader/ShaderResourceInterface.h"

#include <format>
#include <sstream>

#include <trc_util/Util.h>



namespace trc::shader
{

auto ShaderResourceInterface::getGlslCode() const -> const std::string&
{
    return code;
}

auto ShaderResourceInterface::getRequiredShaderInputs() const
    -> const std::vector<ShaderInputInfo>&
{
    return requiredShaderInputs;
}

auto ShaderResourceInterface::getSpecializationConstants() const
    -> const std::vector<SpecializationConstantInfo>&
{
    return specConstants;
}

auto ShaderResourceInterface::getRequiredDescriptorSets() const -> std::vector<std::string>
{
    std::vector<std::string> result;
    result.reserve(descriptorSetIndexPlaceholders.size());
    for (const auto& [name, _] : descriptorSetIndexPlaceholders) {
        result.emplace_back(name);
    }

    return result;
}

auto ShaderResourceInterface::getDescriptorIndexPlaceholder(const std::string& setName) const
    -> std::optional<std::string>
{
    auto it = descriptorSetIndexPlaceholders.find(setName);
    if (it != descriptorSetIndexPlaceholders.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ShaderResourceInterface::getPushConstantSize() const -> ui32
{
    return pushConstantSize;
}

auto ShaderResourceInterface::getPushConstants() const -> std::vector<PushConstantInfo>
{
    std::vector<PushConstantInfo> result;
    result.reserve(pushConstantInfos.size());
    for (const auto& [_, pc] : pushConstantInfos) {
        result.emplace_back(pc);
    }

    return result;
}

auto ShaderResourceInterface::getPushConstantOffsetPlaceholder(ui32 pushConstantId) const
    -> std::optional<std::string>
{
    if (pushConstantInfos.contains(pushConstantId)) {
        return pushConstantInfos.at(pushConstantId).offsetPlaceholder;
    }
    return std::nullopt;
}

auto ShaderResourceInterface::getRequiredPayloads() const -> const std::vector<PayloadInfo>&
{
    return requiredPayloads;
}



auto ShaderResourceInterfaceBuilder::DescriptorBindingFactory::make(
    const CapabilityConfig::DescriptorBinding& binding) -> std::string
{
    const auto placeholder = makeDescriptorSetPlaceholder(binding.setName);
    auto [_, success] = descriptorSetPlaceholders.try_emplace(binding.setName, placeholder);
    if (success) {
        generatedCode += "#define _" + placeholder + " $" + placeholder + "\n";
    }

    const auto descriptorName = binding.descriptorName.empty()
            ? "_descriptorName_" + std::to_string(nextNameIndex++)
            : binding.descriptorName;

    auto& ss = generatedCode;
    ss += "layout (set = _" + placeholder + ", binding = " + std::to_string(binding.bindingIndex);
    if (binding.layoutQualifier) {
        ss += ", " + *binding.layoutQualifier;
    }
    ss += ") " + binding.descriptorType + " " + descriptorName;
    if (binding.descriptorContent) {
        ss += "_Name\n{\n" + *binding.descriptorContent + "\n} " + descriptorName;
    }
    if (binding.arrayCount)
    {
        ss += "[";
        if (*binding.arrayCount > 0) ss += std::to_string(*binding.arrayCount);
        ss += "]";
    }
    ss += ";\n";

    return descriptorName;
}

auto ShaderResourceInterfaceBuilder::DescriptorBindingFactory::getCode() const -> const std::string&
{
    return generatedCode;
}

auto ShaderResourceInterfaceBuilder::DescriptorBindingFactory::getDescriptorSets() const
    -> std::unordered_map<std::string, std::string>
{
    return descriptorSetPlaceholders;
}

auto ShaderResourceInterfaceBuilder::DescriptorBindingFactory::makeDescriptorSetPlaceholder(
    const std::string& set)
    -> std::string
{
    return set + "_DESCRIPTOR_SET_INDEX";
}



auto ShaderResourceInterfaceBuilder::PushConstantFactory::make(
    ResourceID /*resource*/,
    const CapabilityConfig::PushConstant& pc) -> std::string
{
    const auto byteSize = code::types::getTypeSize(pc.type);
    assert(byteSize > 0);

    const std::string name = "_push_constant_" + std::to_string(pc.userId);
    const std::string offsetPlaceholder = name + "_offset";

    infos.try_emplace(
        pc.userId,
        PushConstantInfo{
            .offset=totalSize,
            .size=byteSize,
            .userId=pc.userId,
            .offsetPlaceholder=offsetPlaceholder
        }
    );
    totalSize += byteSize;

    code += std::format(
        "layout (offset=${}) {} {};\n",
        offsetPlaceholder,
        code::types::to_string(pc.type),
        name
    );

    return std::format("{}.{}", kPcBlockNamespaceName, name);
}

auto ShaderResourceInterfaceBuilder::PushConstantFactory::getCode() const -> std::string
{
    if (!code.empty())
    {
        return std::format(
            "layout (push_constant) uniform {} \n{{\n{}\n}} {};",
            kPcBlockName,
            code,
            kPcBlockNamespaceName
        );
    }
    return "";
}

auto ShaderResourceInterfaceBuilder::PushConstantFactory::getTotalSize() const -> ui32
{
    return totalSize;
}

auto ShaderResourceInterfaceBuilder::PushConstantFactory::getInfos() const
    -> const std::unordered_map<ResourceID, PushConstantInfo>&
{
    return infos;
}



auto ShaderResourceInterfaceBuilder::ShaderInputFactory::make(
    Capability capability,
    const CapabilityConfig::ShaderInput& in) -> std::string
{
    // const ui32 shaderInputLocation = nextShaderInputLocation;
    // nextShaderInputLocation += in.type.locations();
    const ui32 shaderInputLocation = in.location;
    const std::string name = "shaderStageInput_" + std::to_string(shaderInputLocation);

    // Make member code
    std::stringstream ss;
    ss << (in.flat ? "flat " : "") << in.type.to_string() << " " << name;
    shaderInputs.push_back({ shaderInputLocation, in.type, name, ss.str(), capability });

    return name;
}



auto ShaderResourceInterfaceBuilder::RayPayloadFactory::make(
    Capability requestingCapability,
    const CapabilityConfig::RayPayload& pl)
    -> std::string
{
    std::string payloadIdentifier = "_ray_payload_" + std::to_string(nextNameIndex++);
    std::string locationPlaceholder = payloadIdentifier + "_LOCATION_PLACEHOLDER";

    payloads.push_back({
        .type=pl.type,
        .capability=requestingCapability,
        .locationPlaceholder=locationPlaceholder
    });

    code +=
        "layout (location = $" + locationPlaceholder + ") " +
        (pl.callableData ? "callableData" : "rayPayload") + (pl.incoming ? "In" : "") + "EXT"
        + " " + code::types::to_string(pl.type)
        + payloadIdentifier
        + ";\n";

    return payloadIdentifier;
}

auto ShaderResourceInterfaceBuilder::RayPayloadFactory::getCode() const -> const std::string&
{
    return code;
}

auto ShaderResourceInterfaceBuilder::RayPayloadFactory::getPayloads() const
    -> const std::vector<PayloadInfo>&
{
    return payloads;
}



auto ShaderResourceInterfaceBuilder::HitAttributeFactory::make(
    const CapabilityConfig::HitAttribute& att)
    -> std::string
{
    const std::string identifier = " _hit_attribute_" + std::to_string(nextNameIndex++);
    code += "hitAttributeEXT " + code::types::to_string(att.type) + identifier;

    return identifier;
}

auto ShaderResourceInterfaceBuilder::HitAttributeFactory::getCode() const -> const std::string&
{
    return code;
}



ShaderResourceInterfaceBuilder::ShaderResourceInterfaceBuilder(
    const CapabilityConfig& config,
    ShaderCodeBuilder& codeBuilder)
    :
    config(config),
    codeBuilder(&codeBuilder),
    requiredExtensions(config.getGlobalShaderExtensions()),
    requiredIncludePaths(config.getGlobalShaderIncludes())
{
}

auto ShaderResourceInterfaceBuilder::compile() const -> ShaderResourceInterface
{
    std::stringstream ss;

    for (const auto& [_, macro] : resourceMacros) {
        ss << "#define " << macro.first << " " << macro.second << "\n";
    }
    for (const auto& [name, val] : requiredMacros) {
        ss << "#define " << name << " (" << val.value_or("") << ")\n";
    }
    for (const auto& ext : requiredExtensions) {
        ss << "#extension " << ext << " : require\n";
    }
    for (const auto& path : requiredIncludePaths) {
        ss << "#include \"" << path.string() << "\"\n";
    }
    ss << "\n";

    // Write specialization constants
    for (const auto& [index, name] : specializationConstants) {
        ss << "layout (constant_id = " << index << ") const uint " << name << " = 0;\n";
    }
    ss << "\n";

    // Write descriptor sets
    ss << descriptorFactory.getCode() << "\n";

    // Write push constants
    ss << pushConstantFactory.getCode() << "\n";

    // Write shader inputs
    for (const auto& out : shaderInput.shaderInputs) {
        ss << "layout (location = " << out.location << ") in " << out.declCode << ";\n";
    }
    ss << "\n";

    // Write ray payloads
    ss << rayPayloadFactory.getCode() << "\n";

    // Write hit attributes
    ss << hitAttributeFactory.getCode() << "\n";

    // Create result value
    ShaderResourceInterface result;
    result.code = ss.str();
    result.requiredShaderInputs = shaderInput.shaderInputs;
    result.requiredPayloads = rayPayloadFactory.getPayloads();
    for (const auto& [specIdx, value] : specializationConstantValues)
    {
        result.specConstants.push_back(ShaderResourceInterface::SpecializationConstantInfo{
            .value=value,
            .specializationConstantIndex=specIdx
        });
    }
    result.descriptorSetIndexPlaceholders = descriptorFactory.getDescriptorSets();
    result.pushConstantSize = pushConstantFactory.getTotalSize();
    result.pushConstantInfos = pushConstantFactory.getInfos();

    return result;
}

auto ShaderResourceInterfaceBuilder::queryCapability(Capability capability) -> code::Value
{
    for (auto id : config.getCapabilityResources(capability)) {
        requireResource(capability, id);
    }

    return config.accessCapability(capability);
}

auto ShaderResourceInterfaceBuilder::makeSpecConstant(s_ptr<ShaderRuntimeConstant> value) -> code::Value
{
    auto [it, success] = specializationConstantValues.try_emplace(nextSpecConstantIndex++, value);
    assert(success);

    const auto& [idx, _] = *it;
    const std::string specConstName = "kSpecConstant" + std::to_string(idx) + "_RuntimeValue";
    specializationConstants.emplace_back(idx, specConstName);

    // Create the shader code value
    auto id = codeBuilder->makeExternalIdentifier(specConstName);
    codeBuilder->annotateType(id, value->getType());

    return id;
}

void ShaderResourceInterfaceBuilder::requireResource(
    Capability capability,
    CapabilityConfig::ResourceID resourceId)
{
    const auto& res = config.getResource(resourceId);

    // Only add a resource once
    if (resourceMacros.contains(&res)) {
        return;
    }

    // First resource access; create it.
    requiredExtensions.insert(res.extensions.begin(), res.extensions.end());
    requiredIncludePaths.insert(res.includeFiles.begin(), res.includeFiles.end());
    for (const auto& [name, val] : res.macroDefinitions) {
        requiredMacros.emplace_back(name, val);
    }

    auto accessorStr = std::visit(util::VariantVisitor{
        [this](const CapabilityConfig::DescriptorBinding& binding) {
            return descriptorFactory.make(binding);
        },
        [this, capability](const CapabilityConfig::ShaderInput& v) {
            return shaderInput.make(capability, v);
        },
        [this, resourceId](const CapabilityConfig::PushConstant& pc) {
            return pushConstantFactory.make(resourceId, pc);
        },
        [this, capability](const CapabilityConfig::RayPayload& pl) {
            return rayPayloadFactory.make(capability, pl);
        },
        [this](const CapabilityConfig::HitAttribute& att) {
            return hitAttributeFactory.make(att);
        }
    }, res.resourceType);

    resourceMacros.try_emplace(&res, res.resourceMacroName, accessorStr);
}

} // namespace trc::shader
