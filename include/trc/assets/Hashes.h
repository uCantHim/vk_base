#pragma once

#include <functional>

#include "trc/assets/AssetManagerBase.h"  // For asset ID types
#include "trc/assets/AssetReference.h"

template<trc::AssetBaseType T>
struct std::hash<trc::TypedAssetID<T>>
{
    auto operator()(const trc::TypedAssetID<T>& id) const -> size_t
    {
        return std::hash<trc::ui32>{}(static_cast<trc::ui32>(id.getDeviceID()));
    }
};

template<trc::AssetBaseType T>
struct std::hash<trc::AssetReference<T>>
{
    auto operator()(const trc::AssetReference<T>& ref) const -> size_t
    {
        if (ref.empty()) return 0;
        if (ref.hasResolvedID()) {
            return std::hash<trc::TypedAssetID<T>>{}(ref.getID());
        }

        assert(ref.hasAssetPath());
        return std::hash<trc::AssetPath>{}(ref.getAssetPath());
    }
};

namespace trc
{
    template<AssetBaseType T>
    inline bool operator==(const TypedAssetID<T>& a, const TypedAssetID<T>& b)
    {
        if (a.getAssetID() == b.getAssetID()) {
            assert(a.getDeviceID() == b.getDeviceID());
        }

        return a.getAssetID() == b.getAssetID();
    }

    template<AssetBaseType T>
    inline bool operator==(const AssetReference<T>& a,
                           const AssetReference<T>& b)
    {
        if (a.empty() && b.empty()) {
            return true;
        }
        if (a.hasResolvedID() && b.hasResolvedID()) {
            return a.getID() == b.getID();
        }

        assert(a.hasAssetPath() && b.hasAssetPath());
        return a.getAssetPath() == b.getAssetPath();
    }
}
