#pragma once

#include <array>
#include <optional>

#include "trc/material/shader/Capability.h"
#include "trc/material/shader/ShaderModuleBuilder.h"
#include "trc/material/shader/ShaderModuleCompiler.h"
#include "trc/material/TorchMaterialSettings.h"

namespace trc
{
    namespace code = shader::code;

    /**
     * A collection of capabilities intended to be used by shader code
     * implementing material calculations: 'user code' if you will.
     */
    namespace MaterialCapability
    {
        constexpr shader::Capability kVertexWorldPos{ "trc_mat_vertexWorldPos" };
        constexpr shader::Capability kVertexNormal{ "trc_mat_vertexNormal" };
        constexpr shader::Capability kVertexUV{ "trc_mat_vertexUV" };
        constexpr shader::Capability kTangentToWorldSpaceMatrix{ "trc_mat_tangentToWorld" };

        constexpr shader::Capability kCameraWorldPos{ "trc_mat_cameraWorldPos" };

        constexpr shader::Capability kTime{ "trc_mat_currentTime" };
        constexpr shader::Capability kTimeDelta{ "trc_mat_frameTime" };

        /**
         * Gives access to an array of texture samplers. The array shall be
         * indexed via the index obtained from `TextureHandle::getDeviceIndex`.
         *
         * Use the `TextureSample` shader function as a default implementation.
         */
        constexpr shader::Capability kTextureSample{ "trc_mat_textureSample" };
    } // namespace MaterialCapability

    /**
     * Capabilities for internal use in fragment shaders generated from material
     * descriptions.
     */
    namespace FragmentCapability
    {
        constexpr shader::Capability kNextFragmentListIndex{ "frag_allocFragListIndex" };
        constexpr shader::Capability kMaxFragmentListIndex{ "frag_maxFragListIndex" };
        constexpr shader::Capability kFragmentListHeadPointerImage{ "frag_fragListPointerImage" };
        constexpr shader::Capability kFragmentListBuffer{ "frag_fragListBuffer" };
        constexpr shader::Capability kShadowMatrices{ "frag_shadowMatrixBuffer" };
        constexpr shader::Capability kLightBuffer{ "frag_lightDataBuffer" };
    } // namespace FragmentCapability

    /**
     * Capabilities for internal use in callable shaders generated from material
     * descriptions.
     */
    namespace RayHitCapability
    {
        constexpr shader::Capability kBarycentricCoords{ "rcall_baryCoords" };
        constexpr shader::Capability kGeometryIndex{ "rcall_geoIndex" };

        constexpr shader::Capability kOutColor{ "rcall_colorOutput" };
    } // namespace RayHitCapability

    /**
     * @brief Torch's implementation of a configurable fragment shader
     *
     * The fragment module has a set of output parameters that are passed to the
     * lighting algorithm. The fragment shader calculates values for these
     * parameters.
     *
     * Set shader code expressions as parameters with the function
     * `FragmentModule::setParameter`.
     *
     * # Example
     *
     * ```cpp
     * ShaderModuleBuilder builder{ myConfig };
     * FragmentModule frag;
     * frag.setParameter(
     *     FragmentModule::Parameter::eColor,
     *     builder.makeConstant(vec4{ 1, 0.5, 0, 1.0f })
     * );
     *
     * auto shaderModule = frag.build(std::move(builder), false);
     * ```
     */
    class FragmentModule
    {
    public:
        static constexpr size_t kNumParams{ 6 };

        enum class Parameter : size_t
        {
            eColor,
            eNormal,
            eSpecularFactor,
            eRoughness,
            eMetallicness,
            eEmissive,
        };

        FragmentModule() = default;

        /**
         * @brief Set a value for one of the module's output parameters
         *
         * @param Parameter param   The parameter for which to set a value.
         * @param code::Value value A shader code expression that calculates a
         *                          value for `param`.
         */
        void setParameter(Parameter param, code::Value value);

        /**
         * @brief Compile the module description to a fragment shader module
         *
         * @param ShaderModuleBuilder moduleCode The code builder with which
         *        the shader code used in `FragmentModule::setParameter` was
         *        generated.
         * @param bool transparent An additional setting for the fragment
         *        shader. Set to `true` if the shader is used for transparent
         *        objects.
         * @param ShaderCapabilityConfig config The capability configuration to
         *        be used to generate the shader module. If a custom one is
         *        provided (e.g. to add custom functionality/inputs to the
         *        fragment shader), it should be a superset (i.e. a modified
         *        version) of the default config, which can be obtained from
         *        `trc::makeFragmentCapabilityConfig`.
         *
         * @throw std::invalid_argument if a required parameter has not been set
         *                              beforehand.
         */
        auto build(shader::ShaderModuleBuilder moduleCode,
                   bool transparent,
                   const shader::CapabilityConfig& config = makeFragmentCapabilityConfig())
            -> shader::ShaderModule;

        /**
         * @brief Compile the module description to a closest-hit shader module
         *
         * @param ShaderModuleBuilder moduleCode The code builder with which
         *        the shader code used in `FragmentModule::setParameter` was
         *        generated.
         */
        auto buildClosesthitShader(shader::ShaderModuleBuilder builder) -> shader::ShaderModule;

    private:
        /** @throw std::invalid_argument */
        auto getParamValue(Parameter param) -> code::Value;

        void fillDefaultValues(shader::ShaderModuleBuilder& builder);

        std::array<std::optional<code::Value>, kNumParams> parameters;
    };
} // namespace trc
