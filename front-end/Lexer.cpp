#include "Lexer.hpp"

#include <charconv>
#include <cctype>
#include <string>
#include <unordered_map>

namespace {

const std::unordered_map<std::string, TokenType> lexemeToTokenType = {
    {"continue", TokenType::CONTINUETK},
    {"default", TokenType::DEFAULTTK},
    {"printf", TokenType::PRINTFTK},
    {"return", TokenType::RETURNTK},
    {"void", TokenType::VOIDTK},
    {"break", TokenType::BREAKTK},
    {"else", TokenType::ELSETK},
    {"if", TokenType::IFTK},
    {"int", TokenType::INTTK},
    {"main", TokenType::MAINTK},
    {"while", TokenType::WHILETK},
    {"char", TokenType::CHARTK},
    {"const", TokenType::CONSTTK},
    {"!=", TokenType::NEQ},
    {"==", TokenType::EQL},
    {">=", TokenType::GEQ},
    {"<=", TokenType::LEQ},
    {">", TokenType::GRE},
    {"<", TokenType::LSS},
    {"+", TokenType::PLUS},
    {"-", TokenType::MINU},
    {"*", TokenType::MULT},
    {"/", TokenType::DIV},
    {"%", TokenType::MOD},
    {"=", TokenType::ASSIGN},
    {";", TokenType::SEMICN},
    {",", TokenType::COMMA},
    {"(", TokenType::LPARENT},
    {")", TokenType::RPARENT},
    {"[", TokenType::LBRACK},
    {"]", TokenType::RBRACK},
    {"{", TokenType::LBRACE},
    {"}", TokenType::RBRACE},
    {":", TokenType::COLON},
    {"!", TokenType::NOT},
    {"&&", TokenType::AND},
    {"||", TokenType::OR},
    {"switch", TokenType::SWITCHTK},
    {"case", TokenType::CASETK},
    {"static", TokenType::STATICTK},
    {"&", TokenType::AND},
    {"|", TokenType::OR},
};

bool isSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool isAlpha(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool isAlnum(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

bool isPunct(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return !std::isalnum(uc) && !std::isspace(uc) && c != '_';
}

char decodeEscape(char c) {
    switch (c) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
        case '0': return '\0';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        default: return c;
    }
}

char decodeCharLiteral(const std::string& word) {
    if (word.size() >= 4 && word[1] == '\\') {
        return decodeEscape(word[2]);
    }
    return word.size() >= 3 ? word[1] : '\0';
}

std::string decodeStringLiteral(const std::string& word) {
    std::string value;
    if (word.size() < 2) {
        return value;
    }

    for (std::size_t i = 1; i + 1 < word.size(); ++i) {
        if (word[i] == '\\' && i + 2 < word.size()) {
            value += decodeEscape(word[++i]);
        } else {
            value += word[i];
        }
    }
    return value;
}

}  

Lexer::Lexer(const std::string& code)
    : sourceCode(code), currentIndex(0), lineNum(1), length(code.length()) {
    startLexing();
}

void Lexer::startLexing() {
    while (currentIndex < length) {
        std::string word = getNextWord();
        if (!word.empty()) {
            addToken(word, lineNum);
        }
    }
    tokens.emplace_back("", TokenType::END_OF_FILE, lineNum);
}

const std::vector<compileError>& Lexer::getErrors() const {
    return errors;
}

const std::vector<Token>& Lexer::getTokens() const {
    return tokens;
}

void Lexer::addToken(const std::string& word, int tokenLineNum) {
    const auto it = lexemeToTokenType.find(word);
    if (it != lexemeToTokenType.end()) {
        tokens.emplace_back(word, it->second, tokenLineNum);
        return;
    }

    // 整数
    if (!word.empty() &&
        word.find_first_not_of("0123456789") == std::string::npos) {
        int value = 0;
        const auto result = std::from_chars(word.data(), word.data() + word.size(), value);
        if (result.ec == std::errc{}) {
            tokens.emplace_back(word, TokenType::INTCON, tokenLineNum, value);
        } else {
            tokens.emplace_back(word, TokenType::INTCON, tokenLineNum);
        }
        return;
    }

    // 字符串
    if (word.size() >= 2 && word.front() == '"' && word.back() == '"') {
        tokens.emplace_back(word, TokenType::STRCON, tokenLineNum,
                            decodeStringLiteral(word));
        return;
    }

    // 字符常量
    if (word.size() >= 3 && word.front() == '\'' && word.back() == '\'') {
        tokens.emplace_back(word, TokenType::CHARCON, tokenLineNum,
                            decodeCharLiteral(word));
        return;
    }

    // 标识符
    if (!word.empty() && (isAlpha(word[0]) || word[0] == '_')) {
        bool valid = true;
        for (char c : word) {
            if (!isAlnum(c) && c != '_') {
                valid = false;
                break;
            }
        }
        if (valid) {
            tokens.emplace_back(word, TokenType::IDENFR, tokenLineNum, word);
            return;
        }
    }

    tokens.emplace_back(word, TokenType::INVALID, tokenLineNum);
}

std::string Lexer::getNextWord() {
    while (currentIndex < length) {
        const char currentChar = sourceCode[currentIndex];
        if (isSpace(currentChar)) {
            if (currentChar == '\n') {
                ++lineNum;
            }
            ++currentIndex;
            continue;
        }

        if (currentChar == '&') {
            if (currentIndex + 1 < length && sourceCode[currentIndex + 1] == '&') {
                currentIndex += 2;
                return "&&";
            }
            errors.emplace_back(lineNum, 'a');
            ++currentIndex;
            return "&";
        }

        if (currentChar == '|') {
            if (currentIndex + 1 < length && sourceCode[currentIndex + 1] == '|') {
                currentIndex += 2;
                return "||";
            }
            errors.emplace_back(lineNum, 'a');
            ++currentIndex;
            return "|";
        }

        if (isAlpha(currentChar) || currentChar == '_') {
            std::string word;
            while (currentIndex < length &&
                   (isAlnum(sourceCode[currentIndex]) || sourceCode[currentIndex] == '_')) {
                word += sourceCode[currentIndex++];
            }
            return word;
        }

        if (isDigit(currentChar)) {
            std::string number;
            if (currentChar == '0') {
                ++currentIndex;
                return "0";
            }
            while (currentIndex < length && isDigit(sourceCode[currentIndex])) {
                number += sourceCode[currentIndex++];
            }
            return number;
        }

        if (currentChar == '"') {
            std::string strConst;
            strConst += sourceCode[currentIndex++];
            while (currentIndex < length) {
                const char c = sourceCode[currentIndex++];
                strConst += c;
                if (c == '\\' && currentIndex < length) {
                    strConst += sourceCode[currentIndex++];
                    continue;
                }
                if (c == '"') {
                    break;
                }
                if (c == '\n') {
                    ++lineNum;
                }
            }
            return strConst;
        }

        if (currentChar == '/') {
            if (currentIndex + 1 < length && sourceCode[currentIndex + 1] == '/') {
                currentIndex += 2;
                while (currentIndex < length && sourceCode[currentIndex] != '\n') {
                    ++currentIndex;
                }
                continue;
            }
            if (currentIndex + 1 < length && sourceCode[currentIndex + 1] == '*') {
                currentIndex += 2;
                bool closed = false;
                while (currentIndex < length) {
                    if (currentIndex + 1 < length && sourceCode[currentIndex] == '*' &&
                        sourceCode[currentIndex + 1] == '/') {
                        currentIndex += 2;
                        closed = true;
                        break;
                    }
                    if (sourceCode[currentIndex] == '\n') {
                        ++lineNum;
                    }
                    ++currentIndex;
                }
                if (!closed) {
                    currentIndex = length;
                }
                continue;
            }
        }

        if (currentChar == '\'') {
            std::string charConst;
            charConst += sourceCode[currentIndex++];
            if (currentIndex >= length) {
                return charConst;
            }
            if (sourceCode[currentIndex] == '\\') {
                charConst += sourceCode[currentIndex++];
                if (currentIndex < length) {
                    charConst += sourceCode[currentIndex++];
                }
            } else {
                charConst += sourceCode[currentIndex++];
            }
            if (currentIndex < length && sourceCode[currentIndex] == '\'') {
                charConst += sourceCode[currentIndex++];
            }
            return charConst;
        }

        if (isPunct(currentChar)) {
            std::string punct(1, currentChar);
            ++currentIndex;
            if (currentIndex < length) {
                const std::string twoCharOp = punct + sourceCode[currentIndex];
                if (lexemeToTokenType.find(twoCharOp) != lexemeToTokenType.end()) {
                    ++currentIndex;
                    return twoCharOp;
                }
            }
            return punct;
        }

        ++currentIndex;
        return std::string(1, currentChar);
    }
    return "";
}
