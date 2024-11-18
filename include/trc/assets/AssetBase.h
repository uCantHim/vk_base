#pragma once

#include <concepts>
#include <string_view>

#include <trc_util/TypeUtils.h>
#include <trc_util/data/TypesafeId.h>

#include "trc/Types.h"

namespace trc
{
    /**
     * @brief Typedef that retrieves an asset type's registry module
     */
    template<typename T>
    using AssetRegistryModule = typename T::Registry;

    /**
     * @brief Forward declaration that has to be specialized for each asset type
     *
     * Two methods *must* be implemented for an AssetData<> specialization:
     *
     *  - AssetData<T>::serialize    of the type `void(std::ostream&)`
     *  - AssetData<T>::deserialize  of the type `void(std::istream&)`
     *
     * A method `AssetData<T>::resolveReferences(AssetManager&)` may be
     * implemented if the data contains references to other assets. In this
     * method, `AssetReference<>::resolve` should be called for all
     * references associated with the AssetData object.
     */
    template<typename T>
    struct AssetData;

    /**
     * @brief Has to be specialized for each asset type
     */
    template<typename T>
    class AssetHandle;

    /**
     * @brief Static interface requirements for an asset type
     */
    template<typename T>
    concept AssetBaseType = requires {
        // Don't do funny business with `T`. It's just supposed to provide
        // static definitions.
        requires std::semiregular<T>;

        // T must define a type `Registry`. This is the device registry
        // implementation for the asset type.
        typename T::Registry;

        // T must define a constexpr function `name` that returns a unique
        // string identifier for the asset type.
        { T::name() } -> std::convertible_to<std::string_view>;
        T::name().length() > 0;  // Also requires that T::name is indeed constexpr

        // The template `AssetData<>` must be specialized for `T`.
        requires util::CompleteType<AssetData<T>>;
    };

    template<AssetBaseType T>
    struct AssetBaseTypeTraits
    {
    public:
                           /* Hard */
        using LocalID = data::TypesafeID<T, /* AssetRegistryModule<T>, */ ui32>;

        using Data = AssetData<T>;
        using Handle = AssetHandle<T>;
        using Registry = typename T::Registry;
    };
} // namespace trc
