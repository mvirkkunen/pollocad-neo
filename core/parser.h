#pragma once

#include <optional>
#include <vector>
#include "ast.h"
#include "logmessage.h"

struct ParseResult {
    std::optional<ast::ExprList> result;
    std::vector<LogMessage> errors;
};

ParseResult parse(std::string code);
