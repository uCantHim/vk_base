#pragma once

#include "input/InputCommand.h"

class ObjectTranslateCommand : public InputCommand
{
public:
    ObjectTranslateCommand() = default;

    void execute(CommandCall& call) override;
};