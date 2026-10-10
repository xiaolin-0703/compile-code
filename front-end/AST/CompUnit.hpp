#pragma once

#include <utility>
#include <vector>

#include "ASTNode.hpp"
#include "Decl.hpp"
#include "Function.hpp"

class CompUnit : public ASTNode {
public:
    std::vector<DeclPtr> declarations;
    std::vector<FuncDef> functions;
    MainFuncDef mainFunction;

    CompUnit(
        std::vector<DeclPtr> declarations,
        std::vector<FuncDef> functions,
        MainFuncDef mainFunction
    )
        : declarations(std::move(declarations)),
          functions(std::move(functions)),
          mainFunction(std::move(mainFunction)) {}
};