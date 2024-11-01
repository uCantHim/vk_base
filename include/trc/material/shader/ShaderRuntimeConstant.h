#pragma once

#include <optional>
#include <string>
#include <vector>

#include "BasicType.h"

namespace trc::shader
{
    /**
     * @brief A constant that has its value determined at shader program link
     *        time, not at module compile time.
     */
    class ShaderRuntimeConstant
    {
    public:
        explicit ShaderRuntimeConstant(BasicType valueType) : type(valueType) {}

        virtual ~ShaderRuntimeConstant() = default;

        /**
         * @brief Load the constant's runtime value
         *
         * Is called during creation of `MaterialShaderProgram` objects.
         *
         * `assert(getType().size() == loadData.size())` must be true.
         */
        virtual auto loadData() -> std::vector<std::byte> = 0;

        /**
         * @brief Serialize the constant
         *
         * Deserialization is implemented by `ShaderRuntimeConstantDeserializer`.
         */
        virtual auto serialize() const -> std::string = 0;

        /**
         * @return The value's type in shader code
         *
         * `assert(getType().size() == loadData.size())` must be true.
         */
        inline auto getType() const -> BasicType {
            return type;
        }

    private:
        const BasicType type;
    };

    /**
     * @brief Implement de-serialization operations for shader runtime constants
     *
     * When users implement a custom set of ShaderRuntimeConstants, they also
     * implement a ShaderRuntimeConstantDeserializer that deserializes all of
     * these objects.
     *
     * The shader building system never pre-defines default implementations
     * of ShaderRuntimeConstant, therefore the set of the user's implementations
     * is also the set of all ShaderRuntimeConstant implementations, and the
     * deserializer can and must handle all of them.
     *
     * # Example
     * ```cpp
     *
     * class Foo : public ShaderRuntimeConstant
     * {
     * public:
     *     auto loadData() -> std::vector<std::byte> override {
     *         return { std::byte('$') };
     *     }
     *     auto serialize() const -> std::string override { return "$"; }
     * };
     *
     * class Bar : public ShaderRuntimeConstant
     * {
     * public:
     *     auto loadData() -> std::vector<std::byte> override {
     *         return { std::byte('%') };
     *     }
     *     auto serialize() const -> std::string override { return "%"; }
     * };
     *
     * class MyDeserializer : public ShaderRuntimeConstantDeserializer
     * {
     * public:
     *     auto deserialize(const std::string& data)
     *         -> std::optional<s_ptr<ShaderRuntimeConstant>> override
     *     {
     *         if (data == "$") {
     *             return std::make_shared<Foo>();
     *         }
     *         if (data == "%") {
     *             return std::make_shared<Bar>();
     *         }
     *         return std::nullopt;
     *     }
     * };
     * ```
     */
    class ShaderRuntimeConstantDeserializer
    {
    public:
        virtual ~ShaderRuntimeConstantDeserializer() = default;

        /**
         * @return std::optional<s_ptr<ShaderRuntimeConstant>> std::nullopt if
         *         deserialization fails, otherwise a non-null pointer to a
         *         ShaderRuntimeConstant object.
         */
        virtual auto deserialize(const std::string& data)
            -> std::optional<s_ptr<ShaderRuntimeConstant>> = 0;
    };
} // namespace trc::shader
