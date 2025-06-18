#pragma once

#include <atomic>
#include <optional>

#include "logmessage.h"
#include "value.h"

struct ExecutorResult
{
    std::optional<Value> result;
    std::vector<LogMessage> messages;
};

class ExecutionContext;
class Environment;

class Executor
{
public:
    Executor();
    ExecutorResult execute(const std::string &code);
    bool isBusy() const;

private:
    std::shared_ptr<ExecutionContext> m_currentContext;
    std::shared_ptr<Environment> m_defaultEnvironment;
};
