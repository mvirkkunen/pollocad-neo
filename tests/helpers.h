#pragma once

#include <string>
#include <sstream>

template <typename T>
char *toAllocatedString(const T &obj) {
    std::string s = (std::stringstream{} << "\n" << obj).str();
    char *r = new char[s.size() + 1];
    std::copy(s.begin(), s.end(), r);
    r[s.size()] = '\0';
    return r;
}
