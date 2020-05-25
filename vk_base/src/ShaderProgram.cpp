#include "ShaderProgram.h"

#include <fstream>
#include <sstream>



vkb::ShaderProgram::ShaderProgram(
    const std::string& vertPath,
    const std::string& fragPath,
    const std::string& geomPath,
    const std::string& tescPath,
    const std::string& tesePath)
{
    auto vert = createShaderModule(readFile(vertPath));
    auto frag = createShaderModule(readFile(fragPath));

    stages.emplace_back(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eVertex,
        *vert,
        "main"
    );
    stages.emplace_back(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eFragment,
        *frag,
        "main"
    );
    modules.push_back(std::move(vert));
    modules.push_back(std::move(frag));

    if (!geomPath.empty()) {
        auto geom = createShaderModule(readFile(geomPath));

        stages.emplace_back(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eGeometry,
            *geom,
            "main"
        );
        modules.push_back(std::move(geom));
    }
    if (!tescPath.empty() && !tesePath.empty()) {
        auto tesc = createShaderModule(readFile(tescPath));
        auto tese = createShaderModule(readFile(tesePath));

        stages.emplace_back(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eTessellationControl,
            *tesc,
            "main"
        );
        stages.emplace_back(
            vk::PipelineShaderStageCreateFlags(),
            vk::ShaderStageFlagBits::eTessellationEvaluation,
            *tese,
            "main"
        );
        modules.push_back(std::move(tesc));
        modules.push_back(std::move(tese));
    }
}


auto vkb::ShaderProgram::create(
    const std::string& vertPath,
    const std::string& fragPath,
    const std::string& geomPath,
    const std::string& tescPath,
    const std::string& tesePath) -> ShaderProgram
{
    return ShaderProgram(vertPath, fragPath, geomPath, tescPath, tesePath);
}


auto vkb::ShaderProgram::getStages() const noexcept -> const ShaderStages&
{
    return stages;
}


auto vkb::readFile(const std::string& path) -> std::vector<char>
{
    assert(!path.empty());

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file " + path);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> result(fileSize);
    file.seekg(0);
    file.read(result.data(), fileSize);
    file.close();

    return result;
}


auto vkb::createShaderModule(std::vector<char> code) -> vk::UniqueShaderModule
{
    assert(!code.empty());

    vk::ShaderModuleCreateInfo info(
        vk::ShaderModuleCreateFlags(),
        code.size(),
        reinterpret_cast<uint32_t*>(code.data())
    );

    return VulkanBase::getDevice().get().createShaderModuleUnique(info);
}
