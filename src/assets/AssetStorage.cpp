#include "trc/assets/AssetStorage.h"

#include "asset.pb.h"



namespace trc
{

AssetStorage::AssetStorage(s_ptr<DataStorage> storage)
    :
    storage(storage)
{
    assert(this->storage != nullptr);
}

auto AssetStorage::getMetadata(const AssetPath& path) -> std::optional<AssetMetadata>
{
    auto metaStream = storage->read(makeMetaPath(path));
    if (metaStream != nullptr) {
        return deserializeMetadata(*metaStream);
    }
    return std::nullopt;
}

bool AssetStorage::remove(const AssetPath& path)
{
    const bool res1 = storage->remove(makeMetaPath(path));
    const bool res2 = storage->remove(makeDataPath(path));
    return res1 && res2;
}

auto AssetStorage::makeMetaPath(const AssetPath& path) -> util::Pathlet
{
    return util::Pathlet(path.string() + ".meta");
}

auto AssetStorage::makeDataPath(const AssetPath& path) -> util::Pathlet
{
    return util::Pathlet(path.string() + ".data");
}

void AssetStorage::serializeMetadata(const AssetMetadata& meta, std::ostream& os)
{
    serial::AssetMetadata serial;
    serial.set_name(meta.name);
    serial.mutable_type()->set_name(meta.type.getName());
    if (meta.path.has_value()) {
        serial.set_path(meta.path->string());
    }

    serial.SerializeToOstream(&os);
}

auto AssetStorage::deserializeMetadata(std::istream& is) -> AssetMetadata
{
    serial::AssetMetadata serial;
    serial.ParseFromIstream(&is);

    AssetMetadata meta{
        .name=serial.name(),
        .type=AssetType::make(serial.type().name()),
    };
    if (serial.has_path()) {
        meta.path = AssetPath(serial.path());
    }

    return meta;
}

auto AssetStorage::begin() -> iterator
{
    return { storage->begin(), storage->end() };
}

auto AssetStorage::end() -> iterator
{
    return { storage->end(), storage->end() };
}



AssetStorage::AssetIterator::AssetIterator(
    DataStorage::iterator _begin,
    DataStorage::iterator _end)
    :
    iter(std::move(_begin)),
    end(std::move(_end))
{
    step();
}

auto AssetStorage::AssetIterator::operator*() const -> const_reference
{
    return *currentPath;
}

auto AssetStorage::AssetIterator::operator->() const -> const_pointer
{
    return &*currentPath;
}

auto AssetStorage::AssetIterator::operator++() -> AssetIterator&
{
    ++iter;
    step();
    return *this;
}

bool AssetStorage::AssetIterator::operator==(const AssetIterator& other) const
{
    return iter == other.iter;
}

bool AssetStorage::AssetIterator::isMetaFile(const util::Pathlet& path)
{
    return path.filename().extension() == ".meta";
}

void AssetStorage::AssetIterator::step()
{
    while (iter != end && !isMetaFile(*iter)) ++iter;
    if (iter != end) {
        currentPath = AssetPath(iter->replaceExtension(""));
    }
}

} // namespace trc
