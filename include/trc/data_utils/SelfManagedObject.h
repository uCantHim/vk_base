#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <optional>

#include "IndexMap.h"

namespace trc::data
{
    /**
     * @brief CRTP interface with a static object collection and unique IDs
     *
     * Useful for long-living, globally accessible objects with permanent IDs.
     *
     * Not useful for fast iteration or rapid reallocations.
     *
     * @tparam Derived The derived class
     */
    template<class Derived>
    class SelfManagedObject
    {
    public:
        using ID = uint64_t;

        // ----------------
        // Static functions

        template<typename ...ConstructArgs>
        static auto createAtNextIndex(ConstructArgs&&... args)
            -> std::pair<ID, std::reference_wrapper<Derived>>;

        /**
         * @throws Error if the index is occupied
         */
        template<typename ...ConstructArgs>
        static auto create(ID index, ConstructArgs&&... args) -> Derived&;

        static auto at(ID index) -> Derived&;

        static void destroy(ID index);

        // ------------------
        // Non-static methods

        auto id() const noexcept -> ID;

    private:
        using StoredType = Derived;

        static inline IndexMap<ID, StoredType> objects;

        ID myId;
    };


#include "SelfManagedObject.inl"

} // namespace trc::data