#pragma once

#include <cassert>
#include <cstring>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <shaderc/shaderc.hpp>

namespace spirv
{
    namespace fs = std::filesystem;

    class FileIncluder : public shaderc::CompileOptions::IncluderInterface
    {
    public:
        /**
         * @param std::vector<fs::path> includePaths A list of directories to
         *                              search for included files.
         */
        explicit FileIncluder(std::vector<fs::path> includePaths);

        /**
         * @brief Add a path to the list of include paths.
         */
        void addIncludePath(fs::path path);

        /** Handles shaderc_include_resolver_fn callbacks. */
        auto GetInclude(const char* requested_source,
                        shaderc_include_type type,
                        const char* requesting_source,
                        size_t)
            -> shaderc_include_result* override;

        /** Handles shaderc_include_result_release_fn callbacks. */
        void ReleaseInclude(shaderc_include_result* data) override;

    private:
        struct IncludeResult
        {
            IncludeResult(const fs::path& fullPath, std::unique_ptr<shaderc_include_result> res)
                : fullPath(fullPath.string()), result(std::move(res)) {}

            std::string fullPath;
            std::unique_ptr<shaderc_include_result> result;
            std::string content;
        };

        std::vector<fs::path> includePaths;

        std::mutex pendingResultsLock;
        std::unordered_map<shaderc_include_result*, IncludeResult> pendingResults;
    };
} // namespace spirv
