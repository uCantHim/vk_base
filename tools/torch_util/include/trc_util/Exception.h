#pragma once

#include <string>
#include <exception>
#include <ostream>

namespace trc
{
    class Exception : public std::exception
    {
    public:
        Exception(const Exception&) noexcept = default;
        Exception(Exception&&) noexcept = default;
        Exception& operator=(const Exception&) noexcept = default;
        Exception& operator=(Exception&&) noexcept = default;
        ~Exception() noexcept override = default;

        explicit Exception(std::string errorMsg = "")
            : errorMsg(std::move(errorMsg))
        {}

        auto what() const noexcept -> const char* override {
            return errorMsg.c_str();
        }

    private:
        std::string errorMsg;
    };

    /**
     * @brief Overloaded stream input operator for Exception
     */
    inline auto operator<<(std::ostream& s, const Exception& e) -> std::ostream&
    {
        s << e.what();
        return s;
    }
}
