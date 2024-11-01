#pragma once

#include <optional>
#include <string>
#include <vector>

#include "trc/assets/AssetReference.h"
#include "trc/assets/TextureRegistry.h"
#include "trc/material/shader/ShaderProgram.h"
#include "trc/material/shader/CapabilityConfig.h"
#include "trc/material/shader/ShaderRuntimeConstant.h"

namespace trc
{
    /**
     * @brief Create an object that defines all inputs that Torch provides to
     *        shaders
     *
     * @return ShaderCapabilityConfig A configuration for fragment shaders in
     *                                Torch's material system for standard
     *                                drawable objects.
     */
    auto makeFragmentCapabilityConfig() -> shader::CapabilityConfig;

    /**
     * @brief Create a configuration that implements all capabilities that Torch
     *        provides to shaders in the `MaterialCapability` namespace.
     *
     * Also implements all capabilities in `RayHitCapability` for internal
     * use.
     *
     * @return ShaderCapabilityConfig A configuration for callable shaders in
     *                                Torch's material system for standard
     *                                drawable objects.
     */
    auto makeRayHitCapabilityConfig() -> shader::CapabilityConfig;

    /**
     * @brief Define Torch's standard program link settings.
     */
    auto makeProgramLinkerSettings() -> shader::ShaderProgramLinkSettings;

    /**
     * @brief A specialization constant that specifies a texture's device index
     */
    class RuntimeTextureIndex : public shader::ShaderRuntimeConstant
    {
    public:
        explicit RuntimeTextureIndex(AssetReference<Texture> texture);

        auto loadData() -> std::vector<std::byte> override;
        auto serialize() const -> std::string override;

        auto getTextureReference() -> AssetReference<Texture>;

        /**
         * @return Is `nullptr` if deserialization fails.
         */
        static auto deserialize(const std::string& data) -> s_ptr<RuntimeTextureIndex>;

    private:
        AssetReference<Texture> texture;
        std::optional<AssetHandle<Texture>> runtimeHandle;
    };
} // namespace trc
