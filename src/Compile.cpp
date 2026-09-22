#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "front-end/compileError.hpp"
#include "front-end/Lexer.hpp"


int main() {
    std::ifstream inputFile("../src/testfile.txt");

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
        for (auto i:errors) {
            i.outError();
            return 0;
        }
    }
    for (const Token& token : tokens) {
        std::cout
            << token.getType()
            << " "
            << token.getWord()
            << std::endl;
    }

    return 0;
}