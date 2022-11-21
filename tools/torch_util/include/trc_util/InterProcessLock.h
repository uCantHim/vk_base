#pragma once

#include <string>

#include <semaphore.h>

namespace trc::util
{
    class InterProcessLock
    {
    public:
        InterProcessLock(const InterProcessLock&) = delete;
        InterProcessLock(InterProcessLock&&) = delete;
        InterProcessLock& operator=(const InterProcessLock&) = delete;
        InterProcessLock& operator=(InterProcessLock&&) = delete;

        /**
         * @param const char* name Must begin with a slash. Must not be
         *        longer than 251 characters.
         *
         * @throw std::invalid_argument if `name` is not a valid semaphore name
         * @throw std::runtime_error if semaphore creation fails.
         */
        explicit InterProcessLock(const char* name);
        ~InterProcessLock() noexcept;

        void lock();
        void unlock();

    private:
        sem_t* semaphore;
    };
} // namespace trc::util