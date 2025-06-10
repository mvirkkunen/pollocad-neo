#pragma once

#include <optional>
#include "ast.h"

std::optional<ast::ExprList> parse(std::string code);
