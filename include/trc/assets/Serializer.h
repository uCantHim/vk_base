#pragma once

#include <expected>
#include <iosfwd>
#include <string>

#include <trc_util/TypeUtils.h>

#include "trc/assets/AssetBase.h"

namespace trc
{
    /**
     * @brief May be specialized for an asset type to make it serializable.
     */
    template<AssetBaseType T>
    struct AssetSerializerTraits;

    struct AssetParseError
    {
        enum class Code
        {
            eSyntaxError,
            eSemanticError,
            eSystemError,
            eOther,
        };

        Code errorCode;
        std::string message;
    };

    template<AssetBaseType Asset>
    using AssetParseResult = std::expected<AssetData<Asset>, AssetParseError>;

    template<typename Asset>
    concept SerializableAsset =
        AssetBaseType<Asset>
        && util::CompleteType<AssetSerializerTraits<Asset>>
        && util::CompleteType<AssetData<Asset>>
        && requires (AssetData<Asset> data, std::istream& is, std::ostream& os) {
            { AssetSerializerTraits<Asset>::deserialize(is) } -> std::same_as<AssetParseResult<Asset>>;
            { AssetSerializerTraits<Asset>::serialize(data, os) };
        };

    /**
     * A default implementation as a solution while I migrate the old interfaces
     * to the new AssetSerializerTraits<>.
     *
     * TODO: Remove this.
     */
    template<AssetBaseType T>
        requires std::default_initializable<AssetData<T>>
              && requires (AssetData<T> data, std::istream& is, std::ostream& os) {
                  { data.deserialize(is) };
                  { data.serialize(os) };
              }
    struct AssetSerializerTraits<T>
    {
        static auto deserialize(std::istream& is) -> AssetParseResult<T>
        {
            AssetData<T> data{};
            try {
                data.deserialize(is);
                return data;
            }
            catch (const std::exception& err) {
                return std::unexpected(AssetParseError{ AssetParseError::Code::eOther, err.what() });
            }
            catch (...) {
                return std::unexpected(AssetParseError{ AssetParseError::Code::eOther, "Unknown error" });
            }
        }

        static void serialize(const AssetData<T>& data, std::ostream& os)
        {
            data.serialize(os);
        }
    };
} // namespace trc
