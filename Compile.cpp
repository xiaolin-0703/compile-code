#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "compileError.hpp"
#include "Visitor.hpp"
#include "front-end/Lexer.hpp"
#include "front-end/Parser.hpp"
#include "middle-end/generation/IRGenerator.hpp"
#include "middle-end/output/IRPrinter.hpp"
#include "middle-end/output/IRVerifier.hpp"

int main() {
    // 每次编译先清空输出，即使读取/前端处理失败也不会遗留旧 IR。
    std::ofstream("llvm_ir.txt", std::ios::trunc).close();
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

    /*---------------------中间代码生成阶段----------*/
    try {
        // Context 先创建、最后销毁，保证 Type 和 Constant 的指针一直有效。
        ir::IRContext context;
        ir::IRGenerator generator(context, vistor.getSemanticModel());
        ir::Module& module = generator.generate(ast);
        ir::IRVerifier().verify(module);
        // 先在内存中完成打印，成功后一次性写入，避免留下半份 IR。
        std::ostringstream irText;
        ir::IRPrinter().print(module, irText);
        std::ofstream irFile("llvm_ir.txt");
        if (!irFile.is_open()) return 1;
        irFile << irText.str();
    } catch (const std::exception& error) {
        std::cerr << "中间代码生成失败: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
