#pragma once

#include "Symbol.hpp"
#include "../AST/Expr.hpp"
#include <unordered_map>

// using 为类型起别名：SymbolId 是声明的稳定编号，不是变量名字。
using SymbolId = std::size_t;

// 语义分析的持久化结果。AST 在分析和代码生成期间必须保持存活且不移动。
class SemanticModel {
private:
    std::vector<Symbol> records;
    std::unordered_map<const Ident*, SymbolId> declarations;
    std::unordered_map<const Expr*, SymbolId> references;
    std::unordered_map<const Expr*, ExprType> expressionTypes;

public:
    SymbolId addSymbol(Symbol& symbol, const Ident* declaration = nullptr) {
        symbol.id = records.size() + 1;
        records.push_back(symbol);
        if (declaration != nullptr) declarations[declaration] = symbol.id;
        return symbol.id;
    }

    void bind(const Expr& expr, SymbolId id) { references[&expr] = id; }
    void setType(const Expr& expr, ExprType type) { expressionTypes[&expr] = type; }
    SymbolId getDeclaration(const Ident& ident) const { return declarations.at(&ident); }
    SymbolId getReference(const Expr& expr) const { return references.at(&expr); }
    ExprType getType(const Expr& expr) const { return expressionTypes.at(&expr); }
    const Symbol& getSymbol(SymbolId id) const { return records.at(id - 1); }
};
