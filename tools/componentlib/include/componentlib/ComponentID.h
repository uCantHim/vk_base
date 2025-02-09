#pragma once

#include <functional>
#include <ostream>
#include <stdexcept>

#include "TableBase.h"

namespace componentlib
{

/**
 * A default implementation for an object ID
 */
template<typename TypeTag>
struct ComponentID
{
    struct NoneType {};
    static constexpr NoneType NONE{};

    constexpr ComponentID() = default;
    constexpr ComponentID(NoneType) : ComponentID() {}

    explicit ComponentID(uint32_t _id) : id(_id) {}

    /**
     * @throw std::invalid_argument if `*this == NONE`.
     */
    template<typename T> requires std::constructible_from<T, uint32_t>
                               && std::is_integral_v<T>
    constexpr explicit operator T() const
    {
        if (*this == NONE) {
            throw std::invalid_argument("Unable to cast ComponentID that is NONE to a number!");
        }
        return T{id};
    }

    constexpr auto operator=(const NoneType&) -> ComponentID&
    {
        id = UINT32_MAX;
        return *this;
    }

    constexpr auto operator<=>(const ComponentID&) const noexcept = default;
    constexpr bool operator==(const ComponentID& rhs) const noexcept {
        return id == rhs.id;
    }

    constexpr bool operator==(const NoneType&) const {
        return id == UINT32_MAX;
    }

    template<typename T>
    friend auto operator<<(std::ostream& s, const ComponentID<T>& obj) -> std::ostream&;

    inline auto toString() const -> std::string
    {
        if (*this == NONE) {
            return "NONE";
        }
        return std::to_string(id);
    }

private:
    // template<typename, TableKey>
    // friend class ComponentStorage;  // Required for ID allocation
    // friend struct std::hash<ComponentID>;

    uint32_t id{ UINT32_MAX };
};

template<typename T>
inline auto operator<<(std::ostream& s, const ComponentID<T>& obj) -> std::ostream&
{
    if (obj == ComponentID<T>::NONE) s << "NONE";
    else                             s << obj.id;

    return s;
}

} // namespace componentlib

template<typename T>
struct std::hash<componentlib::ComponentID<T>>
{
    size_t operator()(const componentlib::ComponentID<T>& obj) const noexcept {
        return obj.id;
    }
};

template<typename T>
inline auto to_string(const componentlib::ComponentID<T>& id) -> std::string
{
    return id.toString();
}
