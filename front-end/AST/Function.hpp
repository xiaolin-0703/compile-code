#pragma once

#include <utility>
#include <vector>

#include "ASTNode.hpp"
#include "Ident.hpp"
#include "Stmt.hpp"
#include "Type.hpp"

class FuncParam {
public:
    BasicType type;
    Ident identifier;
    bool isArray;

    FuncParam(
        BasicType type,
        Ident identifier,
        bool isArray
    )
        : type(type),
          identifier(std::move(identifier)),
          isArray(isArray) {}
};


class FuncDef : public ASTNode {
public:
    BasicType returnType;
    Ident identifier;
    std::vector<FuncParam> parameters;
    BlockStmt body;

    FuncDef(
        BasicType returnType,
        Ident identifier,
        std::vector<FuncParam> parameters,
        BlockStmt body
    )
        : returnType(returnType),
          identifier(std::move(identifier)),
          parameters(std::move(parameters)),
          body(std::move(body)) {}
};

class MainFuncDef : public ASTNode {
public:
    BlockStmt body;

    explicit MainFuncDef(BlockStmt body)
        : body(std::move(body)) {}
};

