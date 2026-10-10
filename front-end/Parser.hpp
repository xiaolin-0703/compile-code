#pragma once

#include <vector>

#include "Token.hpp"
#include "AST/AST.hpp"

class Parser {

    public:
        explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {
            size = tokens.size();
        }

        CompUnit parse() {
            return parseCompUnit();
        }

    private:
        std::vector<Token> tokens;
        std::size_t cur_pos = 0;
        std::size_t size = tokens.size();

        Token& current() {
            return tokens[cur_pos];
        }

        Token& peek(std::size_t offset = 1) {
            std::size_t peek_pos = cur_pos + offset;
            if (peek_pos >= size) {
                return tokens.back();
            }
            return tokens[peek_pos];
        }

        Token& next(std::size_t offset = 1) {
            std::size_t next_pos = cur_pos + offset;
            if (next_pos >= size) {
                return tokens.back();
            }
            return tokens[next_pos];
        }

        void rewind(std::size_t count = 1) {
            if (count > cur_pos) {
                cur_pos = 0;
            } else {
                cur_pos -= count;
            }
        }

        CompUnit parseCompUnit() {
            std::vector<DeclPtr> declarations;
            std::vector<FuncDef> functions;
            MainFuncDef mainFunction(BlockStmt({ }, 0));

            while (cur_pos < size) {
                if (current().getType() == TokenType::INTTK || current().getType() == TokenType::CHARTK) {
                    // 解析变量声明
                    auto decl = parseDecl();
                    if (decl) {
                        declarations.push_back(std::move(decl));
                    }
                } else if (current().getType() == TokenType::VOIDTK || current().getType() == TokenType::INTTK || current().getType() == TokenType::CHARTK) {
                    // 解析函数定义
                    auto funcDef = parseFuncDef();
                    if (funcDef ) {
                        functions.push_back(std::move(*funcDef));
                    }
                } else if (current().getType() == TokenType::MAINTK) {
                    // 解析主函数定义
                    mainFunction = parseMainFuncDef();
                } else {
                    // 未知的语法结构，跳过当前 token
                    ++cur_pos;
                }
            }

            return CompUnit(std::move(declarations), std::move(functions), std::move(mainFunction));
        }

        FuncDef parseFuncDef() {
            // 解析函数定义的逻辑
            // 这里需要根据具体的语法规则实现函数定义的解析
            // 返回一个 FuncDef 对象
        }

        MainFuncDef parseMainFuncDef() {
            // 解析主函数定义的逻辑
            // 这里需要根据具体的语法规则实现主函数定义的解析
            // 返回一个 MainFuncDef 对象
        }

        DeclPtr parseDecl() {
            // 解析声明的逻辑
            // 这里需要根据具体的语法规则实现声明的解析
            // 返回一个 DeclPtr 对象
        }

        DeclPtr parseVarDecl() {
            // 解析变量声明的逻辑
            // 这里需要根据具体的语法规则实现变量声明的解析
            // 返回一个 DeclPtr 对象
        }

        StmtPtr parseStmt() {
            // 解析语句的逻辑
            // 这里需要根据具体的语法规则实现语句的解析
            // 返回一个 StmtPtr 对象
        }

        BlockItemPtr parseBlockItem() {
            // 解析块项的逻辑
            // 这里需要根据具体的语法规则实现块项的解析
            // 返回一个 BlockItemPtr 对象
        }

        ExprPtr parseExp() {
            // 解析表达式的逻辑
            // 这里需要根据具体的语法规则实现表达式的解析
            // 返回一个 ExprPtr 对象
        }

        ExprPtr parseAddExp() {
            // 解析加法表达式的逻辑
            // 这里需要根据具体的语法规则实现加法表达式的解析
            // 返回一个 ExprPtr 对象
        }

        ExprPtr parseMulExp() {
            // 解析乘法表达式的逻辑
            // 这里需要根据具体的语法规则实现乘法表达式的解析
            // 返回一个 ExprPtr 对象
        }

        ExprPtr parseUnaryExp() {
            // 解析一元表达式的逻辑
            // 这里需要根据具体的语法规则实现一元表达式的解析
            // 返回一个 ExprPtr 对象
        }

        ExprPtr parsePrimaryExp() {
            // 解析基本表达式的逻辑
            // 这里需要根据具体的语法规则实现基本表达式的解析
            // 返回一个 ExprPtr 对象
        }



    
        

};