#pragma once

#include <filesystem>
#include <string>

namespace trc::util
{
    namespace fs = std::filesystem;

    /**
     * @brief Read the contents of a file into a string
     *
     * @throw std::runtime_error if the file cannot be opened.
     */
    auto readFile(const fs::path& path) -> std::string;
} // namespace trc::util
