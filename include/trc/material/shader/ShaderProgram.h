#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <spirv/CompileSpirv.h>

#include "ShaderRuntime.h"
#include "ShaderModuleCompiler.h"
#include "ShaderRuntimeConstant.h"
#include "material_shader_program.pb.h"

namespace trc::shader
{
    auto makeDefaultShaderCompileOptions() -> u_ptr<shaderc::CompileOptions>;

    /**
     * @brief Configuration options passed to `linkShaderProgram`.
     */
    struct ShaderProgramLinkSettings
    {
        /**
         * Compile options for shader code. Shall not be `nullptr`.
         *
         * Note that this also defines a file includer strategy. The default
         * value does not search any include paths.
         *
         * @throw std::invalid_argument if `nullptr` is given.
         *
         * NOTE to self: This has to be a unique ptr because moving
         * `shaderc::CompileOptions` seems to be causing crashes related to the
         * includer.
         */
        u_ptr<shaderc::CompileOptions> compileOptions{ makeDefaultShaderCompileOptions() };

        /**
         * Maps numbers to descriptor set names. Lower numbers are preferred
         * to have a lower descriptor set index in the final program.
         *
         * Note that it is not guaranteed that a descriptor set will reside at
         * the exact index specified here; descriptor set indices must be
         * occupied contiguously, starting at 0, in the final program. The
         * numbers specified here define a priority order.
         *
         * Example: An order specified like this:
         *
         *     { {"foo", 2}, {"bar", 5}, {"baz", 4} }
         *
         * will likely result in the following declarations:
         *
         *     layout (set=0, ...) foo;
         *     layout (set=1, ...) baz;
         *     layout (set=2, ...) bar;
         *
         * If no indices are specified this way, descriptor set order will be
         * implementation-defined.
         */
        std::unordered_map<std::string, ui32> preferredDescriptorSetIndices;
    };

    /**
     * @brief A serializable representation of a full shader program
     *
     * Create `MaterialProgramData` structs with `linkMaterialProgram`. This
     * function performs quite complicated logic to collect and merge
     * information from a set of shader modules and one should never edit
     * `MaterialProgramData`'s fields manually.
     *
     * # Example
     * ```cpp
     *
     * ShaderModule myVertexModule = ...;
     * ShaderModule myFragmentModule = ...;
     *
     * MaterialProgramData myProgram = linkMaterialProgram(
     *     {
     *         { vk::ShaderStageFlagBits::eVertex, std::move(myVertexModule) },
     *         { vk::ShaderStageFlagBits::eFragment, std::move(myVertexModule) },
     *     },
     *     myDescriptorConfig
     * );
     * ```
     */
    struct ShaderProgramData
    {
        struct DescriptorSet
        {
            std::string name;

            /** The descriptor set index in the shader program */
            ui32 index;
        };

        struct PushConstantRange
        {
            ui32 offset;
            ui32 size;
            vk::ShaderStageFlagBits shaderStage;

            /** ID that identifies the push constant as a semantic value */
            ui32 userId;
        };

        /**
         * The SPIR-V code for each shader stage.
         */
        std::unordered_map<vk::ShaderStageFlagBits, std::vector<ui32>> spirvCode;

        /**
         * For each shader stage, a list of runtime constants.
         */
        std::unordered_map<
            vk::ShaderStageFlagBits,
            std::vector<std::pair<ui32, s_ptr<ShaderRuntimeConstant>>>
        > specConstants;

        /**
         * A list of push constant ranges.
         *
         * These are *not* the physical byte ranges accessed in shaders. These
         * represent the semantical, user-accessible push constant values that
         * were specified as resources or capabilities and can be referenced
         * with user-supplied IDs. These are therefore subranges of the final
         * combined push constant ranges that are specified for shader modules.
         */
        std::vector<PushConstantRange> pushConstants;

        /**
         * A push constant range for each shader stage.
         *
         * These are the combined, physical byte ranges which are accessed in
         * shaders as push constants.
         */
        std::unordered_map<vk::ShaderStageFlagBits, vk::PushConstantRange> pcRangesPerStage;

        /**
         * A list of descriptor sets that are used by the program.
         *
         * Each descriptor set has an `index` member associated with it; this is
         * the respective descriptor set index in the program's shader code.
         */
        std::vector<DescriptorSet> descriptorSets;

        auto serialize() const -> serial::ShaderProgram;
        void deserialize(const serial::ShaderProgram& program,
                         ShaderRuntimeConstantDeserializer& deserializer);

        void serialize(std::ostream& os) const;
        void deserialize(std::istream& is,
                         ShaderRuntimeConstantDeserializer& deserializer);
    };

    /**
     * @brief Create a shader program from a set of shader modules
     */
    auto linkShaderProgram(std::unordered_map<vk::ShaderStageFlagBits, ShaderModule> modules,
                           const ShaderProgramLinkSettings& config = {})
        -> ShaderProgramData;
} // namespace trc::shader
