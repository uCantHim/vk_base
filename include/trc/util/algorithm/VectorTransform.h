#pragma once

#include <vector>

namespace trc::util
{
    /**
     * @brief Create a merged copy of two verctors
     */
    template<typename T>
    inline auto merged(const std::vector<T>& a, const std::vector<T>& b)
        -> std::vector<T>
    {
        if (a.size() > b.size())
        {
            auto result = a;
            result.insert(a.end(), b.begin(), b.end());
            return result;
        }
        else
        {
            auto result = b;
            result.insert(b.end(), a.begin(), a.end());
            return result;
        }
    }

    /**
     * @brief Merge a vector into another in-place
     *
     * @param std::vector<T>&       a This vector will contain the merged
     *                                result
     * @param const std::vector<T>& b The vector to merge into the first one
     *
     * @return std::vector<T>& Reference to the first vector
     */
    template<typename T>
    inline auto merge(std::vector<T>& a, const std::vector<T>& b) -> std::vector<T>&
    {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    }
} // namespace trc::util