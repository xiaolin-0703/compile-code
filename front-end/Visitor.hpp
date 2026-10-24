#pragma once
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "./AST/AST.hpp"
#include "../compileError.hpp"
#include "semantic/SymbolManager.hpp"

class Vistor {
public:
       explicit Vistor(CompUnit& ast) {
              initBuiltins();
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
       FuncType currentReturnType = FuncType::Void;
       int loopDepth = 0;
       int switchDepth = 0;
       std::unordered_map<std::string, Symbol> builtinFunctions;

       //添加一个符号
       void addSymbol(Symbol symbol) {
              if (!symbols.declare(symbol)) {
                     errors.emplace_back(symbol.ident.getLineNum(),'b');
              }
       }

       const Symbol* resolveSymbol(const Ident& ident) {
              if (const Symbol* symbol = symbols.lookup(ident.getName())) {
                     return symbol;
              }
              auto it = builtinFunctions.find(ident.getName());
              if (it != builtinFunctions.end()) {
                     return &it->second;
              }
              errors.emplace_back(ident.getLineNum(), 'c');
              return nullptr;
       }

       void initBuiltins() {
              auto add = [&](const std::string& name,
                             FuncType returnType,
                             std::vector<ParamInfo> parameters) {
                     builtinFunctions.emplace(
                         name,
                         Symbol(Ident(name, 0), returnType, std::move(parameters))
                     );
              };

              add("get_int", FuncType::Int, {});
              add("get_char", FuncType::Char, {});

              add("get_string", FuncType::Void, {
                  {BType::Char, true},  // char[]
                  {BType::Int, false}   // int
              });

              add("put_int", FuncType::Void, {
                  {BType::Int, false}
              });

              add("put_char", FuncType::Void, {
                  {BType::Char, false}
              });

              add("put_string", FuncType::Void, {
                  {BType::Char, true}
              });
       }

       void analyze(const CompUnit& ast) {
              for (const auto& declaration:ast.declarations) {
                     VisitDecl(*declaration);
              }
              for (const auto& function : ast.functions) {
                     visitFun(function);
              }
              visitMain(ast.mainFunction);
       }

       void visitMain(const MainFuncDef& mainFunction) {
              FuncType previousReturnType = currentReturnType;
              currentReturnType = FuncType::Int;

              symbols.pushScope();//mian的作用域
              visitBlockItems(mainFunction.body);
              // 检查函数体最后一项是否为 return。
              const auto& items = mainFunction.body.blockItems;
              if (items.empty() ||
                  dynamic_cast<const ReturnStmt*>(items.back().get()) == nullptr) {
                     errors.emplace_back(mainFunction.body.closingBraceLine, 'g');
                  }
              symbols.popScope();
              currentReturnType = previousReturnType;
       }

       void visitFun(const FuncDef& function) {
              std::vector<ParamInfo> parameters;
              parameters.reserve(function.parameters.size());
              for (const auto& param : function.parameters) {
                     parameters.push_back({param.type, param.isArray});
              }
              // 2. 在当前的全局作用域登记函数。
              Symbol functionSymbol(
                  function.identifier,
                  function.returnType,
                  std::move(parameters)
              );
              addSymbol(std::move(functionSymbol));
              symbols.pushScope();//进入函数作用域
              FuncType previousReturnType = currentReturnType;
              currentReturnType = function.returnType;
              for (const auto& param : function.parameters) {
                     SymbolKind kind = param.isArray
                         ? SymbolKind::Array
                         : SymbolKind::Variable;

                     Symbol parameterSymbol(
                         param.identifier,
                         kind,
                         param.type,
                         false,
                         false
                     );
                     addSymbol(std::move(parameterSymbol));
              }
              visitBlockItems(function.body);
              if (function.returnType != FuncType::Void) {
                     const auto& items = function.body.blockItems;
                     if (items.empty() ||
                         dynamic_cast<const ReturnStmt*>(items.back().get()) == nullptr) {
                            errors.emplace_back(function.body.closingBraceLine, 'g');
                         }
              }
              currentReturnType = previousReturnType;
              symbols.popScope();
       }

       void visitBlockItems(const BlockStmt& body) {
              for (const auto& stmt : body.blockItems) {
                     visitStmt(*stmt);
              }

       }

       void visitStmt(const BlockItem& stmt) {
              if (const auto* decl = dynamic_cast<const Decl*>(&stmt)) {
                     VisitDecl(*decl);
                     return;
              }

              if (const auto* block = dynamic_cast<const BlockStmt*>(&stmt)) {
                     symbols.pushScope();
                     visitBlockItems(*block);
                     symbols.popScope();
                     return;
              }

              const auto* assign = dynamic_cast<const AssignStmt*>(&stmt);
              if (assign) {
                     visitAssignStmt(*assign);
                     return;
              }
              const auto* expression = dynamic_cast<const ExprStmt*>(&stmt);
              if (expression) {
                     visitExprStmt(*expression);
                     return ;
              }
              const auto* ifstmt = dynamic_cast<const IfStmt*>(&stmt);
              if (ifstmt) {
                     visitIfStmt(*ifstmt);
                     return ;
              }
              const auto* whilestmt = dynamic_cast<const WhileStmt*>(&stmt);
              if (whilestmt) {
                     visitWhileStmt(*whilestmt);
                     return ;
              }
              const auto* breakStmt = dynamic_cast<const BreakStmt*>(&stmt);
              if (breakStmt) {
                     visitBreakStmt(*breakStmt);
                     return ;
              }
              const auto* continueStmt = dynamic_cast<const ContinueStmt*>(&stmt);
              if (continueStmt) {
                     visitContinueStmt(*continueStmt);
                     return ;
              }
              const auto* returnStmt = dynamic_cast<const ReturnStmt*>(&stmt);
              if (returnStmt) {
                     visitReturnStmt(*returnStmt);
                     return ;
              }
              const auto* printfStmt = dynamic_cast<const PrintfStmt*>(&stmt);
              if (printfStmt) {
                     visitPrintfStmt(*printfStmt);
                     return ;
              }
              const auto* switchStmt = dynamic_cast<const SwitchStmt*>(&stmt);
              if (switchStmt) {
                     visitSwitchStmt(*switchStmt);
                     return ;
              }

              throw std::runtime_error("不是Stmt!!");
       }

       void visitSwitchStmt(const SwitchStmt& switch_stmt) {
              ExprInfo switchInfo = visitExpr(*switch_stmt.switchExpr);
              if (switchInfo.type != ExprType::Error &&
                     switchInfo.type != ExprType::Int &&
                     switchInfo.type != ExprType::Char) {
                     errors.emplace_back(switchInfo.line, 'e');
    }
              ++switchDepth;
              bool seenDefault = false;
              // 另外用集合保存已经出现过的 case 常量值。
              std::unordered_set<int> caseValues;
              for (const auto& caseStmt : switch_stmt.cases) {
                     if (caseStmt.isDefault()) {
                            if (seenDefault) {
                                   errors.emplace_back(caseStmt.caseToken.getLineNum(), 'n');
                            }
                            seenDefault = true;
                     } else {
                            const Token& token = *caseStmt.value;
                            int value;
                            if (token.getType() == TokenType::INTCON) {
                                   value = std::get<int>(token.getValue());
                            } else {
                                   value = static_cast<unsigned char>(
                                       std::get<char>(token.getValue())
                                   );
                            }
                            if (!caseValues.insert(value).second) {
                                   errors.emplace_back(caseStmt.caseToken.getLineNum(), 'n');
                            }
                     }
                     visitCaseStmt(caseStmt, switchInfo.type);
              }
              --switchDepth;
       }

       void visitCaseStmt(const CaseStmt& stmt, ExprType switchType) {
              if (stmt.value) {
                     const Token& value = *stmt.value;
                     ExprType caseType = value.getType() == TokenType::INTCON
                         ? ExprType::Int
                         : ExprType::Char;
                     if ((switchType == ExprType::Int ||
                          switchType == ExprType::Char) &&
                         caseType != switchType) {
                            errors.emplace_back(value.getLineNum(), 'e');
                         }
              }

              for (const auto& statement : stmt.statements) {
                     visitStmt(*statement);
              }
       }

       void visitPrintfStmt(const PrintfStmt& printfStmt) {
              int line = printfStmt.printfToken.getLineNum();
              const std::string& format = printfStmt.formatString.getWord();
              std::vector<ExprType> expectedTypes;
              for (std::size_t i = 0; i + 1 < format.size(); ++i) {
                     if (format[i] != '%') {
                            continue;
                     }
                     switch (format[i + 1]) {
                            case 'd':
                                   expectedTypes.push_back(ExprType::Int);
                                   ++i;
                                   break;
                            case 'c':
                                   expectedTypes.push_back(ExprType::Char);
                                   ++i;
                                   break;
                            case 's':
                                   expectedTypes.push_back(ExprType::CharArray);
                                   ++i;
                                   break;
                     }
              }
              std::vector<ExprInfo> actualTypes;
              for (const auto& argument : printfStmt.arguments) {
                     actualTypes.push_back(visitExpr(*argument));
              }

              // 3. 格式符数量与实参数量不一致，报 l。
              if (expectedTypes.size() != actualTypes.size()) {
                     errors.emplace_back(line, 'l');
                     return;
              }

              // 4. 按位置比较类型。
              for (std::size_t i = 0; i < expectedTypes.size(); ++i) {
                     if (actualTypes[i].type == ExprType::Error) {
                            continue;
                     }
                     if (actualTypes[i].type != expectedTypes[i]) {
                            errors.emplace_back(actualTypes[i].line, 'e');
                            return;
                     }
              }
       }

       void visitReturnStmt(const ReturnStmt& returnStmt) {
              int line = returnStmt.returnToken.getLineNum();
              ExprInfo actual{ExprType::Void, line};
              if (returnStmt.returnValue) {
                     actual = visitExpr(*returnStmt.returnValue);
              }
              // void 函数不能出现 return 表达式；
              if (currentReturnType == FuncType::Void) {
                     if (returnStmt.returnValue) {
                            errors.emplace_back(line, 'f');//void时报错
                     }
                     return;
              }
              if (actual.type == ExprType::Error) {// 表达式内部已经出错，避免追加类型错误。
                     return;
              }
              ExprType expected = currentReturnType == FuncType::Int
                  ? ExprType::Int
                  : ExprType::Char;
              if (actual.type != expected) {
                     errors.emplace_back(line, 'e');//返回类型不匹配
              }
       }

       void visitContinueStmt(const ContinueStmt& continue_stmt) {
              if (loopDepth == 0) {
                     errors.emplace_back(continue_stmt.continueToken.getLineNum(),'m');
              }
       }

       void visitBreakStmt(const BreakStmt& break_stmt) {
              if (loopDepth == 0 && switchDepth == 0) {
                     errors.emplace_back(break_stmt.breakToken.getLineNum(),'m');
              }
       }

       void visitWhileStmt(const WhileStmt& whilestmt) {
              ExprInfo condition = visitExpr(*whilestmt.condition);
              ++loopDepth;
              visitStmt(*whilestmt.body);
              --loopDepth;
       }


       void visitIfStmt(const IfStmt& ifstmt) {
              ExprInfo condition = visitExpr(*ifstmt.condition);
              visitStmt(*ifstmt.thenBranch);
              if (ifstmt.elseBranch) {
                     visitStmt(*ifstmt.elseBranch);
              }
       }

       void visitExprStmt(const ExprStmt& stmt) {
              if (stmt.expression) {//可能为空只有一个";"
                     visitExpr(*stmt.expression);
              }
       }

       void visitAssignStmt(const AssignStmt& assign) {
              const LValExpr& target = *assign.target;
              int line = target.getIdent().getLineNum();
              ExprInfo left = visitLValExpr(target);
              ExprInfo right = visitExpr(*assign.value);
              if (left.type == ExprType::Error) {
                     return;
              }
              // 2. 检查是否修改常量。
              // 左值访问时已经检查过未定义，这里直接 lookup，
              // 不再调用 resolveSymbol，避免重复记录 c。
              const Symbol* symbol =
                  symbols.lookup(target.getIdent().getName());
              if (symbol != nullptr && symbol->isConst) {
                     errors.emplace_back(line, 'h');
                     return;
              }
              if (right.type == ExprType::Error) {
                     return;
              }
              if (left.type != ExprType::Int &&
                  left.type != ExprType::Char) {
                     errors.emplace_back(line, 'e');
                     return;
                  }
              if (left.type != right.type) {
                     errors.emplace_back(line, 'e');
              }
       }



       void VisitDecl (const Decl& decl) {
              const auto* dec1 = dynamic_cast<const VarDecl*>(&decl);
              if (dec1 != nullptr) {
                     visitVarDecl(*dec1);
              }
              const auto* dec2 = dynamic_cast<const ConstDecl*>(&decl);
              if (dec2 != nullptr) {
                     visitConDecl(*dec2);
              }
       }

       void visitConDecl(const ConstDecl& condecl) {
              BType type = condecl.type;
              for (const auto& def:condecl.definitions) {
                     visitConstDef(def,type);
              }
       }

       void visitConstDef(const ConstDef& def,BType type) {
              SymbolKind kind = def.arraySize ? SymbolKind::Array:SymbolKind::Variable;
              Symbol symbol1(def.ident,kind,type,true,false);
              if (def.arraySize) {
                     ExprInfo size = visitExpr(*def.arraySize);
                     if (size.type != ExprType::Error && size.type != ExprType::Int) {
                            errors.emplace_back(size.line, 'e');
                     }
              }
              addSymbol(symbol1);
              visitInit(def.initValue,type,kind == SymbolKind::Array);
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
              if (def.arraySize) {
                     ExprInfo size = visitExpr(*def.arraySize);
                     if (size.type != ExprType::Error && size.type != ExprType::Int) {
                            errors.emplace_back(size.line, 'e');
                     }
              }
              addSymbol(std::move(symbol));
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
              if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
                    return visitUnaryExpr(*unary);
              }
              if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
                     return visitBinaryExpr(*binary);
              }
              if (const auto* call = dynamic_cast<const CallExpr*>(&expr)) {
                     return visitCallExpr(*call);
              }
              if (const auto* cast = dynamic_cast<const CastExpr*>(&expr)) {
                     return visitCastExpr(*cast);
              }
              throw std::runtime_error("不是一个表达式！！");
              return {ExprType::Error,0};
       }

       ExprInfo visitCastExpr (const CastExpr& expr) {
              const BType btype = expr.targetType;
              ExprInfo changedExpr = visitExpr(*expr.expr);
              int line = changedExpr.line;
              if (changedExpr.type == ExprType::Int || changedExpr.type == ExprType::Char) {
                     return {btype == BType::Char ? ExprType::Char : ExprType::Int,line};
              }
              return {ExprType::Error,line};
       }

       ExprInfo visitCallExpr(const CallExpr& expr) {
              int line = expr.functionName.getLineNum();
              const Symbol* function = resolveSymbol(expr.functionName);

              std::vector<ExprInfo> arguments;
              bool argumentError = false;

              for (const auto& argument : expr.arguments) {
                     ExprInfo info = visitExpr(*argument);
                     arguments.push_back(info);
                     if (info.type == ExprType::Error) {
                            argumentError = true;
                     }
              }
              // resolveSymbol 已经记录未定义错误 c。
              if (function == nullptr) {
                     return {ExprType::Error, line};
              }
              if (!function->isFunction()) {
                     errors.emplace_back(line, 'e');
                     return {ExprType::Error, line};
              }

              if (arguments.size() != function->parameters.size()) {
                     errors.emplace_back(line, 'd');
                     return {ExprType::Error, line};
              }

              if (argumentError) {
                     return {ExprType::Error, line};
              }

              for (std::size_t i = 0; i < arguments.size(); ++i) {
                     const ParamInfo& parameter = function->parameters[i];
                     ExprType expected;
                     if (parameter.isArray) {
                            expected = parameter.type == BType::Int
                                ? ExprType::IntArray
                                : ExprType::CharArray;
                     } else {
                            expected = parameter.type == BType::Int
                                ? ExprType::Int
                                : ExprType::Char;
                     }

                     if (arguments[i].type != expected) {
                            errors.emplace_back(line, 'e');
                            return {ExprType::Error, line};
                     }
              }
              switch (function->type) {
                     case BasicType::Int:
                            return {ExprType::Int, line};
                     case BasicType::Char:
                            return {ExprType::Char, line};
                     case BasicType::Void:
                            return {ExprType::Void, line};
                     default:
                            return {ExprType::Error, line};
              }
       }

       ExprInfo visitBinaryExpr (const BinaryExpr& expr) {
              ExprInfo left = visitExpr(*expr.left);
              ExprInfo right = visitExpr(*expr.right);
              int line = expr.op.getLineNum();

              if (left.type == ExprType::Error || right.type == ExprType::Error) {
                     return {ExprType::Error,line};
              }

              if (left.type != ExprType::Int && left.type != ExprType::Char || (left.type != right.type) ) {
                     errors.emplace_back(line, 'e');
                     return {ExprType::Error,line};
              }
              switch (expr.op.getType()) {
                     case TokenType::PLUS:
                     case TokenType::MINU:
                     case TokenType::MULT:
                     case TokenType::DIV:
                     case TokenType::MOD:
                            return {left.type, line};
                     case TokenType::LSS:
                     case TokenType::LEQ:
                     case TokenType::GRE:
                     case TokenType::GEQ:
                     case TokenType::EQL:
                     case TokenType::NEQ:
                     case TokenType::AND:
                     case TokenType::OR:
                            return {ExprType::Int, line};
                     default:
                            return {ExprType::Error, line};
              }
       }

       ExprInfo visitUnaryExpr (const UnaryExpr& expr) {
              ExprInfo operand = visitExpr(*expr.operand);
              int line = expr.op.getLineNum();
              if (operand.type == ExprType::Error) {
                     return {ExprType::Error,line};
              }
              if (operand.type != ExprType::Int && operand.type != ExprType::Char) {
                     errors.emplace_back(line,'e');
                     return {ExprType::Error,line};
              }

              switch (expr.op.getType()) {
                     case TokenType::PLUS:
                     case TokenType::MINU:
                            return {operand.type, line};
                     case TokenType::NOT:
                            return {ExprType::Int, line};
                     default:
                            return {ExprType::Error, line};
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
              const Token literal = expr.getLiteral();
              switch (literal.getType()) {
                     case TokenType::INTCON:
                            return {ExprType::Int,literal.getLineNum()};
                     case TokenType::CHARCON:
                            return {ExprType::Char,literal.getLineNum()};
                     default:
                            return {ExprType::Error,literal.getLineNum()};
              }
       }


       void visitInit(const Init& initValue, BType type, bool isArray) {
              ExprType expected = type == BType::Int
                  ? ExprType::Int
                  : ExprType::Char;
              auto checkExpression = [&](const Expr& expr) {
                     ExprInfo actual = visitExpr(expr);
                     if (actual.type != ExprType::Error &&
                         actual.type != expected) {
                            errors.emplace_back(actual.line, 'e');
                         }
              };

              // 1. 单个表达式。
              if (const auto* expr =
                      std::get_if<ExprPtr>(&initValue.value)) {
                     checkExpression(**expr);
                     return;
                      }

              // 2. 表达式列表。
              if (const auto* list =
                      std::get_if<std::vector<ExprPtr>>(&initValue.value)) {
                     for (const auto& expr : *list) {
                            checkExpression(*expr);
                     }
                     return;
                      }

              // 3. 字符串。
              if (const auto* stringLiteral =
                      std::get_if<Token>(&initValue.value)) {
                     if (type != BType::Char) {
                            errors.emplace_back(stringLiteral->getLineNum(), 'e');
                     }
                      }
       }

};