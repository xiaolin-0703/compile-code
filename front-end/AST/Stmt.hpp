#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "ASTNode.hpp"
#include "Expr.hpp"
#include "../Token.hpp"

class Stmt : public BlockItem {
public:
    ~Stmt() override = default;
};

using StmtPtr = std::unique_ptr<Stmt>;

//块语句
class BlockStmt : public Stmt {
public:
    std::vector<BlockItemPtr> blockItems;
    int closingBraceLine;
    explicit BlockStmt(std::vector<BlockItemPtr> blockItems, int closingBraceLine)
        : blockItems(std::move(blockItems)), closingBraceLine(closingBraceLine) {}
};

//赋值语句
class AssignStmt : public Stmt {
public:
    LValExpr target;
    ExprPtr value;

    AssignStmt(LValExpr target, ExprPtr value)
        : target(std::move(target)), value(std::move(value)) {}
};

//表达式语句
class ExprStmt : public Stmt {
public: 
    ExprPtr expression;

    explicit ExprStmt(ExprPtr expression)
        : expression(std::move(expression)) {}
};

//if语句
class IfStmt : public Stmt {
public:
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;

    IfStmt(ExprPtr condition, StmtPtr thenBranch, StmtPtr elseBranch = nullptr)
        : condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}

};

//while语句
class WhileStmt : public Stmt {
public:
    ExprPtr condition;
    StmtPtr body;

    WhileStmt(ExprPtr condition, StmtPtr body)
        : condition(std::move(condition)), body(std::move(body)) {}
};

//break

class BreakStmt : public Stmt {
public:
    Token breakToken;

    explicit BreakStmt(Token breakToken)
        : breakToken(std::move(breakToken)) {}
};

//continue
class ContinueStmt : public Stmt {
public:
    Token continueToken;

    explicit ContinueStmt(Token continueToken)
        : continueToken(std::move(continueToken)) {}
};

//return

class ReturnStmt : public Stmt {
public:
    Token returnToken;
    ExprPtr returnValue;

    ReturnStmt(Token returnToken, ExprPtr returnValue = nullptr)
        : returnToken(std::move(returnToken)), returnValue(std::move(returnValue)) {}
};

//printf
class PrintfStmt : public Stmt {
public:
    Token printfToken;
    Token formatString;
    std::vector<ExprPtr> arguments;

    PrintfStmt(Token printfToken, Token formatString, std::vector<ExprPtr> arguments)
        : printfToken(std::move(printfToken)), formatString(std::move(formatString)), arguments(std::move(arguments)) {}
};

//switch and case
class CaseStmt : public ASTNode {
public:
    Token caseToken;

    std::optional<Token> value;//case对应的值
    std::vector<StmtPtr> statements;//case条件成立时的代码

    CaseStmt(
        Token caseToken,
        std::optional<Token> value,
        std::vector<StmtPtr> statements
    )
        : caseToken(std::move(caseToken)),
          value(std::move(value)),
          statements(std::move(statements)) {}

    bool isDefault() const {
        return !value.has_value();
    }

};

//switch

class SwitchStmt : public Stmt {
public:
    ExprPtr switchExpr;
    std::vector<CaseStmt> cases;

    SwitchStmt(ExprPtr switchExpr, std::vector<CaseStmt> cases)
        :  switchExpr(std::move(switchExpr)), cases(std::move(cases)) {}
};

