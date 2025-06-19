#pragma once

#include <optional>
#include <vector>
#include "ast.h"
#include "logmessage.h"

struct ParseResult {
    std::optional<ast::Expr> result;
    std::vector<LogMessage> errors;
};

ParseResult parse(std::string code, bool enableSpans = true);
