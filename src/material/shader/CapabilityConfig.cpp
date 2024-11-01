#include "trc/material/shader/CapabilityConfig.h"

#include <stdexcept>



namespace trc::shader
{

auto CapabilityConfig::getCodeBuilder() -> ShaderCodeBuilder&
{
    return *codeBuilder;
}

void CapabilityConfig::addGlobalShaderExtension(std::string extensionName)
{
    globalExtensions.emplace(std::move(extensionName));
}

void CapabilityConfig::addGlobalShaderInclude(util::Pathlet includePath)
{
    globalIncludes.emplace(std::move(includePath));
}

auto CapabilityConfig::getGlobalShaderExtensions() const
    -> const std::unordered_set<std::string>&
{
    return globalExtensions;
}

auto CapabilityConfig::getGlobalShaderIncludes() const
    -> const std::unordered_set<util::Pathlet>&
{
    return globalIncludes;
}

auto CapabilityConfig::addResource(Resource shaderResource) -> ResourceID
{
    const ResourceID id{ static_cast<ui32>(resources.size()) };
    const std::string name = "_access_resource_" + std::to_string(id);

    resources.emplace_back(new ResourceData{ std::move(shaderResource), name, {}, {}, {} });
    resourceAccessors.try_emplace(id, codeBuilder->makeExternalIdentifier(name));

    return id;
}

void CapabilityConfig::addShaderExtension(ResourceID resource, std::string extensionName)
{
    resources.at(resource)->extensions.emplace(std::move(extensionName));
}

void CapabilityConfig::addShaderInclude(ResourceID resource, util::Pathlet includePath)
{
    resources.at(resource)->includeFiles.emplace(std::move(includePath));
}

void CapabilityConfig::addMacro(
    ResourceID resource,
    std::string name,
    std::optional<std::string> value)
{
    resources.at(resource)->macroDefinitions.try_emplace(std::move(name), std::move(value));
}

auto CapabilityConfig::accessResource(ResourceID resource) const -> code::Value
{
    return resourceAccessors.at(resource);
}

auto CapabilityConfig::getResource(ResourceID resource) const -> const ResourceData&
{
    assert(resource < resources.size());
    return *resources.at(resource);
}

void CapabilityConfig::linkCapability(
    Capability capability,
    ResourceID resource)
{
    return linkCapability(capability, accessResource(resource), { resource });
}

void CapabilityConfig::linkCapability(
    Capability capability,
    code::Value value,
    std::vector<ResourceID> resources)
{
    auto [_, success] = capabilityAccessors.try_emplace(capability, value);
    if (!success)
    {
        throw std::invalid_argument(
            "[In ShaderCapabilityConfig::linkCapability]: The capability \""
            + capability.toString() + "\" is already linked to a resource!");
    }

    auto res = requiredResources.try_emplace(capability, resources.begin(), resources.end());
    assert(res.second);
}

bool CapabilityConfig::hasCapability(Capability capability) const
{
    return capabilityAccessors.contains(capability);
}

auto CapabilityConfig::accessCapability(Capability capability) const -> code::Value
{
    return capabilityAccessors.at(capability);
}

auto CapabilityConfig::getCapabilityResources(Capability capability) const
    -> std::vector<ResourceID>
{
    auto it = requiredResources.find(capability);
    if (it == requiredResources.end())
    {
        throw std::runtime_error(
            "[In ShaderCapabilityConfig::getCapabilityResources]: Shader capability \""
            + capability.toString() + "\" has not been defined.");
    }

    return { it->second.begin(), it->second.end() };
}

} // namespace trc::shader
