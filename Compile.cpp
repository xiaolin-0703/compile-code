#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "compileError.hpp"
#include "Visitor.hpp"
#include "front-end/Lexer.hpp"
#include "front-end/Parser.hpp"

int main() {
 /*---------------------前端阶段----------*/
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
    Parser parser(tokens);
    CompUnit ast = parser.parse();
    Vistor vistor(ast);
    SymbolManager symbols = vistor.getSymbolManager();

    std::vector<compileError> errors = lexer.getErrors();
    const auto& parserErrors = parser.getErrors();
    const auto& visitorErrors = vistor.getErrors();

    errors.insert(errors.end(), parserErrors.begin(), parserErrors.end());
    errors.insert(errors.end(), visitorErrors.begin(), visitorErrors.end());

    std::stable_sort(errors.begin(), errors.end(),
        [](const compileError& lhs, const compileError& rhs) {
            return lhs.getLineNum() < rhs.getLineNum();
        });

    if (!errors.empty()) {
        std::ofstream("lexer.txt", std::ios::trunc).close();
        std::ofstream("parser.txt", std::ios::trunc).close();
        std::ofstream("symbol.txt",std::ios::trunc).close();

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

    std::ofstream("error.txt", std::ios::trunc).close();
    std::ofstream lexerFile("lexer.txt");
    if (!lexerFile.is_open()) {
        return 1;
    }
    for (const auto& token : tokens) {
        if (token.getType() == TokenType::END_OF_FILE) {
            continue;
        }
        lexerFile << tokenTypeToString(token.getType()) << ' ' << token.getWord() << '\n';
    }

    std::ofstream parserFile("parser.txt");
    if (!parserFile.is_open()) {
        return 1;
    }
    for (const auto& line : parser.getOutput()) {
        parserFile << line << '\n';
    }

    std::ofstream visitorFile("symbol.txt");
    if (!visitorFile.is_open()) {
        return 1;
    }
    symbols.output(visitorFile);

    return 0;
}
