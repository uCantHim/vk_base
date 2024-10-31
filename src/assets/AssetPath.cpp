#include "trc/assets/AssetPath.h"



namespace trc
{

AssetPath::AssetPath(fs::path path)
    : AssetPath(util::Pathlet(std::move(path)))
{
}

AssetPath::AssetPath(util::Pathlet path)
    : Pathlet(std::move(path))
{
    // Ensure that path is actually a subdirectory of the asset root
    const auto isSubdir = !filesystemPath("").lexically_relative(fs::path("."))
                           .string().starts_with("..");
    if (!isSubdir)
    {
        throw std::invalid_argument(
            "Unable to construct unique asset path from \"" + this->string() + "\": "
            "Path is not lexically relative to the logical asset root.");
    }
}

auto AssetPath::getAssetName() const -> std::string
{
    return filename().replace_extension("").string();
}

} // namespace trc
