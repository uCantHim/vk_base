#pragma once

#include <string>
#include <vector>

#include "CodePrimitives.h"

namespace trc::shader
{
    /**
     * @brief Defines a shader's logic for writing output values
     *
     * # Example
     * ```cpp
     *
     * auto fragColor = builder.makeOutputLocation(0, vec4{});
     * output.makeStore(fragColor, myColor);
     * ```
     *
     * Results in:
     * ```glsl
     *
     * layout (location=0) out vec4 fragColor;
     * void main() { fragColor = <myColor>; }
     * ```
     */
    class ShaderOutputInterface
    {
    public:
        /**
         * @brief Create a store.
         *
         * Creates an assignment to an lvalue storage destination. The store
         * is treated as a side effect and therefore a point of control flow
         * termination.
         */
        void makeStore(code::Value dst, code::Value value);

        /**
         * @brief Create a call to a function with side-effects.
         */
        void makeFunctionCall(code::Function func, std::vector<code::Value> args);

        /**
         * @brief Create a call to a built-in (externally defined) function.
         */
        void makeBuiltinCall(std::string funcName, std::vector<code::Value> args);

        /**
         * @brief Create the required output statements in the current block
         */
        void buildStatements(ShaderCodeBuilder& builder) const;

    private:
        std::vector<std::pair<code::Value, code::Value>> stores;
        std::vector<std::pair<code::Function, std::vector<code::Value>>> functionCalls;
        std::vector<std::pair<std::string, std::vector<code::Value>>> externalCalls;
    };
} // namespace trc::shader
