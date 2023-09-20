#include "GraphTopology.h"



auto GraphTopology::makeNode(Node node) -> NodeID
{
    NodeID id{ nodeId.generate() };
    nodeInfo.emplace(id, node);
    inputSockets.emplace(id);
    outputSockets.emplace(id);

    return id;
}

auto GraphTopology::makeSocket(Socket newSock) -> SocketID
{
    assert(newSock.parentNode != NodeID::NONE && "A socket must have a parent node!");

    SocketID id{ socketId.generate() };
    socketInfo.emplace(id, newSock);

    return id;
}

void GraphTopology::linkSockets(SocketID a, SocketID b)
{
    link.emplace(a, b);
    link.emplace(b, a);
}

void GraphTopology::unlinkSockets(SocketID a)
{
    link.erase(link.get(a));
    link.erase(a);
}

void GraphTopology::removeNode(NodeID id)
{
    if (id == outputNode) {
        throw std::invalid_argument("[In MaterialGraph::removeNode]: Unable to remove the"
                                    " output node!");
    }

    /** Remove a socket and its link (if it has any) from the graph */
    auto removeSocket = [this](SocketID sock) {
        if (link.contains(sock))
        {
            const auto linkedSock = link.get(sock);
            assert(link.contains(linkedSock));
            link.erase(sock);
            link.erase(linkedSock);
        }
        socketInfo.erase(sock);
    };

    for (auto sock : inputSockets.get(id)) {
        removeSocket(sock);
    }
    for (auto sock : outputSockets.get(id)) {
        removeSocket(sock);
    }

    inputSockets.erase(id);
    outputSockets.erase(id);
    nodeInfo.erase(id);
}



void createSockets(NodeID node, GraphTopology& graph, const NodeDescription& desc)
{
    for (const auto& arg : desc.computation.arguments)
    {
        auto sock = graph.makeSocket({ .parentNode=node, .desc=arg });
        graph.inputSockets.get(node).emplace_back(sock);
    }
    if (desc.computation.hasOutputValue())
    {
        auto sock = graph.makeSocket({ .parentNode=node, .desc{ "Output", "" } });
        graph.outputSockets.get(node).emplace_back(sock);
    }
}
