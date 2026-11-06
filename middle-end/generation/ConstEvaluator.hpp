#pragma once

#include "../../front-end/semantic/SemanticModel.hpp"
#include <cstdint>

namespace ir {
// 常量求值只计算语言要求的编译期表达式，不执行有副作用的函数调用。
class ConstEvaluator {
private:
    const SemanticModel& semanticModel;
    std::unordered_map<SymbolId, std::vector<std::int64_t>> values;

    std::int64_t normalize(const Expr& expr, std::int64_t value) const {
        if (semanticModel.getType(expr) == ExprType::Char) return static_cast<std::uint8_t>(value);
        std::uint32_t bits = static_cast<std::uint32_t>(value);
        return bits < 0x80000000u ? static_cast<std::int64_t>(bits)
                                : static_cast<std::int64_t>(bits) - 0x100000000LL;
    }
public:
    explicit ConstEvaluator(const SemanticModel& semanticModel) : semanticModel(semanticModel) {}
    static std::int64_t literal(const Token& token) {
        if (const auto* number = std::get_if<int>(&token.getValue())) return *number;
        if (const auto* character = std::get_if<char>(&token.getValue())) return static_cast<unsigned char>(*character);
        if (token.getType() == TokenType::INTCON) return std::stoll(token.getWord());
        throw std::logic_error("Literal has no decoded value");
    }
    static const std::string& stringValue(const Token& token) {
        return std::get<std::string>(token.getValue());
    }
    void define(SymbolId id, const std::vector<std::uint32_t>& data, bool character) {
        auto& result = values[id];
        for (std::uint32_t bits : data) {
            result.push_back(character || bits < 0x80000000u ? static_cast<std::int64_t>(bits)
                                                            : static_cast<std::int64_t>(bits) - 0x100000000LL);
        }
    }
    std::int64_t evaluate(const Expr& expr) const {
        // dynamic_cast 按实际 AST 类型分派；不匹配时返回 nullptr。
        if (const auto* node = dynamic_cast<const LiteralExpr*>(&expr)) return literal(node->getLiteral());
        if (const auto* node = dynamic_cast<const LValExpr*>(&expr)) {
            const auto& data = values.at(semanticModel.getReference(expr));
            std::int64_t index = node->index ? evaluate(*node->index) : 0;
            if (index < 0 || static_cast<std::size_t>(index) >= data.size()) throw std::logic_error("Constant array index out of range");
            return data[static_cast<std::size_t>(index)];
        }
        if (const auto* node = dynamic_cast<const CastExpr*>(&expr)) return normalize(expr, evaluate(*node->expr));
        if (const auto* node = dynamic_cast<const UnaryExpr*>(&expr)) {
            std::int64_t operand = evaluate(*node->operand);
            switch (node->op.getType()) {
                case TokenType::PLUS: return normalize(expr, operand);
                case TokenType::MINU: return normalize(expr, -operand);
                case TokenType::NOT: return operand == 0;
                default: throw std::logic_error("Unsupported constant unary operator");
            }
        }
        if (const auto* node = dynamic_cast<const BinaryExpr*>(&expr)) {
            std::int64_t left = evaluate(*node->left);
            TokenType op = node->op.getType();
            if (op == TokenType::AND && left == 0) return 0;
            if (op == TokenType::OR && left != 0) return 1;
            std::int64_t right = evaluate(*node->right);
            switch (op) {
                case TokenType::PLUS: return normalize(expr, left + right);
                case TokenType::MINU: return normalize(expr, left - right);
                case TokenType::MULT: return normalize(expr, left * right);
                case TokenType::DIV:
                case TokenType::MOD:
                    if (right == 0) throw std::logic_error("Division by zero in constant expression");
                    return normalize(expr, op == TokenType::DIV ? left / right : left % right);
                case TokenType::LSS: return left < right;
                case TokenType::LEQ: return left <= right;
                case TokenType::GRE: return left > right;
                case TokenType::GEQ: return left >= right;
                case TokenType::EQL: return left == right;
                case TokenType::NEQ: return left != right;
                case TokenType::AND: return left != 0 && right != 0;
                case TokenType::OR: return left != 0 || right != 0;
                default: break;
            }
        }
        throw std::logic_error("Expression is not a compile-time constant");
    }
};
} // namespace ir
