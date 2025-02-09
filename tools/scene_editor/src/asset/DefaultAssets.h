#pragma once

#include <trc/assets/Assets.h>

void initDefaultAssets(trc::AssetManager& assetManager);

namespace g
{
    struct DefaultGeometries
    {
        trc::GeometryID cube;
        trc::GeometryID sphere;
    };

    struct DefaultMaterials
    {
        trc::MaterialID undefined;
        trc::MaterialID objectHighlight;
        trc::MaterialID objectSelect;
        trc::MaterialID objectHitbox;
    };

    auto geos() -> const DefaultGeometries&;
    auto mats() -> const DefaultMaterials&;
}
