#include "AssetManager.h"

#include "assets/AssetDataProxy.h"



namespace trc
{

inline auto generateUniqueName(std::string name)
{
    return "__trc_fileless_asset__" + name;
}



template<AssetBaseType T>
inline auto AssetManager::createAsset(const AssetData<T>& data) -> TypedAssetID<T>
{
    try {
        auto res = _createAsset(AssetDataProxy{ data });
        return std::any_cast<TypedAssetID<T>>(res);
    }
    catch(const std::bad_any_cast&)
    {
        throw std::logic_error(
            "[In AssetManager::createAsset<>]: Internal engine logic error: Imported asset data"
            " type did not result in the correct TypedAssetID specialization returned as a"
            " std::any from createAsset(AssetDataProxy)! This result should not be possible."
        );
    }
}

template<AssetBaseType T>
inline void AssetManager::destroyAsset(const TypedAssetID<T> id)
{
    assetIdPool.free(static_cast<AssetID::IndexType>(id.uniqueId));
    assetMetaData.erase(id.uniqueId);

    id.reg->remove(id.id);
    // registry.remove(id.id);
}

} // namespace trc