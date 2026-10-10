#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "ASTNode.hpp"
#include "Ident.hpp"
#include "Type.hpp"
#include "../Token.hpp"

class Expr : public ASTNode {
public:
    ~Expr() override = default;
};

using ExprPtr = std::unique_ptr<Expr>;


class LiteralExpr final : public Expr {
public:
    explicit LiteralExpr(Token token)
        : literal(std::move(token)) {}

    const Token& getLiteral() const {
        return literal;
    }
private:
    Token literal;
};



class LValExpr final : public Expr {
public:
    Ident ident;
    ExprPtr index;

    explicit LValExpr(Ident ident,ExprPtr index = nullptr)
        : ident(std::move(ident)), index(std::move(index)) {}

    const Ident& getIdent() const {
        return ident;
    }

    const ExprPtr& getIndex() const {
        return index;
    }
};

class UnaryExpr final : public Expr {
public:
    Token op;
    ExprPtr operand;

    UnaryExpr(Token op, ExprPtr operand)
        : op(std::move(op)), operand(std::move(operand)) {}
};

class BinaryExpr final : public Expr {
public:
    ExprPtr left;
    Token op;
    ExprPtr right;

    BinaryExpr(ExprPtr left, Token op, ExprPtr right)
        : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
};

class CallExpr final : public Expr {
public:
    Ident functionName;
    std::vector<ExprPtr> arguments;

    CallExpr(Ident functionName, std::vector<ExprPtr> arguments)
        : functionName(std::move(functionName)), arguments(std::move(arguments)) {}
};

class CastExpr final : public Expr {
public:
    BasicType targetType;
    ExprPtr expr;

    CastExpr(BasicType targetType, ExprPtr expr)
        : targetType(targetType), expr(std::move(expr)) {}
};

