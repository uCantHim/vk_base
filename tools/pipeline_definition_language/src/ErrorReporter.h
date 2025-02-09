#pragma once

#include <cstdint>

#include <ostream>
#include <string>

#include "Token.h"

struct Error
{
    TokenLocation location;
    std::string message;
};

class ErrorReporter
{
public:
    virtual ~ErrorReporter() = default;

    void error(const Error& error);
    bool hadError() const;

protected:
    virtual void reportError(const Error& error) = 0;

private:
    uint32_t numErrors{ 0 };
};

class DefaultErrorReporter : public ErrorReporter
{
public:
    explicit DefaultErrorReporter(std::ostream& os);

    void reportError(const Error& error) override;

private:
    std::ostream* os;
};
