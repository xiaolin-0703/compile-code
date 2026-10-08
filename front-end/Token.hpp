#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <variant>

enum class TokenType {
    IDENFR,
    INTCON,
    CHARCON,
    STRCON,
    CONTINUETK,
    DEFAULTTK,
    PRINTFTK,
    RETURNTK,
    VOIDTK,
    BREAKTK,
    ELSETK,
    IFTK,
    INTTK,
    MAINTK,
    SWITCHTK,
    WHILETK,
    CONSTTK,
    CHARTK,
    CASETK,
    STATICTK,
    NEQ,
    EQL,
    GEQ,
    LEQ,
    GRE,
    LSS,
    PLUS,
    MINU,
    MULT,
    DIV,
    MOD,
    ASSIGN,
    SEMICN,
    COMMA,
    LPARENT,
    RPARENT,
    LBRACK,
    RBRACK,
    LBRACE,
    RBRACE,
    COLON,
    NOT,
    AND,
    OR,
    END_OF_FILE,
    INVALID
};

constexpr std::string_view tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::IDENFR:
            return "IDENFR";
        case TokenType::INTCON:
            return "INTCON";
        case TokenType::CHARCON:
            return "CHARCON";
        case TokenType::STRCON:
            return "STRCON";
        case TokenType::INTTK:
            return "INTTK";
        case TokenType::CHARTK:
            return "CHARTK";
        case TokenType::PLUS:
            return "PLUS";
        case TokenType::MINU:
            return "MINU";
        case TokenType::END_OF_FILE:
            return "EOF";
        case TokenType::INVALID:
            return "INVALID";
        case TokenType::PRINTFTK:
            return "PRINTFTK";
        case TokenType::RETURNTK:
            return "RETURNTK";
        case TokenType::VOIDTK:
            return "VOIDTK";
        case TokenType::BREAKTK:
            return "BREAKTK";
        case TokenType::ELSETK:
            return "ELSETK";
        case TokenType::IFTK:
            return "IFTK";
        case TokenType::MAINTK:
            return "MAINTK";
        case TokenType::SWITCHTK:
            return "SWITCHTK";
        case TokenType::WHILETK:
            return "WHILETK";
        case TokenType::CONSTTK:
            return "CONSTTK";
        case TokenType::CASETK:
            return "CASETK";
        case TokenType::STATICTK:
            return "STATICTK";
        case TokenType::NEQ:
            return "NEQ";
        case TokenType::EQL:
            return "EQL";
        case TokenType::GEQ:
            return "GEQ";
        case TokenType::LEQ:
            return "LEQ";
        case TokenType::GRE:
            return "GRE";
        case TokenType::LSS:
            return "LSS";
        case TokenType::MULT:
            return "MULT";
        case TokenType::DIV:
            return "DIV";
        case TokenType::MOD:
            return "MOD";
        case TokenType::ASSIGN:
            return "ASSIGN";
        case TokenType::SEMICN:
            return "SEMICN";
        case TokenType::COMMA:
            return "COMMA";
        case TokenType::LPARENT:
            return "LPARENT";
        case TokenType::RPARENT:
            return "RPARENT";
        case TokenType::LBRACK:
            return "LBRACK";
        case TokenType::RBRACK:
            return "RBRACK";
        case TokenType::LBRACE:
            return "LBRACE";
        case TokenType::RBRACE:
            return "RBRACE";
        case TokenType::COLON:
            return "COLON";
        case TokenType::NOT:
            return "NOT";
        case TokenType::AND:
            return "AND";
        case TokenType::OR:
            return "OR";
        case TokenType::CONTINUETK:
            return "CONTINUETK";
        case TokenType::DEFAULTTK:
            return "DEFAULTTK";
    }
    return  "UNKNOWN";
};

using TokenValue = std::variant<std::monostate, int, char, std::string>;

class Token {
public:
    Token(std::string word, TokenType type, int lineNum,
          TokenValue value = std::monostate{})
        : word(std::move(word)), type(type), lineNum(lineNum),
          value(std::move(value)) {}

    const std::string& getWord() const {
        return word;
    }

    TokenType getType() const {
        return type;
    }

    int getLineNum() const {
        return lineNum;
    }

    const TokenValue& getValue() const {
        return value;
    }

private:
    std::string word;
    TokenType type;
    int lineNum;
    TokenValue value;
};
