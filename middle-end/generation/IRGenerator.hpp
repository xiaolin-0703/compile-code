#pragma once

#include "ConstEvaluator.hpp"
#include "../builder/IRBuilder.hpp"
#include "../../front-end/AST/CompUnit.hpp"

namespace ir {

    //保存源码常量或者变量的地址以及基本类型
    struct IRBinding {
        Value* address = nullptr;
        const Type* elementType = nullptr;
        bool array = false;
        bool constant = false;
    };

class IRGenerator {
private:
    IRContext& context;
    const SemanticModel& semanticModel;
    Module module;
    IRBuilder builder;
    ConstEvaluator constants;
    Function* currentFunction = nullptr;
    std::unordered_map<SymbolId, IRBinding> bindings;
    std::vector<BasicBlock*> breakTargets;
    std::vector<BasicBlock*> continueTargets;
    std::size_t globalNumber = 0;

    // 语言类型与 IR 类型分开：char 是 i8，int 是 i32。
    const Type* basicType(BType type) const {
        return type == BType::Char ? context.getInt8Type() : context.getInt32Type();
    }
    const Type* returnType(FuncType type) const {
        if (type == FuncType::Void) return context.getVoidType();
        return type == FuncType::Char ? context.getInt8Type() : context.getInt32Type();
    }
    ConstantInt* integer(std::int64_t value) { return context.getInt(context.getInt32Type(), value); }
    Value* boolean(Value* value) {
        if (value->getType()->isInteger(1)) return value;
        return builder.createCompare("ne", value, context.getInt(value->getType(), 0));
    }
    void jumpIfOpen(BasicBlock* target) {
        if (!builder.isTerminated()) builder.createBranch(target);
    }
    void addBuiltins() {
        const Type* i32 = context.getInt32Type();
        const Type* i8 = context.getInt8Type();
        const Type* pointer = context.getPointerType(i8);
        const Type* nothing = context.getVoidType();
        module.addFunction("get_int", i32, {});
        module.addFunction("get_char", i8, {});
        module.addFunction("get_string", nothing, {pointer, i32});
        module.addFunction("put_int", nothing, {i32});
        module.addFunction("put_char", nothing, {i8});
        module.addFunction("put_string", nothing, {pointer});
    }

    std::vector<std::uint32_t> constantInitializer(const Type* element, std::size_t count, const Init* init) {
        std::vector<std::uint32_t> result(count, 0);
        if (init == nullptr) return result;
        if (const auto* expr = std::get_if<ExprPtr>(&init->value)) {
            result.at(0) = context.getInt(element, constants.evaluate(**expr))->getBits();
        } else if (const auto* list = std::get_if<std::vector<ExprPtr>>(&init->value)) {
            if (list->size() > count) throw std::logic_error("Too many array initializers");
            for (std::size_t i = 0; i < list->size(); ++i) {
                result[i] = context.getInt(element, constants.evaluate(*(*list)[i]))->getBits();
            }
        } else {
            const std::string& text = ConstEvaluator::stringValue(std::get<Token>(init->value));
            for (std::size_t i = 0; i < count && i < text.size(); ++i) {
                result[i] = static_cast<unsigned char>(text[i]);
            }
        }
        return result;
    }

    // 统一处理变量/常量、标量/数组、局部/全局/static 的存储与初始化。
    void emitDefinition(const Ident& ident, BType sourceType, const Expr* size,
                        const Init* init, bool constant, bool isStatic) {
        SymbolId id = semanticModel.getDeclaration(ident);
        const Type* element = basicType(sourceType);
        std::int64_t length = size == nullptr ? 1 : constants.evaluate(*size);
        if (length <= 0) throw std::logic_error("Array size must be positive");
        std::size_t count = static_cast<std::size_t>(length);
        const Type* storedType = size == nullptr ? element : context.getArrayType(element, count);
        Value* address;
        bool global = currentFunction == nullptr || isStatic;
        std::vector<std::uint32_t> initialValues;
        if (global || constant || (init != nullptr && std::holds_alternative<Token>(init->value))) {
            initialValues = constantInitializer(element, count, init);
        }
        if (global) {
            std::string name = ident.getName();
            // 现有前端也接受全局 static；只有静态局部变量需要函数名消歧。
            if (isStatic && currentFunction != nullptr) {
                name = ".static." + currentFunction->getName() + "." + std::to_string(globalNumber++) + "." + name;
            }
            address = module.addGlobal(name, storedType, constant, initialValues);
        } else {
            // alloca 放在 entry，初始化的 store 保留在原来的控制流位置。
            address = builder.createAlloca(storedType);
        }
        bindings[id] = {address, element, size != nullptr, constant};
        if (constant) constants.define(id, initialValues, element->isInteger(8));
        if (global || init == nullptr) return;

        if (!initialValues.empty()) {
            for (std::size_t i = 0; i < count; ++i) {
                Value* target = size == nullptr ? address : builder.createGep(address, integer(i));
                builder.createStore(context.getInt(element, initialValues[i]), target);
            }
            return;
        }
        if (const auto* expr = std::get_if<ExprPtr>(&init->value)) {
            Value* value = emitExpr(**expr);
            Value* target = size == nullptr ? address : builder.createGep(address, integer(0));
            builder.createStore(builder.createCast(value, element), target);
        } else if (const auto* list = std::get_if<std::vector<ExprPtr>>(&init->value)) {
            if (list->size() > count) throw std::logic_error("Too many array initializers");
            for (std::size_t i = 0; i < count; ++i) {
                Value* value = i < list->size() ? emitExpr(*(*list)[i]) : context.getInt(element, 0);
                Value* target = size == nullptr ? address : builder.createGep(address, integer(i));
                builder.createStore(builder.createCast(value, element), target);
            }
        }
    }

    void emitDecl(const Decl& declaration) {
        if (const auto* node = dynamic_cast<const VarDecl*>(&declaration)) {
            for (const auto& definition : node->definitions) {
                emitDefinition(definition.ident, node->type, definition.arraySize.get(),
                               definition.initValue ? &*definition.initValue : nullptr, false, node->isStatic);
            }
        } else if (const auto* node = dynamic_cast<const ConstDecl*>(&declaration)) {
            for (const auto& definition : node->definitions) {
                emitDefinition(definition.ident, node->type, definition.arraySize.get(), &definition.initValue, true, false);
            }
        } else throw std::logic_error("Unknown declaration node");
    }

    // 左值求地址：赋值目标不进行 load，数组元素先通过 GEP 求元素地址。
    Value* emitAddress(const LValExpr& expr) {
        const IRBinding& binding = bindings.at(semanticModel.getReference(expr));
        if (expr.index) {
            Value* index = emitExpr(*expr.index);
            return builder.createGep(binding.address, index);
        }
        return binding.address;
    }

    // 递归求表达式的值。返回非拥有指针，具体对象归 Context 或基本块所有。
    Value* emitExpr(const Expr& expr) {
        if (const auto* node = dynamic_cast<const LiteralExpr*>(&expr)) {
            const Type* type = node->getLiteral().getType() == TokenType::CHARCON
                ? context.getInt8Type() : context.getInt32Type();
            return context.getInt(type, ConstEvaluator::literal(node->getLiteral()));
        }
        if (const auto* node = dynamic_cast<const LValExpr*>(&expr)) {
            const IRBinding& binding = bindings.at(semanticModel.getReference(expr));
            if (binding.constant && !binding.array) return context.getInt(binding.elementType, constants.evaluate(expr));
            Value* address = emitAddress(*node);
            if (binding.array && !node->index) {
                return address->getType()->getElementType()->isArray() ? builder.createGep(address, integer(0)) : address;
            }
            return builder.createLoad(address);
        }
        if (const auto* node = dynamic_cast<const CastExpr*>(&expr)) {
            return builder.createCast(emitExpr(*node->expr), basicType(node->targetType));
        }
        if (const auto* node = dynamic_cast<const CallExpr*>(&expr)) {
            std::vector<Value*> arguments;
            for (const auto& argument : node->arguments) arguments.push_back(emitExpr(*argument));
            const Symbol& symbol = semanticModel.getSymbol(semanticModel.getReference(expr));
            return builder.createCall(module.getFunction(symbol.getName()), arguments);
        }
        if (const auto* node = dynamic_cast<const UnaryExpr*>(&expr)) {
            Value* operand = emitExpr(*node->operand);
            switch (node->op.getType()) {
                case TokenType::PLUS: return operand;
                case TokenType::MINU: return builder.createBinary(Opcode::Sub, context.getInt(operand->getType(), 0), operand);
                case TokenType::NOT:
                    return builder.createCast(builder.createCompare("eq", operand, context.getInt(operand->getType(), 0)), context.getInt32Type());
                default: throw std::logic_error("Unknown unary operator");
            }
        }
        if (const auto* node = dynamic_cast<const BinaryExpr*>(&expr)) {
            TokenType op = node->op.getType();
            if (op == TokenType::AND || op == TokenType::OR) {
                BasicBlock* onTrue = currentFunction->createBlock();
                BasicBlock* onFalse = currentFunction->createBlock();
                BasicBlock* end = currentFunction->createBlock();
                emitCondition(expr, onTrue, onFalse);
                builder.setBlock(onTrue);
                builder.createBranch(end);
                builder.setBlock(onFalse);
                builder.createBranch(end);
                builder.setBlock(end);
                PhiInst* result = builder.createPhi(context.getInt32Type());
                result->addIncoming(integer(1), onTrue);
                result->addIncoming(integer(0), onFalse);
                return result;
            }
            Value* left = emitExpr(*node->left);
            Value* right = emitExpr(*node->right);
            bool character = left->getType()->isInteger(8);
            switch (op) {
                case TokenType::PLUS: return builder.createBinary(Opcode::Add, left, right);
                case TokenType::MINU: return builder.createBinary(Opcode::Sub, left, right);
                case TokenType::MULT: return builder.createBinary(Opcode::Mul, left, right);
                case TokenType::DIV: return builder.createBinary(character ? Opcode::UDiv : Opcode::SDiv, left, right);
                case TokenType::MOD: return builder.createBinary(character ? Opcode::URem : Opcode::SRem, left, right);
                default: break;
            }
            std::string predicate;
            switch (op) {
                case TokenType::EQL: predicate = "eq"; break;
                case TokenType::NEQ: predicate = "ne"; break;
                case TokenType::LSS: predicate = character ? "ult" : "slt"; break;
                case TokenType::LEQ: predicate = character ? "ule" : "sle"; break;
                case TokenType::GRE: predicate = character ? "ugt" : "sgt"; break;
                case TokenType::GEQ: predicate = character ? "uge" : "sge"; break;
                default: throw std::logic_error("Unknown binary operator");
            }
            return builder.createCast(builder.createCompare(predicate, left, right), context.getInt32Type());
        }
        throw std::logic_error("Unknown expression node");
    }

    // 条件翻译返回控制流，而不是先把整棵条件表达式都求值。
    void emitCondition(const Expr& expr, BasicBlock* onTrue, BasicBlock* onFalse) {
        if (const auto* node = dynamic_cast<const BinaryExpr*>(&expr)) {
            TokenType op = node->op.getType();
            if (op == TokenType::AND || op == TokenType::OR) {
                BasicBlock* next = currentFunction->createBlock();
                // 短路：只有需要右值时才进入 next，不能预先计算左右两边。
                if (op == TokenType::AND) emitCondition(*node->left, next, onFalse);
                else emitCondition(*node->left, onTrue, next);
                builder.setBlock(next);
                emitCondition(*node->right, onTrue, onFalse);
                return;
            }
        }
        Value* value = emitExpr(expr);
        builder.createBranch(boolean(value), onTrue, onFalse);
    }

    Value* stringAddress(const std::string& text) {
        std::vector<std::uint32_t> bytes;
        for (unsigned char character : text) bytes.push_back(character);
        bytes.push_back(0);
        const Type* type = context.getArrayType(context.getInt8Type(), bytes.size());
        auto* global = module.addGlobal(".str." + std::to_string(globalNumber++), type, true, bytes);
        return builder.createGep(global, integer(0));
    }
    void emitPrintf(const PrintfStmt& stmt) {
        std::vector<Value*> arguments;
        // 所有参数先求值，随后再输出，避免把参数副作用夹在文本输出之间。
        for (const auto& argument : stmt.arguments) arguments.push_back(emitExpr(*argument));
        const std::string& text = ConstEvaluator::stringValue(stmt.formatString);
        std::string plain;
        std::size_t argumentIndex = 0;
        auto flush = [&]() {
            if (!plain.empty()) {
                builder.createCall(module.getFunction("put_string"), {stringAddress(plain)});
                plain.clear();
            }
        };
        for (std::size_t i = 0; i < text.size() && text[i] != '\0'; ++i) {
            if (text[i] != '%' || i + 1 == text.size()) { plain += text[i]; continue; }
            char format = text[++i];
            if (format == '%') { plain += '%'; continue; }
            flush();
            std::string function = format == 'd' ? "put_int" : format == 'c' ? "put_char" : "put_string";
            builder.createCall(module.getFunction(function), {arguments.at(argumentIndex++)});
        }
        flush();
    }

    void emitBlock(const BlockStmt& block) {
        for (const auto& item : block.blockItems) {
            if (builder.isTerminated()) break;
            emitStmt(*item);
        }
    }
    // 与现有语义分析风格一致，用 dynamic_cast 识别语句类型后分别处理。
    void emitStmt(const BlockItem& stmt) {
        if (const auto* node = dynamic_cast<const Decl*>(&stmt)) { emitDecl(*node); return; }
        if (const auto* node = dynamic_cast<const BlockStmt*>(&stmt)) { emitBlock(*node); return; }
        if (const auto* node = dynamic_cast<const AssignStmt*>(&stmt)) {
            Value* address = emitAddress(*node->target);
            Value* value = emitExpr(*node->value);
            builder.createStore(value, address);
            return;
        }
        if (const auto* node = dynamic_cast<const ExprStmt*>(&stmt)) {
            if (node->expression) emitExpr(*node->expression);
            return;
        }
        if (const auto* node = dynamic_cast<const ReturnStmt*>(&stmt)) {
            builder.createReturn(node->returnValue ? emitExpr(*node->returnValue) : nullptr);
            return;
        }
        if (dynamic_cast<const BreakStmt*>(&stmt)) { builder.createBranch(breakTargets.back()); return; }
        if (dynamic_cast<const ContinueStmt*>(&stmt)) { builder.createBranch(continueTargets.back()); return; }
        if (const auto* node = dynamic_cast<const PrintfStmt*>(&stmt)) { emitPrintf(*node); return; }
        if (const auto* node = dynamic_cast<const IfStmt*>(&stmt)) {
            BasicBlock* thenBlock = currentFunction->createBlock();
            BasicBlock* end = currentFunction->createBlock();
            BasicBlock* elseBlock = node->elseBranch ? currentFunction->createBlock() : end;
            emitCondition(*node->condition, thenBlock, elseBlock);
            builder.setBlock(thenBlock);
            emitStmt(*node->thenBranch);
            jumpIfOpen(end);
            if (node->elseBranch) {
                builder.setBlock(elseBlock);
                emitStmt(*node->elseBranch);
                jumpIfOpen(end);
            }
            builder.setBlock(end);
            return;
        }
        if (const auto* node = dynamic_cast<const WhileStmt*>(&stmt)) {
            BasicBlock* condition = currentFunction->createBlock();
            BasicBlock* body = currentFunction->createBlock();
            BasicBlock* end = currentFunction->createBlock();
            builder.createBranch(condition);
            builder.setBlock(condition);
            emitCondition(*node->condition, body, end);
            breakTargets.push_back(end);
            continueTargets.push_back(condition);
            builder.setBlock(body);
            emitStmt(*node->body);
            jumpIfOpen(condition);
            continueTargets.pop_back();
            breakTargets.pop_back();
            builder.setBlock(end);
            return;
        }
        if (const auto* node = dynamic_cast<const SwitchStmt*>(&stmt)) {
            Value* value = emitExpr(*node->switchExpr);
            std::vector<BasicBlock*> cases;
            for (std::size_t i = 0; i < node->cases.size(); ++i) cases.push_back(currentFunction->createBlock());
            BasicBlock* end = currentFunction->createBlock();
            BasicBlock* otherwise = end;
            for (std::size_t i = 0; i < node->cases.size(); ++i) {
                if (node->cases[i].isDefault()) otherwise = cases[i];
            }
            for (std::size_t i = 0; i < node->cases.size(); ++i) {
                if (node->cases[i].isDefault()) continue;
                BasicBlock* next = currentFunction->createBlock();
                Value* number = context.getInt(value->getType(), ConstEvaluator::literal(*node->cases[i].value));
                builder.createBranch(builder.createCompare("eq", value, number), cases[i], next);
                builder.setBlock(next);
            }
            builder.createBranch(otherwise);
            breakTargets.push_back(end);
            for (std::size_t i = 0; i < node->cases.size(); ++i) {
                builder.setBlock(cases[i]);
                for (const auto& item : node->cases[i].statements) {
                    if (builder.isTerminated()) break;
                    emitStmt(*item);
                }
                jumpIfOpen(i + 1 < cases.size() ? cases[i + 1] : end); // C 风格贯穿。
            }
            breakTargets.pop_back();
            builder.setBlock(end);
            return;
        }
        throw std::logic_error("Unknown statement node");
    }

    // 标量形参先落到 entry 的栈槽；数组形参直接保留传入的元素指针。
    void emitFunction(const FuncDef& definition) {
        currentFunction = module.getFunction(definition.identifier.getName());
        builder.setBlock(currentFunction->createBlock());
        for (std::size_t i = 0; i < definition.parameters.size(); ++i) {
            const auto& parameter = definition.parameters[i];
            Value* value = currentFunction->getArguments()[i].get();
            Value* address = value;
            if (!parameter.isArray) {
                address = builder.createAlloca(value->getType());
                builder.createStore(value, address);
            }
            bindings[semanticModel.getDeclaration(parameter.identifier)] = {
                address, basicType(parameter.type), parameter.isArray, false};
        }
        emitBlock(definition.body);
        if (!builder.isTerminated()) {
            builder.createReturn(currentFunction->getReturnType()->isVoid() ? nullptr
                : context.getInt(currentFunction->getReturnType(), 0));
        }
        currentFunction = nullptr;
    }
public:
    IRGenerator(IRContext& context, const SemanticModel& semanticModel)
        : context(context), semanticModel(semanticModel), module(context), builder(context), constants(semanticModel) {}

    Module& generate(const CompUnit& ast) {
        if (!module.getFunctions().empty()) throw std::logic_error("Generator can only run once");
        addBuiltins();
        for (const auto& declaration : ast.declarations) emitDecl(*declaration);
        // 先登记函数签名，再生成函数体；自递归调用能找到对应 Function 对象。
        for (const auto& definition : ast.functions) {
            std::vector<const Type*> parameters;
            for (const auto& parameter : definition.parameters) {
                const Type* type = basicType(parameter.type);
                parameters.push_back(parameter.isArray ? context.getPointerType(type) : type);
            }
            module.addFunction(definition.identifier.getName(), returnType(definition.returnType), parameters);
        }
        module.addFunction("main", context.getInt32Type(), {});
        for (const auto& definition : ast.functions) emitFunction(definition);
        currentFunction = module.getFunction("main");
        builder.setBlock(currentFunction->createBlock());
        emitBlock(ast.mainFunction.body);
        if (!builder.isTerminated()) builder.createReturn(integer(0));
        currentFunction = nullptr;
        return module;
    }
};
} // namespace ir
