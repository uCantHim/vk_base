#pragma once

#include <concepts>
#include <limits>

#include <trc_util/data/SafeVector.h>

#include "trc/Types.h"

namespace trc
{
    template<typename DeviceData>
    class DeviceDataCache
    {
    private:
        class ReferenceCounter;
        class SharedRef;

        using Self = DeviceDataCache<DeviceData>;

    public:
        static_assert(std::move_constructible<DeviceData>);

        /**
         * @brief A lightweight handle to a cache entry
         *
         * Is reference counted. The entry is removed from the cache when no
         * handles exist anymore.
         */
        struct CacheEntryHandle
        {
        public:
            CacheEntryHandle(const CacheEntryHandle&) = default;
            CacheEntryHandle(CacheEntryHandle&&) noexcept = default;
            CacheEntryHandle& operator=(const CacheEntryHandle&) = default;
            CacheEntryHandle& operator=(CacheEntryHandle&&) noexcept = default;
            ~CacheEntryHandle() noexcept = default;

            auto operator->() -> DeviceData* { return data; }
            auto operator->() const -> const DeviceData* { return data; }
            auto operator*() -> DeviceData& { return *data; }
            auto operator*() const -> const DeviceData& { return *data; }

        private:
            friend Self;
            CacheEntryHandle(DeviceData& data, ReferenceCounter& counter)
                : data(&data), refCount(counter)
            {}

            DeviceData* data;
            SharedRef refCount;
        };

        /**
         * @brief An interface to the cache owner
         *
         * Defines operations to create device data for an item and to free it
         * once it is removed from the cache.
         */
        class Loader
        {
        public:
            virtual ~Loader() noexcept = default;

            virtual auto loadDeviceData(ui32 id) -> DeviceData = 0;
            virtual void freeDeviceData(ui32 id, DeviceData data) = 0;
        };

        /**
         * @brief Construct a device data cache object
         *
         * @param s_ptr<Loader> An implementation of the `Loader` interface.
         *                      Must not be nullptr.
         */
        explicit DeviceDataCache(s_ptr<Loader> dataLoader);

        /**
         * @brief Retrieve an entry from the cache
         *
         * Loads the item lazily if it is not currently cached.
         *
         * @param ui32 id The item to retrieve
         */
        auto get(ui32 id) -> CacheEntryHandle;

        /**
         * @brief Create an ad-hoc implementation of the `Loader` interface
         */
        template<typename LoadFunc, typename FreeFunc>
            requires requires (LoadFunc load, FreeFunc free, ui32 id, DeviceData data) {
                { load(id) } -> std::convertible_to<DeviceData>;
                { free(id, std::move(data)) } -> std::same_as<void>;
            }
        static auto makeLoader(LoadFunc load, FreeFunc free) -> u_ptr<Loader>;

    private:
        /**
         * @brief A reference counter
         *
         * Create shared references through `SharedCacheReference`.
         */
        class ReferenceCounter
        {
        public:
            ReferenceCounter() = delete;
            ReferenceCounter(const ReferenceCounter&) = delete;
            ReferenceCounter(ReferenceCounter&&) noexcept = delete;
            ReferenceCounter& operator=(const ReferenceCounter&) = delete;
            ReferenceCounter& operator=(ReferenceCounter&&) noexcept = delete;

            ReferenceCounter(ui32 asset, Self& owningCache)
                : asset(asset), cache(&owningCache)
            {}

            ~ReferenceCounter() = default;

            void incRefCount()
            {
                assert(count < std::numeric_limits<decltype(count)>::max());
                ++count;
            }

            void decRefCount()
            {
                assert(count > 0);
                assert(cache != nullptr);
                if (--count == 0) {
                    cache->unload(asset);
                }
            }

        private:
            ui32 count{ 0 };
            ui32 asset;
            Self* cache;
        };

        /**
         * A lifetime-based guard object that counts references via an external
         * reference counter.
         *
         * Must reference an object, thus move operations have copy semantics.
         */
        class SharedRef
        {
        public:
            explicit SharedRef(ReferenceCounter& _cacheItem);
            SharedRef(const SharedRef& other);
            SharedRef(SharedRef&& other) noexcept;
            ~SharedRef();

            auto operator=(const SharedRef& other) -> SharedRef&;
            auto operator=(SharedRef&& other) noexcept -> SharedRef&;

        protected:
            ReferenceCounter* counter;
        };

        struct CacheEntry
        {
            u_ptr<ReferenceCounter> refCount;
            DeviceData data;
        };

        void unload(ui32 id);

        s_ptr<Loader> dataLoader;
        util::SafeVector<CacheEntry> entries;
    };



    template<typename DeviceData>
    DeviceDataCache<DeviceData>::DeviceDataCache(
        s_ptr<Loader> dataLoader)
        :
        dataLoader(dataLoader)
    {
        assert(this->dataLoader != nullptr);
    }

    template<typename DeviceData>
    auto DeviceDataCache<DeviceData>::get(ui32 id) -> CacheEntryHandle
    {
        if (!entries.contains(id))
        {
            entries.emplace(id, CacheEntry{
                .refCount=std::make_unique<ReferenceCounter>(id, *this),
                .data=dataLoader->loadDeviceData(id)
            });
        }

        auto& [refCount, data] = entries.at(id);
        return CacheEntryHandle{ data, *refCount };
    }

    template<typename DeviceData>
    template<typename LoadFunc, typename FreeFunc>
        requires requires (LoadFunc load, FreeFunc free, ui32 id, DeviceData data) {
            { load(id) } -> std::convertible_to<DeviceData>;
            { free(id, std::move(data)) } -> std::same_as<void>;
        }
    auto DeviceDataCache<DeviceData>::makeLoader(LoadFunc load, FreeFunc free) -> u_ptr<Loader>
    {
        struct LoaderImpl : public Loader
        {
            LoaderImpl(LoadFunc l, FreeFunc f) : _load(std::move(l)), _free(std::move(f)) {}

            auto loadDeviceData(ui32 id) -> DeviceData override { return _load(id); }
            void freeDeviceData(ui32 id, DeviceData data) override { _free(id, std::move(data)); }

            LoadFunc _load;
            FreeFunc _free;
        };

        return std::make_unique<LoaderImpl>(std::move(load), std::move(free));
    }

    template<typename DeviceData>
    void DeviceDataCache<DeviceData>::unload(ui32 id)
    {
        dataLoader->freeDeviceData(id, std::move(entries.at(id).data));
        entries.erase(id);
    }



    template<typename DeviceData>
    DeviceDataCache<DeviceData>::SharedRef::SharedRef(ReferenceCounter& _cacheItem)
        : counter(&_cacheItem)
    {
        counter->incRefCount();
    }

    template<typename DeviceData>
    DeviceDataCache<DeviceData>::SharedRef::SharedRef(const SharedRef& other)
        : counter(other.counter)
    {
        counter->incRefCount();
    }

    template<typename DeviceData>
    DeviceDataCache<DeviceData>::SharedRef::SharedRef(SharedRef&& other) noexcept
        : counter(other.counter)
    {
        counter->incRefCount();
    }

    template<typename DeviceData>
    DeviceDataCache<DeviceData>::SharedRef::~SharedRef()
    {
        counter->decRefCount();
    }

    template<typename DeviceData>
    auto DeviceDataCache<DeviceData>::SharedRef::operator=(const SharedRef& other)
        -> SharedRef&
    {
        if (this != &other)
        {
            counter->decRefCount();
            counter = other.counter;
            counter->incRefCount();
        }
        return *this;
    }

    template<typename DeviceData>
    auto DeviceDataCache<DeviceData>::SharedRef::operator=(SharedRef&& other) noexcept
        -> SharedRef&
    {
        if (this != &other)
        {
            counter->decRefCount();
            counter = other.counter;
            counter->incRefCount();
        }
        return *this;
    }
} // namespace trc
