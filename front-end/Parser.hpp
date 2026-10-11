#pragma once

#include <iostream>
#include <stdexcept>
#include <vector>

#include "Token.hpp"
#include "AST/AST.hpp"

class Parser {

    public:
        explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {
            token_size = tokens.size();
        }

        CompUnit parse() {
            return parseCompUnit();
        }

    private:
        std::vector<Token> tokens;
        std::size_t cur_pos = 0;
        std::size_t token_size = tokens.size();

        Token& current() {
            return tokens[cur_pos];
        }

        bool match(TokenType expectedType) {
            if (cur_pos >= token_size) {
                return false;
            }
            if (current().getType() != expectedType) {
                return false;
            }
            ++cur_pos; // 匹配成功，同时消费当前 token
            return true;
        }   

        bool check(TokenType expectedType) const {
            return cur_pos < token_size &&
                tokens[cur_pos].getType() == expectedType;
        }

        bool expect(TokenType expectedType) {
            if (match(expectedType)) {
                return true;
            }

            std::cerr
                << "第 " << current().getLineNum()
                << " 行：期望 "
                << tokenTypeToString(expectedType)
                << "，实际得到 "
                << tokenTypeToString(current().getType())
                << '\n';

            return false;
        }

        Token& peek(std::size_t offset = 1) {
            std::size_t peek_pos = cur_pos + offset;
            if (peek_pos >= token_size) {
                return tokens.back();
            }
            return tokens[peek_pos];
        }

        void next(std::size_t offset = 1) {
             if (cur_pos < token_size) {
                ++cur_pos;
            }
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

            while (cur_pos < token_size) {
                if (current().getType() == TokenType::CONSTTK || current().getType() == TokenType::INTTK ||
                 current().getType() == TokenType::CHARTK || current().getType() == TokenType::STATICTK) {
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

        //定义区
        DeclPtr parseDecl() {
            if (match(TokenType::CONSTTK)) {
                return parseConstDecl();
            }
            return parseVarDecl();
        }


        //常量定义
        ConstDef parseConstDef() {
            
            if (!check(TokenType::IDENFR)) {
                throw std::runtime_error("Expected identifier for constant definition.");
            }
            Ident ident(current());
            next(); // 消费标识符 token

            ExprPtr arraySize = nullptr;
            if (match(TokenType::LBRACK)) {
                arraySize = parseConstExp();
                expect(TokenType::RBRACK);
            }
           
            expect(TokenType::ASSIGN);
            Init InitValue = parseConstInitval();
            return ConstDef(std::move(ident), std::move(arraySize), std::move(InitValue));
        }

        Init parseConstInitval() {
            if (check(TokenType::STRCON)) {
                Token stringLiteral = current();
                next();
                return Init(std::move(stringLiteral));
            }

            if (match(TokenType::LBRACE)) {
                std::vector<ExprPtr> elements;
                if (!check(TokenType::RBRACE)) {
                    elements.push_back(parseConstExp());
                    while (match(TokenType::COMMA)) {
                        elements.push_back(parseConstExp());
                    }
                }
                expect(TokenType::RBRACE);
                return Init(std::move(elements));
            }

            ExprPtr expression = parseConstExp();
            return Init(std::move(expression));
        }

        //常量表达式
        ExprPtr parseConstExp() {
            return parseAddExp(); // 假设常量表达式可以是加法表达式
        }

        DeclPtr parseConstDecl() {
            BType type = parseBType();
            std::vector<ConstDef> definitions;
            definitions.push_back(parseConstDef());

            while (match(TokenType::COMMA)) {
                definitions.push_back(parseConstDef());
            }

            expect(TokenType::SEMICN);

            return std::make_unique<ConstDecl>(type, std::move(definitions));
        }

        BType parseBType() {
            if (current().getType() == TokenType::INTTK) {
                next();
                return BType::Int;
            } else if (current().getType() == TokenType::CHARTK) {
                next();
                return BType::Char;
            } else { 
                throw std::runtime_error("不是Btype！！");// 处理错误情况，返回默认类型或抛出异常
                return BType::Int; // 默认返回 Int 类型
            }
        }
     
        DeclPtr parseVarDecl() {
            //变量声明 VarDecl → [ 'static' ] BType VarDef { ',' VarDef } ';'
            bool isStatic = false;
            if (match(TokenType::STATICTK)) {
                isStatic = true;
            }
            BType type = parseBType();
            std::vector<VarDef> definitions;
            definitions.push_back(parseVarDef());
            while(match(TokenType::COMMA)) {
                definitions.push_back(parseVarDef());
            }
            expect(TokenType::SEMICN);
            
            return std::make_unique<VarDecl>(type,
                isStatic,
                std::move(definitions));
        }

        VarDef parseVarDef() {
            //变量定义 VarDef → Ident [ '[' ConstExp ']' ] | Ident [ '[' ConstExp ']' ]
           // '=' InitVal // 包含普通常量、⼀维数组定义
            if (!check(TokenType::IDENFR)) {
                throw std::runtime_error("Expected identifier for variable definition.");
            }

            Ident ident(current());
            next();
            ExprPtr arraySize = nullptr;
            if (match(TokenType::LBRACK)) {
                arraySize = parseConstExp();
                expect(TokenType::RBRACK);
            }
            std::optional<Init> initVar = std::nullopt;

            if (match(TokenType::ASSIGN)) {
                 initVar = parseInitVal();
            }
            return VarDef(
            std::move(ident),
            std::move(arraySize),
            std::move(initVar)
             );
        }

        Init parseInitVal() {
            //变量初值 InitVal → Exp | '{' [ Exp { ',' Exp } ] '}' | StringConst // 1.表达
//式初值 2.⼀维数组初值 3. 字符串初值
            if (check(TokenType::STRCON)) {
                Token stringLiteral = current();
                next();
                return Init(std::move(stringLiteral));
            }
            if (match(TokenType::LBRACE)) {
                std::vector<ExprPtr> elements;
                if (!check(TokenType::RBRACE)) {
                    elements.push_back(parseExp());
                    while (match(TokenType::COMMA)) {
                        elements.push_back(parseExp());
                    }
                }
                expect(TokenType::RBRACE);
                return Init(std::move(elements));
            }
            ExprPtr expression = parseExp();
            return Init(std::move(expression));
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

        // 解析加法表达式
        ExprPtr parseAddExp() { 
            ExprPtr left = parseMulExp();
            while (check(TokenType::PLUS) || check(TokenType::MINU)) {
                Token op = current();
                next(); 
                ExprPtr right = parseMulExp();
                left = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
            }
            return left;
        }

        ExprPtr parseMulExp() {
            // 解析乘法表达式的逻辑
            // 这里需要根据具体的语法规则实现乘法表达式的解析
            // 返回一个 ExprPtr 对象
            ExprPtr left = parseUnaryExp();
            while (check(TokenType::MULT) || check(TokenType::DIV) || check(TokenType::MOD)) {
                Token op = current();
                next(); 
                ExprPtr right = parseUnaryExp();
                left = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
            }
            return left;
        }

        ExprPtr parseUnaryExp() {
            // 解析一元表达式的逻辑
            // 这里需要根据具体的语法规则实现一元表达式的解析
            // 返回一个 ExprPtr 对象
            if (check(TokenType::PLUS) || check(TokenType::MINU) || check(TokenType::NOT)) {
                Token op = current();
                next();
                ExprPtr expr = parseUnaryExp();
                if (!expr) {
                    throw std::runtime_error("Expected expression after unary operator.");
                }
                return std::make_unique<UnaryExpr>(std::move(op), std::move(expr));
            }

            if (check(TokenType::IDENFR) && peek().getType() == TokenType::LPARENT) {
                return parseCallExpr();
            } 
            
            if (check(TokenType::LPARENT) &&
                (peek().getType() == TokenType::INTTK ||
                peek().getType() == TokenType::CHARTK)) {
                return parseCastExpr();
            }
            return parsePrimaryExp();
        }

        //解析函数调用表达式
        ExprPtr parseCallExpr() {
            Ident functionName = Ident(current());
            next();
            if (!match(TokenType::LPARENT)) {
                throw std::runtime_error("Expected '(' after function name.");
            }
            std::vector<ExprPtr> arguments;
            if (!check(TokenType::RPARENT)) {
                arguments.push_back(parseExp());
                while (check(TokenType::COMMA)) {
                    next();
                    arguments.push_back(parseExp());
                }
            }
            if (!match(TokenType::RPARENT)) {//j类错误
                throw std::runtime_error("Expected ')' after function arguments.");
            }
            return std::make_unique<CallExpr>(std::move(functionName), std::move(arguments));
        }

        //解析类型转换表达式
        ExprPtr parseCastExpr() {
           
            if (!match(TokenType::LPARENT)) {
                throw std::runtime_error("缺少左括号！！");
            }
            
            BType targetType;
            if (match(TokenType::INTTK)) {
                targetType = BType::Int;
            } else if(match(TokenType::CHARTK)) {
                targetType = BType::Char;
            } else {
                throw std::runtime_error("不是BType");
            }

            expect(TokenType::RPARENT);

            ExprPtr operand = parseUnaryExp();

            return std::make_unique<CastExpr>(targetType,operand);

        
        }

        ExprPtr parsePrimaryExp() {
            // 解析基本表达式的逻辑
            // 这里需要根据具体的语法规则实现基本表达式的解析
            // 返回一个 ExprPtr 对象
            if (match(TokenType::LPARENT)) {
                ExprPtr expr = parseExp();
                expect(TokenType::RPARENT);
                return expr;
            }

            if(check(TokenType::IDENFR)) {
                return parseLVal();
            }

            if (check(TokenType::INTCON)) {
                Token literal = current();
                next();
                return std::make_unique<LiteralExpr>(
                std::move(literal));
            }
            if (check(TokenType::CHARCON)) {
                Token literal = current();
                next();

                return std::make_unique<LiteralExpr>(
                    std::move(literal));
            }
            throw std::runtime_error("不是primaryExp");
            return nullptr;
        }

        //普通表达式其实就是加法表达式
        ExprPtr parseExp() {
            return parseAddExp();
        }

        ExprPtr parseLVal() {
            if (!check(TokenType::IDENFR)) {
                throw std::runtime_error("不是Lval！！");
            }

            Ident ident(current());
            next();

            ExprPtr index = nullptr;

            if(match(TokenType::LBRACK)) {
                index = parseExp();
                expect(TokenType::RBRACK);

            }

            return std::make_unique<LValExpr>(
                std::move(ident),
                std::move(index));
        }
    
        

};