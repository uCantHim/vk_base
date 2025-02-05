#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include <trc/material/shader/CodePrimitives.h>
#include <trc/material/shader/ShaderModuleBuilder.h>

#include "GraphTopology.h"

namespace code = trc::code;

/**
 * @brief Try to infer a socket's type
 *
 * Applies inferred types of linked sockets and the socket's own type
 * constraints to try to infer a type for the socket.
 *
 * @return The widest type constaint possible from the inputs, or nothing if
 *         the type constraints considered are irreconcilable; this is a type
 *         error.
 */
auto inferType(const GraphTopology& graph, SocketID sock) -> std::optional<TypeConstraint>;

/**
 * @brief Query or create a computation builder for an output value
 */
auto getComputationBuilder(const NodeOutput& value)
    -> ComputedValue::ComputationBuilder;

struct GraphOutput
{
    std::unordered_map<std::string, code::Value> values;
};

struct GraphValidationError : public trc::Exception
{
    GraphValidationError(std::string msg)
        : Exception("Error during graph validation: " + std::move(msg))
    {}
};

/**
 * @throw GraphValidationError
 */
auto compileMaterialGraph(trc::ShaderModuleBuilder& builder, const GraphTopology& graph)
    -> std::optional<GraphOutput>;
