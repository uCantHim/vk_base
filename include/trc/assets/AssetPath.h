#pragma once

#include <cassert>
#include <string>
#include <filesystem>

#include <trc_util/Exception.h>

namespace trc
{
    namespace fs = std::filesystem;

    /**
     * @brief A path to an asset file
     *
     * A logical path to an internal asset file, relative to the asset
     * storage directory. Can be converted to a filesystem path.
     */
    class AssetPath
    {
    public:
        AssetPath(const AssetPath&) = default;
        AssetPath(AssetPath&&) noexcept = default;
        AssetPath& operator=(const AssetPath&) = default;
        AssetPath& operator=(AssetPath&&) noexcept = default;
        ~AssetPath() = default;

        /**
         * @param fs::path path Can be one of the following:
         *  - A relative path: The path will be interpreted as relative
         *    to the asset directory.
         *  - An absolute path: Absolute paths MUST contain the asset
         *    directory as a prefix.
         *
         * @throw std::invalid_argument if no valid AssetPath can be
         *        constructed.
         */
        explicit AssetPath(fs::path path);

        /**
         * @return std::string A string that identifies an asset uniquely
         *                     based on its storage path.
         */
        auto getUniquePath() const -> std::string;

        /**
         * @return fs::path An absolute path to the asset file
         */
        auto getFilesystemPath() const -> fs::path;

        /**
         * @brief Retrieve an asset's name.
         *
         * The name is a string that identifies an asset, though it is not
         * necessarily unique! This is mostly useful for user interaction.
         *
         * Effectively, this is the filename without its extension.
         *
         * @return std::string The asset's name.
         */
        auto getAssetName() const -> std::string;

        auto operator<=>(const AssetPath&) const = default;

    private:
        /** Path relative to the asset directory */
        fs::path pathlet;
    };
} // namespace trc


namespace std
{
    template<>
    struct hash<::trc::AssetPath>
    {
        auto operator()(const ::trc::AssetPath& path) const noexcept
        {
            return hash<std::string>{}(path.getFilesystemPath());
        }
    };
}
