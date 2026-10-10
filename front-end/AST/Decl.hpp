#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "Expr.hpp"
#include "../Token.hpp"

class Decl : public BlockItem {
public:
    ~Decl() override = default;
};

using DeclPtr = std::unique_ptr<Decl>;

using InitValue = std::variant<//只能定义为其中一种
    ExprPtr,
    std::vector<ExprPtr>,
    Token
>;

class Init {
public:
    explicit Init(ExprPtr expression)
        : value(std::move(expression)) {}

    explicit Init(std::vector<ExprPtr> list)
        : value(std::move(list)) {}

    explicit Init(Token stringLiteral)
        : value(std::move(stringLiteral)) {}

    InitValue value;
};


class VariableDef {
public:
    Ident ident;
    ExprPtr arraySize;
    std::optional<Init> initValue;

    VariableDef(
        Ident ident,
        ExprPtr arraySize = nullptr,
        std::optional<Init> initValue = std::nullopt
    )
        : ident(std::move(ident)),
          arraySize(std::move(arraySize)),
          initValue(std::move(initValue)) {}
};

class VarDecl final : public Decl;
class ConstDecl final : public Decl;


class VarDecl final : public Decl {
public:
    BasicType type;
    bool isStatic;
    std::vector<VariableDef> varDefs;

    VarDecl(BasicType type, bool isStatic, std::vector<VariableDef> varDefs)
        : type(type), isStatic(isStatic), varDefs(std::move(varDefs)) {}
};

class ConstDecl : public Decl {
public:
    BasicType type;
    std::vector<VariableDef> constDefs;

    ConstDecl(BasicType type, std::vector<VariableDef> constDefs)
        : type(type), constDefs(std::move(constDefs)) {}
};

