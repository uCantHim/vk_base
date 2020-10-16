#pragma once

#ifdef TRC_USE_FBX_SDK

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <optional>

#include <fbxsdk.h>

#include "Boilerplate.h"
#include "Geometry.h"
#include "Material.h"

namespace trc
{
    struct Mesh
    {
        std::string name;
        mat4 globalTransform;

        MeshData mesh;
        std::vector<Material> materials;
        std::optional<RigData> rig;
        std::vector<AnimationData> animations;
    };

    /**
     * @brief A holder for all data loaded from a file.
     */
    struct FileImportData
    {
        std::vector<Mesh> meshes;
    };

    class FBXLoader
    {
    public:
        FBXLoader();

        FBXLoader(const FBXLoader&) = default;
        FBXLoader(FBXLoader&&) = default;
        FBXLoader& operator=(const FBXLoader&) = default;
        FBXLoader& operator=(FBXLoader&&) noexcept = default;
        ~FBXLoader() = default;

        auto loadFBXFile(const std::string& path) -> FileImportData;

    private:
        static inline FbxManager* fbx_memory_manager{ nullptr };
        static inline FbxIOSettings* fbx_io_settings{ nullptr };
        static inline bool initialized{ false };

        /** FBX data for one mesh */
        struct MeshImport
        {
            // Contructor needed by clang because that compiler is weird as fuck
            MeshImport(FbxMesh* fbxMesh, std::string name, mat4 transform)
                : fbxMesh(fbxMesh), name(std::move(name)), transform(transform)
            {}

            FbxMesh* fbxMesh;
            std::string name;
            mat4 transform;
        };

        /** Relevant FBX data of one scene */
        struct SceneImport
        {
            std::vector<MeshImport> meshes;
            std::vector<FbxSkeleton*> skeletonRoots;
        };

        ///////////////////
        // FBX file loading
        auto loadSceneFromFile(const std::string& path) -> std::optional<SceneImport>;

        // Ususal mesh data
        static auto loadMesh(FbxMesh* mesh) -> MeshData;
        static void loadVertices(FbxMesh* mesh, MeshData& result);
        static void loadUVs(FbxMesh* mesh, MeshData& result);
        static void loadNormals(FbxMesh* mesh, MeshData& result);
        static void loadTangents(FbxMesh* mesh, MeshData& result);
        static void computeTangents(MeshData& result);

        static auto loadMaterials(FbxMesh* mesh) -> std::vector<Material>;

        // Animation data
        static constexpr ui32 MAX_WEIGHTS_PER_VERTEX = 4;
        /**
         * @brief Build a rig from a skeleton root node
         *
         * @return The created rig and an array of bone nodes. Entries in the bone node
         *         array correspond to the bone with the same index in the rig. This array
         *         is used internally to load animations.
         */
        auto loadSkeleton(FbxSkeleton* skeleton) -> std::pair<RigData, std::vector<FbxNode*>>;

        /**
         * Builds a rig and loads that rig's bone indices and weights into the mesh
         */
        auto loadRig(FbxMesh* mesh, MeshData& result) -> std::pair<RigData, std::vector<FbxNode*>>;
        auto loadAnimations(const RigData& rig, const std::vector<FbxNode*>& boneNodes)
            -> std::vector<AnimationData>;

        static void correctBoneWeights(MeshData& mesh);

        FbxScene* scene{ nullptr };
    };
} // namespace trc

#endif // #ifdef TRC_USE_FBX_SDK
