#pragma once

#include <string>
#include <vector>

#include <componentlib/Table.h>
#include <trc/Types.h>
#include <trc/material/BasicType.h>
#include <trc/material/ShaderModuleBuilder.h>
#include <trc_util/data/IdPool.h>
#include <trc_util/data/TypesafeId.h>

using namespace trc::basic_types;

struct Socket;
struct Node;

using SocketID = trc::data::TypesafeID<Socket>;
using NodeID = trc::data::TypesafeID<Node>;
using componentlib::Table;

struct Node
{
    s_ptr<trc::ShaderFunction> computation;
};

struct Socket
{
    trc::BasicType type;
    std::string name;
};

/**
 * @brief Topological information about a material graph
 */
struct MaterialGraph
{
    NodeID outputNode;

    Table<Node, NodeID> nodeInfo;

    Table<std::vector<SocketID>, NodeID> inputSockets;
    Table<std::vector<SocketID>, NodeID> outputSockets;

    /**
     * Every valid socket ID has an associated entry in the `socketInfo` table.
     */
    Table<Socket, SocketID> socketInfo;

    /**
     * Links between sockets are defined as entries in the `link` table. If a
     * socket has no entry here, it is not linked to another socket.
     */
    Table<SocketID, SocketID> link;

    auto makeNode() -> NodeID;
    auto makeSocket(Socket newSock) -> SocketID;

    /**
     * @brief Remove a node and all associated objects from the graph
     *
     * Removes a node from the graph. Removes all of the node's sockets. Removes
     * all links from/to any of the node's sockets.
     */
    void removeNode(NodeID id);

private:
    trc::data::IdPool<uint32_t> nodeId;
    trc::data::IdPool<uint32_t> socketId;
};
