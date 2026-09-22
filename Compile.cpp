#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "front-end/compileError.hpp"
#include "front-end/Lexer.hpp"


int main() {
    std::ifstream inputFile("testfile.txt");

    if (!inputFile.is_open()) {
        std::cerr << "无法打开 testfile.txt" << std::endl;
        return 1;
    }
    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    std::string sourceCode = buffer.str();
    inputFile.close();
    Lexer lexer(sourceCode);
    const std::vector<Token>& tokens = lexer.getTokens();
    const std::vector<compileError>& errors = lexer.getErrors();
    if (!errors.empty()) {
        std::ofstream errorFile("error.txt");
        if (!errorFile.is_open()) {
            return 1;
        }
        for (const auto& error : errors) {
            errorFile << error.getLineNum()
                      << ' '
                      << error.getErrorType()
                      << '\n';
        }
        return 0;
    }

    std::ofstream lexerFile("lexer.txt");
    if (!lexerFile.is_open()) {
        return 1;
    }
    for (const auto& token : tokens) {
        lexerFile << token.getType() << ' ' << token.getWord() << '\n';
    }
    return 0;
}