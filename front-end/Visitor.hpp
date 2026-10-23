#pragma once
#include <vector>
#include "./AST/AST.hpp"
#include "../compileError.hpp"
#include "semantic/SymbolManager.hpp"

class Vistor {
public:
       explicit Vistor(CompUnit& ast) {
              analyze(ast);
       }

       const std::vector<compileError> getErrors() {
              return errors;
       }

       const SymbolManager& getSymbolManager() {
              return symbols;
       }

private:
       SymbolManager symbols;
       std::vector<compileError> errors;

       //添加一个符号
       void addSymbol(Symbol symbol) {
              if (!symbols.declare(symbol)) {
                     errors.emplace_back(symbol.ident.getLineNum(),'b');
              }
       }

       const Symbol* resolveSymbol(const Ident& ident) {
              const Symbol* symbol = symbols.lookup(ident.getName());

              if (symbol == nullptr) {
                     errors.emplace_back(ident.getLineNum(), 'c');
              }

              return symbol;
       }

       void analyze(const CompUnit& ast) {
              for (const auto& declaration:ast.declarations) {
                     VisitDecl(*declaration);
              }
              for (const auto& function : ast.functions) {
                     vistitFun(function);
              }
              visitMain(ast.mainFunction);
       }

       void VisitDecl (const Decl& decl) {
              const auto* dec1 = dynamic_cast<const VarDecl*>(&decl);

              if (dec1 != nullptr) {
                     visitVarDecl(*dec1);
              } else {
                     visitConDecl(*dec1);
              }
       }

       //解析变量声明
       void visitVarDecl (const VarDecl& decl) {
              bool isStatic = decl.isStatic;
              BType type = decl.type;
              for (const auto& def:decl.definitions) {
                     visitVarDef(def,isStatic,type);
              }
       }

       void visitVarDef (const VarDef& def,bool isStatic,BType type) {
              SymbolKind kind = def.arraySize ? SymbolKind::Array:SymbolKind::Variable;
              Symbol symbol(def.ident,kind,type,false,isStatic);
              addSymbol(std::move(symbol));

              if (def.arraySize) {
                     visitExpr(*def.arraySize);
              }

              if (def.initValue) {
                     visitInit(*def.initValue,type,kind == SymbolKind::Array);

              }

       }

       ExprInfo visitExpr (const Expr& expr) {
             if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expr) ) {
                    return visitLiteralExpr(*literal);
             }
             if (const auto* lval = dynamic_cast<const LValExpr*>(&expr)) {
                    return visitLValExpr(*lval);
             }

       }

       ExprInfo visitLValExpr (const LValExpr& expr) {
              const Ident idnet1 = expr.getIdent();
              const int lineNum = idnet1.getLineNum();

              const Symbol* symbol1 = resolveSymbol(idnet1);
              const auto& index = expr.getIndex();

              ExprInfo indexInfo{ExprType::Error,lineNum};

              if (index) {
                     indexInfo = visitExpr(*index);
              }

              if (symbol1 == nullptr) {
                     return {ExprType::Error,lineNum};
              }

              if (symbol1->isFunction()) {
                     errors.emplace_back(lineNum,'e');
                     return {ExprType::Error,lineNum};
              }

              if (index) {
                     if (!symbol1->isArray()) {
                            errors.emplace_back(lineNum,'e');
                            return {ExprType::Error,lineNum};
                     }
                     if (indexInfo.type == ExprType::Error) {
                            return {ExprType::Error,lineNum};
                     }
                     if (indexInfo.type != ExprType::Int) {
                            errors.emplace_back(indexInfo.line,'e');
                            return {ExprType::Error,lineNum};
                     }
              }
              bool wholeArray = symbol1->isArray() && !index;
              switch (symbol1->type) {
                     case BasicType::Int:
                            return {
                                   wholeArray ? ExprType::IntArray : ExprType::Int,
                                   lineNum
                               };

                     case BasicType::Char:
                            return {
                                   wholeArray ? ExprType::CharArray : ExprType::Char,
                                   lineNum
                               };

                     default:
                            return {ExprType::Error, lineNum};
              }

       }

       ExprInfo visitLiteralExpr (const LiteralExpr& expr) {
              Token literal = expr.getLiteral();
              switch (literal.getType()) {
                     case TokenType::INTCON:
                            return {ExprType::Int,literal.getLineNum()};
                     case TokenType::CHARCON:
                            return {ExprType::Char,literal.getLineNum()};
                     default:
                            return {ExprType::Error,literal.getLineNum()};
              }
       }


       void visitInit(const Init& initValue,BType type,bool isArray) {
              if (const auto* expr = std::get_if<ExprPtr>(&initValue.value)) {
                     visitExpr(**expr);
                     return;
              }
              // 2. 表达式列表。
              if (const auto* list = std::get_if<std::vector<ExprPtr>>(&initValue.value)) {
                     for (const auto& expr : *list) {
                            visitExpr(*expr);
                            // 后续：检查每个表达式的类型是否与数组元素类型匹配。
                     }

                     // 后续：结合 isArray 检查初始化形式。
                     return;
              }

              // 3. 字符串。
              if (const auto* stringLiteral = std::get_if<Token>(&initValue.value)) {
                     if (type != BType::Char) {
                            errors.emplace_back(stringLiteral->getLineNum(), 'e');
                     }

              }
       }

};