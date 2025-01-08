#include "trc/Meshlet.h"

#include <algorithm>

#include <trc_util/Padding.h>

#include "trc/util/TriangleCacheOptimizer.h"



auto trc::makeMeshletIndices(const std::vector<ui32>& _indexBuffer, const MeshletInfo& info)
    -> MeshletGeometry
{
    constexpr ui32 PADDING = 8;

    auto indexBuffer = util::optimizeTriangleOrderingForsyth(_indexBuffer);
    assert(indexBuffer.size() % 3 == 0);

    MeshletGeometry geo;

    ui32 primitiveBegin{ 0 };
    ui32 numPrimVerts{ 0 };
    ui32 vertexBegin{ 0 };
    ui32 numVertices{ 0 };

    for (size_t i = 0; i < indexBuffer.size(); /* nothing */)
    {
        // Declare range of current meshlet's unique vertices
        auto begin = geo.uniqueVertices.begin() + vertexBegin;
        auto end = geo.uniqueVertices.end();
        auto findVertexIndex = [&](ui32 index) -> i32
        {
            auto it = std::find(begin, end, index);
            return it == end ? -1 : static_cast<int>(it - begin);
        };

        i32 verts[] = {
            findVertexIndex(indexBuffer[i + 0]),
            findVertexIndex(indexBuffer[i + 1]),
            findVertexIndex(indexBuffer[i + 2]),
        };
        const ui32 newUniques = [&]{ ui32 n = 0; for (i32 i : verts) n += i < 0; return n; }();

        const ui32 paddedNumPrimVerts = util::pad(numPrimVerts, PADDING);
        // Create meshlet if size limitations have been reached
        if (paddedNumPrimVerts > info.maxPrimitives * 3    // This has been exceeded in the last loop
            || numVertices + newUniques > info.maxVertices)  // This would be exceeded this loop
        {
            assert(numPrimVerts % 3 == 0);

            // Create meshlet
            for (ui32 i = 0; i < paddedNumPrimVerts - numPrimVerts; i++)
                geo.primitiveIndices.emplace_back(0);

            geo.meshlets.emplace_back(MeshletDescription{
                .vertexBegin = vertexBegin,
                .primitiveBegin = primitiveBegin,
                .numVertices = numVertices,
                .numPrimVerts = numPrimVerts,
            });

            vertexBegin += numVertices;
            primitiveBegin += paddedNumPrimVerts;
            numVertices = 0;
            numPrimVerts = 0;
            continue;
        }

        // Add new unique indices
        for (size_t j = 0; j < 3; j++)
        {
            if (verts[j] < 0)
            {
                geo.uniqueVertices.emplace_back(indexBuffer[i + j]);
                ++numVertices;
                verts[j] = (i32(geo.uniqueVertices.size()) - 1) - i32(vertexBegin);
            }
            assert(verts[j] >= 0 && verts[j] <= UINT8_MAX);
            assert(static_cast<ui32>(verts[j]) < numVertices);
            assert(static_cast<ui32>(verts[j]) < info.maxVertices);
            geo.primitiveIndices.emplace_back(static_cast<ui8>(verts[j]));
        }

        // Each loop adds one primitive
        numPrimVerts += 3;
        i += 3;  // Only go to next primitive if it has been added to a meshlet
    }

    if (numVertices > 0 && numPrimVerts > 0)
    {
        geo.meshlets.emplace_back(MeshletDescription{
            .vertexBegin=vertexBegin,
            .primitiveBegin=primitiveBegin,
            .numVertices=numVertices,
            .numPrimVerts=numPrimVerts
        });
    }

    return geo;
}
