#include "AssetUtils.h"


#ifdef TRC_USE_FBX_SDK

auto trc::loadScene(const fs::path& fbxFilePath) -> SceneImportResult
{
    SceneImportResult result;

    FBXLoader loader;
    FileImportData importData = loader.loadFBXFile(fbxFilePath);

    for (const auto& mesh : importData.meshes)
    {
        // Load geometry
        std::unique_ptr<Rig> rig{ nullptr };
        if (mesh.rig.has_value()) {
            rig = std::make_unique<Rig>(std::move(mesh.rig.value()), std::move(mesh.animations));
        }
        GeometryID geoIdx = AssetRegistry::addGeometry({ mesh.mesh, std::move(rig) });

        // Load material
        MaterialID matIdx{ 0 };
        if (!mesh.materials.empty())
        {
            matIdx = AssetRegistry::addMaterial(mesh.materials.front());
        }

        result.importedGeometries.emplace_back(geoIdx, matIdx);

        // Create drawable
        Drawable& d = *result.drawables.emplace_back(new Drawable(geoIdx, matIdx, result.scene));
        d.setFromMatrix(mesh.globalTransform);

        // Apply transparency if appropriate
        if (matIdx != 0 && AssetRegistry::getMaterial(matIdx).get()->opacity < 1.0f) {
            d.enableTransparency();
        }
    }

    AssetRegistry::updateMaterialBuffer();

    return result;
}

#endif // #ifdef TRC_USE_FBX_SDK
