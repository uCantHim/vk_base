#include "spirv/FileIncluder.h"

#include <fstream>
#include <sstream>



namespace spirv
{

FileIncluder::FileIncluder(std::vector<fs::path> _includePaths)
    :
    includePaths(std::move(_includePaths))
{
}

void FileIncluder::addIncludePath(fs::path path)
{
    includePaths.emplace_back(std::move(path));
}

auto FileIncluder::GetInclude(
    const char* requested_source,
    shaderc_include_type type,
    const char* requesting_source,
    size_t)
    -> shaderc_include_result*
{
    auto makeFullPath = [&](const fs::path& base) {
        if (type == shaderc_include_type::shaderc_include_type_relative)
        {
            const auto parentPath = fs::path{ requesting_source }.parent_path();
            if (!parentPath.is_absolute()) {
                return base / parentPath / requested_source;
            }
            return parentPath / requested_source;
        }
        return base / requested_source;
    };

    // Test all available include paths
    const fs::path path = [&]{
        for (const auto& base : includePaths)
        {
            const auto path = makeFullPath(base);
            if (fs::is_regular_file(path)) {
                return path;
            }
        }
        return fs::path{};
    }();

    auto result = std::make_unique<shaderc_include_result>();
    auto res = result.get();

    // Insert pending include result into list
    auto& data = [&]() -> IncludeResult& {
        std::scoped_lock lock(pendingResultsLock);
        auto [it, success] = pendingResults.try_emplace(res, path, std::move(result));
        assert(success);

        return it->second;
    }();

    std::ifstream file{ path };
    if (!file.is_open())
    {
        res->source_name = "";
        res->source_name_length = 0;
        res->content = "Unable to open file";
        res->content_length = strlen(res->content);
        return res;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    data.content = ss.str();

    res->source_name        = data.fullPath.c_str();
    res->source_name_length = data.fullPath.size();
    res->content            = data.content.c_str();
    res->content_length     = data.content.size();

    return res;
}

void FileIncluder::ReleaseInclude(shaderc_include_result* data)
{
    std::scoped_lock lock(pendingResultsLock);
    pendingResults.erase(data);
}

} // namespace spirv
