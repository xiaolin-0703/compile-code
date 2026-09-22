#include <unordered_map>
#include <string>
#include "token.hpp"
#include "Lexer.hpp"
#include <vector>
#include "compileError.hpp"
#include <cctype>

const std::unordered_map<std::string, std::string> tokenType = {
    {"Ident", "IDENFR"},
    {"IntConst","INTCON"},
    {"CharConst","CHARCON"},
    {"StringConst","STRCON"},
    {"continue","CONTINUETK"},
    {"default","DEFAULTTK"},
    {"printf","PRINTFTK"},
    {"return","RETURNTK"},
    {"void","VOIDTK"},
    {"break","BREAKTK"},
    {"else","ELSETK"},
    {"if","IFTK"},
    {"int","INTTK"},
    {"main","MAINTK"},
    {"switch","SWITCHTK"},
    {"while","WHILETK"},
    {"const","CONSTTK"},
    {"char","CHARTK"},
    {"void","VOIDTK"},
    {"const","CONSTTK"},
    {"!=","NEQ"},
    {"==","EQL"},
    {">=","GEQ"},
    {"<=","LEQ"},
    {">", "GRE"},
    {"<", "LSS"},
    {"+", "PLUS"},
    {"-", "MINU"},
    {"*", "MULT"},
    {"/", "DIV"},
    {"%", "MOD"},
    {"=", "ASSIGN"},
    {";", "SEMICN"},
    {",", "COMMA"},
    {"(", "LPARENT"},
    {")", "RPARENT"},
    {"[", "LBRACK"},
    {"]", "RBRACK"},
    {"{", "LBRACE"},
    {"}", "RBRACE"},
    {":", "COLON"},
    {"!", "NOT"},
    {"&&", "AND"},
    {"||", "OR"},
    {"switch","SWITCHTK"},
    {"case", "CASETK"},
    {"static","STATICTK"},
    {"&", "AND"},
    {"|", "OR"},
};

        Lexer::Lexer(const std::string& code)
            : sourceCode(code), currentIndex(0), lineNum(1), length(code.length()) {
            startLexing();
        }

        void Lexer::startLexing() {
            while (currentIndex < length) {
                std::string word = getNextWord();
                if (!word.empty()) {
                    addToken(word,lineNum);
                }
            }
        }

        const std::vector<compileError>& Lexer::getErrors() const {
            return errors;
        }

        const std::vector<Token>& Lexer::getTokens() const {
            return tokens;
        }

        void Lexer::addToken(const std::string& word, int lineNum) {
            auto it = tokenType.find(word);

            // 关键字、运算符、分隔符
            if (it != tokenType.end()) {
                tokens.emplace_back(word, it->second, lineNum);
                return;
        }

    // 整数
            if (!word.empty() &&
                word.find_first_not_of("0123456789") == std::string::npos) {
                tokens.emplace_back(word,tokenType.at("IntConst"),
                lineNum
            );
            return;
            }

    // 字符串
        if (word.size() >= 2 && word.front() == '"' && word.back() == '"') {
            tokens.emplace_back(word,tokenType.at("StringConst"),lineNum);
            return;
        }

    // 字符常量
        if (word.size() >= 3 && word.front() == '\'' && word.back() == '\'') {
            tokens.emplace_back(word,tokenType.at("CharConst"),lineNum);
            return;
        }

    // 标识符
        if (!word.empty() &&(std::isalpha(static_cast<unsigned char>(word[0])) || word[0] == '_')) {
            bool valid = true;
            for (char c : word) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                tokens.emplace_back(word,tokenType.at("Ident"),lineNum);
                return;
            }
        }
}

        std::string Lexer::getNextWord() {
            while (currentIndex < sourceCode.length()) {
                char currentChar = sourceCode[currentIndex];
                if (isspace(currentChar)) {
                    if (currentChar == '\n') {
                        lineNum++;
                    }
                    currentIndex++;
                    continue;
                }
                if (currentChar == '&') {
                    if (currentIndex + 1 < length &&
                        sourceCode[currentIndex + 1] == '&') {

                        currentIndex += 2;
                        return "&&";
                        }
                    errors.push_back(compileError(lineNum,'a'));
                    currentIndex++;
                    return "&";
                }
                if (currentChar == '|') {
                    if (currentIndex + 1 < length &&
                        sourceCode[currentIndex + 1] == '|') {

                        currentIndex += 2;
                        return "||";
                        }
                    errors.push_back(compileError(lineNum,'a'));
                    currentIndex++;
                    return "|";
                }

                if (isalpha(currentChar) || currentChar == '_') {
                    std::string word;
                    while (currentIndex < sourceCode.length() && (isalnum(sourceCode[currentIndex]) || sourceCode[currentIndex] == '_')) {
                        word += sourceCode[currentIndex];
                        currentIndex++;
                    }
                    return word;
                }

                if (isdigit(currentChar)) {
                    std::string number;
                    if (currentChar == '0') {
                        currentIndex++;
                        return "0";
                    }
                    while (currentIndex < sourceCode.length() && isdigit(sourceCode[currentIndex])) {
                        number += sourceCode[currentIndex];
                        currentIndex++;
                    }
                    return number;
                }
               
                if (currentChar == '"') {
                    std::string strConst;
                    strConst += currentChar; // Add the opening quote
                    currentIndex++;
                    while (currentIndex < sourceCode.length() && sourceCode[currentIndex] != '"') {
                        strConst += sourceCode[currentIndex];
                        currentIndex++;
                    }
                    if (currentIndex < sourceCode.length()) {
                        strConst += sourceCode[currentIndex]; // Add the closing quote
                        currentIndex++;
                    }
                    return strConst;
                }
                
                if (currentChar == '/') {
                    if (currentIndex + 1 < sourceCode.length() && sourceCode[currentIndex + 1] == '/') {
                        // Single-line comment
                        currentIndex += 2; // Skip the "//"
                        while (currentIndex < sourceCode.length() && sourceCode[currentIndex] != '\n') {
                            currentIndex++;
                        }
                        continue; // Skip to the next iteration
                    } else if (currentIndex + 1 < sourceCode.length() && sourceCode[currentIndex + 1] == '*') {
                        // Multi-line comment
                        currentIndex += 2; // Skip the "/*"
                        while (currentIndex + 1 < sourceCode.length() && !(sourceCode[currentIndex] == '*' && sourceCode[currentIndex + 1] == '/')) {
                            if (sourceCode[currentIndex] == '\n') {
                                lineNum++;
                            }
                            currentIndex++;
                        }
                        if (currentIndex + 1 < sourceCode.length()) {
                            currentIndex += 2; // Skip the "*/"
                        }
                        continue; 
                    }    
               }
 
                if (currentChar == '\'') {
                    std::string charConst;
                    charConst += sourceCode[currentIndex];
                    currentIndex++;

                    if (currentIndex >= length) {
                        return charConst;
                    }
                    if (sourceCode[currentIndex] == '\\') {

                        charConst += sourceCode[currentIndex];
                        currentIndex++;
                        if (currentIndex < length) {
                            charConst += sourceCode[currentIndex];
                            currentIndex++;
                        }
                    }
                    else {
                        charConst += sourceCode[currentIndex];
                        currentIndex++;
                    }
                    if (currentIndex < length &&
                        sourceCode[currentIndex] == '\'') {

                        charConst += sourceCode[currentIndex];
                        currentIndex++;
                        }

                    return charConst;
                }
                if (ispunct(currentChar)) {
                    std::string punct(1, currentChar);
                    currentIndex++;
                    if (currentIndex < sourceCode.length()) {
                        std::string twoCharOp = punct + sourceCode[currentIndex];
                        if (tokenType.find(twoCharOp) != tokenType.end()) {
                            currentIndex++;
                            return twoCharOp;
                        }
                    }
                    return punct;
                }

            }
            return "";
        }

