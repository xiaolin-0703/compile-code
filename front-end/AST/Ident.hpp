#pragma once

#include <string>
#include <utility>

#include "../Token.hpp"

class Ident {
public:
    explicit Ident(const Token& token)
        : name(token.getWord()), lineNum(token.getLineNum()) {}

    Ident(std::string name, int lineNum)
        : name(std::move(name)), lineNum(lineNum) {}

    const std::string& getName() const {
        return name;
    }

    int getLineNum() const {
        return lineNum;
    }

private:
    std::string name;
    int lineNum;
};
