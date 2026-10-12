#pragma once

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../compileError.hpp"
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

        const std::vector<compileError>& getErrors() const {
            return errors;
        }

        const std::vector<std::string>& getOutput() const {
            return output;
        }

    private:
        std::vector<Token> tokens;
        std::size_t cur_pos = 0;
        std::size_t token_size = tokens.size();
        std::vector<compileError> errors;
        std::vector<std::string> output;

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
            next(); // 匹配成功，同时消费并记录当前 token
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

            char errorType = '\0';
            if (expectedType == TokenType::SEMICN) {
                errorType = 'i';
            } else if (expectedType == TokenType::RPARENT) {
                errorType = 'j';
            } else if (expectedType == TokenType::RBRACK) {
                errorType = 'k';
            }

            if (errorType != '\0') {
                errors.emplace_back(previousLine(), errorType);
                return false; // 不消费当前 token，让上层继续解析
            }

            std::ostringstream message;
            message << "line " << current().getLineNum()
                    << ": expected " << tokenTypeToString(expectedType)
                    << ", got " << tokenTypeToString(current().getType());
            throw std::runtime_error(message.str());
        }

        Token& peek(std::size_t offset = 1) {
            std::size_t peek_pos = cur_pos + offset;
            if (peek_pos >= token_size) {
                return tokens.back();
            }
            return tokens[peek_pos];
        }

        void next() {
            if (cur_pos < token_size) {
                if (!check(TokenType::END_OF_FILE)) {
                    output.emplace_back(
                        std::string(tokenTypeToString(current().getType())) +
                        " " + current().getWord());
                }
                ++cur_pos;
            }
        }

        int previousLine() const {
            if (cur_pos == 0 || tokens.empty()) {
                return tokens.empty() ? 1 : tokens.front().getLineNum();
            }
            return tokens[cur_pos - 1].getLineNum();
        }

        void mark(const char* nonTerminal) {
            output.emplace_back("<" + std::string(nonTerminal) + ">");
        }

        bool isMainFunctionStart() {
            return check(TokenType::INTTK) &&
                   peek().getType() == TokenType::MAINTK;
        }

        bool isFunctionDefinitionStart() {
            return check(TokenType::VOIDTK) ||
                   ((check(TokenType::INTTK) || check(TokenType::CHARTK)) &&
                    peek().getType() == TokenType::IDENFR &&
                    peek(2).getType() == TokenType::LPARENT);
        }

        bool isDeclarationStart() {
            if (check(TokenType::CONSTTK) || check(TokenType::STATICTK)) {
                return true;
            }
            return (check(TokenType::INTTK) || check(TokenType::CHARTK)) &&
                   !isMainFunctionStart() &&
                   !isFunctionDefinitionStart();
        }

        CompUnit parseCompUnit() {
            std::vector<DeclPtr> declarations;
            std::vector<FuncDef> functions;
            MainFuncDef mainFunction(BlockStmt({ }, 0));
            while (isDeclarationStart()) {
                declarations.push_back(parseDecl());
            }
            while (isFunctionDefinitionStart()) {
                functions.push_back(parseFuncDef());
            }
            if (!isMainFunctionStart()) {
                throw std::runtime_error("missing main function");
            }
            mainFunction = parseMainFuncDef();
            if (!check(TokenType::END_OF_FILE)) {
                throw std::runtime_error("unexpected token after main function");
            }
            mark("CompUnit");
            return CompUnit(std::move(declarations), std::move(functions), std::move(mainFunction));
        }

        FuncDef parseFuncDef() {
            // 解析函数定义的逻辑
            // 这里需要根据具体的语法规则实现函数定义的解析
            // 返回一个 FuncDef 对象
            //函数定义 FuncDef → FuncType Ident '(' [FuncFParams] ')' Block
            FuncType funType = parseFunType();
            if (!check(TokenType::IDENFR)) {
                throw std::runtime_error("不是ident!!");
            }
            Ident ident = parseIdent();
            std::vector<FuncParam> func_params;
            expect(TokenType::LPARENT);
            if (check(TokenType::INTTK) || check(TokenType::CHARTK)) {
                func_params = parseFuncParams();
            }
            expect(TokenType::RPARENT);
            BlockStmt body = parseBlock();
            FuncDef result(
                funType,
                std::move(ident),
                std::move(func_params),
                std::move(body));
            mark("FuncDef");
            return result;
        }

        BlockStmt parseBlock() {
            //语句块 Block → '{' { BlockItem } '}'
            expect(TokenType::LBRACE);
            std::vector<BlockItemPtr> blockItems;
            while (cur_pos < token_size && !check(TokenType::RBRACE)
                && !check(TokenType::END_OF_FILE)) {
                blockItems.push_back(parseBlockItem());
            }
            int closingBraceLine = current().getLineNum();
            expect(TokenType::RBRACE);
            BlockStmt result(std::move(blockItems),closingBraceLine);
            mark("Block");
            return result;
        }

        std::vector<FuncParam> parseFuncParams() {
            // 函数形参表 FuncFParams → FuncFParam { ',' FuncFParam } // 1.花括号内重复0次 2.
            // 花括号内重复多次(花括号内重复>=2次，即，⾄少出现3次FuncFParam)
            // 函数形参 FuncFParam → BType Ident ['[' ']']
            std::vector<FuncParam> params;
            params.push_back(parseFuncParam());
            while (match(TokenType::COMMA)) {
                params.push_back(parseFuncParam());
            }
            mark("FuncFParams");
            return params;
        }

        FuncParam parseFuncParam() {
            // 函数形参 FuncFParam → BType Ident ['[' ']']
            BType type = parseBType();
            Ident ident = parseIdent();
            bool isArray = false;
            if (match(TokenType::LBRACK)) {
                isArray = true;
                expect(TokenType::RBRACK);
            }
            FuncParam result(
                type,std::move(ident),isArray);
            mark("FuncFParam");
            return result;
        }

        Ident parseIdent() {
            if (!check(TokenType::IDENFR)) {
                throw std::runtime_error("Expected identifier for function definition.");
            }
            Ident ident(current());
            next();
            return ident;
        }

        FuncType parseFunType() {
            FuncType result;
            if (match(TokenType::INTTK)) {
                result = FuncType::Int;
            } else if (match(TokenType::CHARTK)) {
                result = FuncType::Char;
            } else if (match(TokenType::VOIDTK)) {
                result = FuncType::Void;
            } else {
                throw std::runtime_error("不是FunType！！");
            }
            mark("FuncType");
            return result;
        }

        MainFuncDef parseMainFuncDef() {
            // 解析主函数定义的逻辑
            // 这里需要根据具体的语法规则实现主函数定义的解析
            // 返回一个 MainFuncDef 对象
            //主函数定义 MainFuncDef → 'int' 'main' '(' ')' Block
            expect(TokenType::INTTK);
            expect(TokenType::MAINTK);
            expect(TokenType::LPARENT);
            expect(TokenType::RPARENT);
            BlockStmt body = parseBlock();
            MainFuncDef result(std::move(body));
            mark("MainFuncDef");
            return result;
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
            ConstDef result(std::move(ident), std::move(arraySize), std::move(InitValue));
            mark("ConstDef");
            return result;
        }

        Init parseConstInitval() {
            if (check(TokenType::STRCON)) {
                Token stringLiteral = current();
                next();
                Init result(std::move(stringLiteral));
                mark("ConstInitVal");
                return result;
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
                Init result(std::move(elements));
                mark("ConstInitVal");
                return result;
            }

            ExprPtr expression = parseConstExp();
            Init result(std::move(expression));
            mark("ConstInitVal");
            return result;
        }

        //常量表达式
        ExprPtr parseConstExp() {
            ExprPtr result = parseAddExp();
            mark("ConstExp");
            return result;
        }

        DeclPtr parseConstDecl() {
            BType type = parseBType();
            std::vector<ConstDef> definitions;
            definitions.push_back(parseConstDef());

            while (match(TokenType::COMMA)) {
                definitions.push_back(parseConstDef());
            }

            expect(TokenType::SEMICN);

            auto result = std::make_unique<ConstDecl>(type, std::move(definitions));
            mark("ConstDecl");
            return result;
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
            
            auto result = std::make_unique<VarDecl>(type,
                isStatic,
                std::move(definitions));
            mark("VarDecl");
            return result;
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
            VarDef result(
            std::move(ident),
            std::move(arraySize),
            std::move(initVar)
             );
            mark("VarDef");
            return result;
        }

        Init parseInitVal() {
            //变量初值 InitVal → Exp | '{' [ Exp { ',' Exp } ] '}' | StringConst // 1.表达
//式初值 2.⼀维数组初值 3. 字符串初值
            if (check(TokenType::STRCON)) {
                Token stringLiteral = current();
                next();
                Init result(std::move(stringLiteral));
                mark("InitVal");
                return result;
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
                Init result(std::move(elements));
                mark("InitVal");
                return result;
            }
            ExprPtr expression = parseExp();
            Init result(std::move(expression));
            mark("InitVal");
            return result;
        }

        StmtPtr parseStmt() {
            // 解析语句的逻辑
            // 这里需要根据具体的语法规则实现语句的解析
            // 返回一个 StmtPtr 对象
//             语句 Stmt → LVal '=' Exp ';' // 每种类型的语句都要覆盖
// | [Exp] ';' // 有⽆Exp两种情况；printf函数调⽤
// | Block
// | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] // 1.有else 2.⽆else
// | 'while' '(' Cond ')' Stmt // while循环语句
// | 'switch' '(' Exp ')' '{' { CaseStmt } '}' // switch语句，需覆盖不同case情况
// | 'break' ';'
// | 'continue' ';'
// | 'return' [Exp] ';' // 1.有Exp 2.⽆Exp
// | 'printf' '(' StringConst { ',' Exp } ')' ';' // 1.有Exp 2.⽆Exp
            StmtPtr result;
            if (check(TokenType::IFTK)) {
                result = parseIfStmt();
            } else if (check(TokenType::WHILETK)) {
                result = parseWhileStmt();
            } else if (check(TokenType::SWITCHTK)) {
                result = parseSwitchStmt();
            } else if (check(TokenType::BREAKTK)) {
                result = parseBreakStmt();
            } else if (check(TokenType::CONTINUETK)) {
                result = parseContinueStmt();
            } else if (check(TokenType::RETURNTK)) {
                result = parseReturnStmt();
            } else if (check(TokenType::PRINTFTK)) {
                result = parsePrintfStmt();
            } else if (check(TokenType::LBRACE)) {
                result = std::make_unique<BlockStmt>(parseBlock());
            } else if (isAssignmentStmt()) {
                result = parseAssignStmt();
            } else {
                result = parseExprStmt();
            }
            mark("Stmt");
            return result;
        }

        StmtPtr parseExprStmt() {
            //[Exp] ';'
            ExprPtr expression;
            if (canStartExpression()) {
                expression = parseExp();
            }
            expect(TokenType::SEMICN);
            return std::make_unique<ExprStmt>(std::move(expression));
        }

        StmtPtr parseAssignStmt() {
            //LVal '=' Exp ';'
            LValExprPtr target = parseLVal();
            expect(TokenType::ASSIGN);
            ExprPtr expression = parseExp();
            expect(TokenType::SEMICN);
            return std::make_unique<AssignStmt>(std::move(target),
                std::move(expression));
        }

        bool isAssignmentStmt() const {
            std::size_t pos = cur_pos;
            if (pos >= token_size ||
                tokens[pos].getType() != TokenType::IDENFR) {
                return false;
                }
            ++pos;
            if (pos < token_size &&
                tokens[pos].getType() == TokenType::LBRACK) {
                int bracketDepth = 1;
                ++pos;
                while (pos < token_size && bracketDepth > 0) {
                    TokenType type = tokens[pos].getType();

                    if (type == TokenType::LBRACK) {
                        ++bracketDepth;
                    } else if (type == TokenType::RBRACK) {
                        --bracketDepth;
                    } else if (type == TokenType::ASSIGN && bracketDepth == 1) {
                        // a[exp = ...：把它识别为缺少 ']' 的赋值语句，
                        // parseLVal() 会记录 k，并在 '=' 处继续解析。
                        return true;
                    }
                    ++pos;
                }
                if (bracketDepth != 0) {
                    return false;
                }
                }
            return pos < token_size &&
                   tokens[pos].getType() == TokenType::ASSIGN;
        }

        bool canStartExpression() const {
            return check(TokenType::IDENFR) ||
                   check(TokenType::LPARENT) ||
                   check(TokenType::INTCON) ||
                   check(TokenType::CHARCON) ||
                   check(TokenType::PLUS) ||
                   check(TokenType::MINU) ||
                   check(TokenType::NOT);
        }

        StmtPtr parsePrintfStmt() {
            // 'printf' '(' StringConst { ',' Exp } ')' ';'
            if (!check(TokenType::PRINTFTK)) {
                throw std::runtime_error("不是Printf!!!");
            }
            Token printfToken = current();
            next();
            expect(TokenType::LPARENT);
            if (!check(TokenType::STRCON)) {
                throw std::runtime_error("Expected string constant after 'printf('.");
            }
            Token strConst = current();
            next();
            std::vector<ExprPtr> arguments;
            while (match(TokenType::COMMA)) {
                arguments.push_back(parseExp());
            }
            expect(TokenType::RPARENT);
            expect(TokenType::SEMICN);
            return std::make_unique<PrintfStmt>(std::move(printfToken),
                std::move(strConst),
                std::move(arguments));
        }

        StmtPtr parseReturnStmt() {
            // | 'return' [Exp] ';'
            if (!check(TokenType::RETURNTK)) {
                throw std::runtime_error("不是Return!!");
            }
            Token returnToken = current();
            next();
            ExprPtr exp;
            if (canStartExpression()) {
                exp = parseExp();
            }
            expect(TokenType::SEMICN);
            return std::make_unique<ReturnStmt>(std::move(returnToken),std::move(exp));
        }

        StmtPtr parseContinueStmt() {
           // 'continue' ';'
            if (!check(TokenType::CONTINUETK)) {
                throw std::runtime_error("不是Continue!!");
            }
            Token token = current();
            next();
            expect(TokenType::SEMICN);
            return std::make_unique<ContinueStmt>(std::move(token));
        }

        StmtPtr parseBreakStmt() {
            //'break' ';'
            if (!check(TokenType::BREAKTK)) {
                throw std::runtime_error("不是Break!!!");
            }
            Token token = current();
            next();
            expect(TokenType::SEMICN);
            return std::make_unique<BreakStmt>(std::move(token));
        }

        StmtPtr parseSwitchStmt() {
            //'switch' '(' Exp ')' '{' { CaseStmt } '}'
            expect(TokenType::SWITCHTK);
            expect(TokenType::LPARENT);
            ExprPtr expr = parseExp();
            expect(TokenType::RPARENT);
            expect(TokenType::LBRACE);
            std::vector<CaseStmt> cases;
            while (check(TokenType::CASETK) || check(TokenType::DEFAULTTK)) {
                cases.push_back(parseCaseStmt());
            }
            expect(TokenType::RBRACE);
            return std::make_unique<SwitchStmt>(std::move(expr), std::move(cases));
        }

        CaseStmt parseCaseStmt() {
           // Case语句 CaseStmt → 'case' Number ':' { Stmt } | 'default' ':' { Stmt } //
           // 1.case语句 2.default语句
            Token lableToken = current();
            std::optional<Token> value = std::nullopt;
            if (check(TokenType::CASETK)) {
                next();
                if (!check(TokenType::INTCON) &&
                !check(TokenType::CHARCON)) {
                    throw std::runtime_error(
                        "Expected number after 'case'."
                    );
                }
                value = current();
                next();
                mark("Number");
                expect(TokenType::COLON);

             } else if (check(TokenType::DEFAULTTK)) {
                 next();
                 value = std::nullopt;
                 expect(TokenType::COLON);
             } else {
                 throw std::runtime_error("不是caseStmt!!");
             }
            std::vector<StmtPtr> statements;
            while (!check(TokenType::CASETK) &&
                !check(TokenType::DEFAULTTK) &&
                !check(TokenType::RBRACE) &&
                !check(TokenType::END_OF_FILE)) {
                    statements.push_back(parseStmt());
                }
            CaseStmt result(
            std::move(lableToken),
            std::move(value),
            std::move(statements)
            );
            mark("CaseStmt");
            return result;
        }

        StmtPtr parseWhileStmt() {
           // 'while' '(' Cond ')' Stmt // while循环语句
            expect(TokenType::WHILETK);
            expect(TokenType::LPARENT);
            ExprPtr condition = parseCond();
            expect(TokenType::RPARENT);
            StmtPtr body = parseStmt();
            return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
        }

        StmtPtr parseIfStmt() {
            expect(TokenType::IFTK);
            expect(TokenType::LPARENT);
            ExprPtr condition = parseCond();
            expect(TokenType::RPARENT);
            //'if' '(' Cond ')' Stmt [ 'else' Stmt ] // 1.有else 2.⽆else
            StmtPtr thenBranch = parseStmt();
            StmtPtr elseBranch;
            if (match(TokenType::ELSETK)) {
                elseBranch = parseStmt();
            }
            return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch),
                std::move(elseBranch));
        }

        ExprPtr parseCond() {
            ExprPtr result = parseLOrExp();
            mark("Cond");
            return result;
        }

    //逻辑或
        ExprPtr parseLOrExp() {
            //逻辑或表达式 LOrExp → LAndExp | LOrExp '||' LAndExp
            ExprPtr left = parseLAndExp();
            while (check(TokenType::OR)) {
                mark("LOrExp");
                Token op = current();
                next();
                ExprPtr right = parseLAndExp();
                left = std::make_unique<BinaryExpr>(
                    std::move(left),
                    std::move(op),
                    std::move(right)
                );
            }
            mark("LOrExp");
            return left;
        }

    //逻辑与
        ExprPtr parseLAndExp() {
            ExprPtr left = parseEqExp();
            while (check(TokenType::AND)) {
                mark("LAndExp");
                Token op = current();
                next();
                ExprPtr right = parseEqExp();
                left = std::make_unique<BinaryExpr>(
                    std::move(left),
                    std::move(op),
                    std::move(right)
                );
            }
            mark("LAndExp");
            return left;
        }
        //相等表达式
        ExprPtr parseEqExp() {
            ExprPtr left = parseRelExp();
            while (check(TokenType::EQL) ||
                   check(TokenType::NEQ)) {
                mark("EqExp");
                Token op = current();
                next();
                ExprPtr right = parseRelExp();
                left = std::make_unique<BinaryExpr>(
                    std::move(left),
                    std::move(op),
                    std::move(right)
                );
                   }
            mark("EqExp");
            return left;
        }
        //关系表达式
        ExprPtr parseRelExp() {
            ExprPtr left = parseAddExp();
            while (check(TokenType::LSS) ||
                   check(TokenType::LEQ) ||
                   check(TokenType::GRE) ||
                   check(TokenType::GEQ)) {
                mark("RelExp");
                Token op = current();
                next();
                ExprPtr right = parseAddExp();
                left = std::make_unique<BinaryExpr>(
                    std::move(left),
                    std::move(op),
                    std::move(right)
                );
                   }
            mark("RelExp");
            return left;
        }
    //解析一个项，是stmt还是decl
        BlockItemPtr parseBlockItem() {
            // 返回一个 BlockItemPtr 对象
            //语句块项 BlockItem → Decl | Stmt
            if (check(TokenType::CONSTTK) ||
                check(TokenType::STATICTK) ||
                check(TokenType::INTTK) ||
                check(TokenType::CHARTK)) {
                return parseDecl();
            }
            return parseStmt();
        }

        // 解析加法表达式
        ExprPtr parseAddExp() { 
            ExprPtr left = parseMulExp();
            while (check(TokenType::PLUS) || check(TokenType::MINU)) {
                mark("AddExp");
                Token op = current();
                next(); 
                ExprPtr right = parseMulExp();
                left = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
            }
            mark("AddExp");
            return left;
        }

        ExprPtr parseMulExp() {
            // 解析乘法表达式的逻辑
            // 这里需要根据具体的语法规则实现乘法表达式的解析
            // 返回一个 ExprPtr 对象
            ExprPtr left = parseUnaryExp();
            while (check(TokenType::MULT) || check(TokenType::DIV) || check(TokenType::MOD)) {
                mark("MulExp");
                Token op = current();
                next(); 
                ExprPtr right = parseUnaryExp();
                left = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
            }
            mark("MulExp");
            return left;
        }

        ExprPtr parseUnaryExp() {
            // 解析一元表达式的逻辑
            // 这里需要根据具体的语法规则实现一元表达式的解析
            // 返回一个 ExprPtr 对象
            ExprPtr result;
            if (check(TokenType::PLUS) || check(TokenType::MINU) || check(TokenType::NOT)) {
                Token op = current();
                next();
                mark("UnaryOp");
                ExprPtr expr = parseUnaryExp();
                if (!expr) {
                    throw std::runtime_error("Expected expression after unary operator.");
                }
                result = std::make_unique<UnaryExpr>(std::move(op), std::move(expr));
            } else if (check(TokenType::IDENFR) && peek().getType() == TokenType::LPARENT) {
                result = parseCallExpr();
            } else if (check(TokenType::LPARENT) &&
                       (peek().getType() == TokenType::INTTK ||
                        peek().getType() == TokenType::CHARTK)) {
                result = parseCastExpr();
            } else {
                result = parsePrimaryExp();
            }
            mark("UnaryExp");
            return result;
        }

        //解析函数调用表达式
        ExprPtr parseCallExpr() {
            Ident functionName = Ident(current());
            next();
            if (!match(TokenType::LPARENT)) {
                throw std::runtime_error("Expected '(' after function name.");
            }
            std::vector<ExprPtr> arguments;
            if (canStartExpression()) {
                arguments.push_back(parseExp());
                while (check(TokenType::COMMA)) {
                    next();
                    arguments.push_back(parseExp());
                }
                mark("FuncRParams");
            }
            expect(TokenType::RPARENT);
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
            return std::make_unique<CastExpr>(targetType,std::move(operand));
        }

        ExprPtr parsePrimaryExp() {
            // 解析基本表达式的逻辑
            // 这里需要根据具体的语法规则实现基本表达式的解析
            // 返回一个 ExprPtr 对象
            if (match(TokenType::LPARENT)) {
                ExprPtr expr = parseExp();
                expect(TokenType::RPARENT);
                mark("PrimaryExp");
                return expr;
            }
            if(check(TokenType::IDENFR)) {
                ExprPtr result = parseLVal();
                mark("PrimaryExp");
                return result;
            }
            if (check(TokenType::INTCON)) {
                Token literal = current();
                next();
                mark("Number");
                auto result = std::make_unique<LiteralExpr>(std::move(literal));
                mark("PrimaryExp");
                return result;
            }
            if (check(TokenType::CHARCON)) {
                Token literal = current();
                next();
                mark("Number");
                auto result = std::make_unique<LiteralExpr>(std::move(literal));
                mark("PrimaryExp");
                return result;
            }
            throw std::runtime_error("不是primaryExp");
            return nullptr;
        }

        //普通表达式其实就是加法表达式
        ExprPtr parseExp() {
            ExprPtr result = parseAddExp();
            mark("Exp");
            return result;
        }

        LValExprPtr parseLVal() {
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
            auto result = std::make_unique<LValExpr>(
                std::move(ident),
                std::move(index));
            mark("LVal");
            return result;
        }
        

};
