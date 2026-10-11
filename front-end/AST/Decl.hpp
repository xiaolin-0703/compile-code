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


class VarDef {
public:
    Ident ident;
    ExprPtr arraySize;
    std::optional<Init> initValue;
    VarDef(
        Ident ident,
        ExprPtr arraySize = nullptr,
        std::optional<Init> initValue = std::nullopt
    )
        : ident(std::move(ident)),
          arraySize(std::move(arraySize)),
          initValue(std::move(initValue)) {}
};

class ConstDef {
public:    
    Ident ident;
    ExprPtr arraySize;
    Init initValue; // 常量必须有初值
    ConstDef(
        Ident ident,
        ExprPtr arraySize,
        Init initValue
    )
        : ident(std::move(ident)),
          arraySize(std::move(arraySize)),
          initValue(std::move(initValue)) {}
};

class VarDecl final : public Decl {
public:    
    BType type;
    bool isStatic;
    std::vector<VarDef> definitions;

    VarDecl(BType type,bool isStatic,std::vector<VarDef> definitions)
    : type(type),isStatic(isStatic),
    definitions(std::move(definitions))
    {}
    
};

class ConstDecl final : public Decl {
public:
    BType type;
    std::vector<ConstDef> definitions;

    ConstDecl(
        BType type,
        std::vector<ConstDef> definitions
    )
        : type(type),
          definitions(std::move(definitions)) {}
};

