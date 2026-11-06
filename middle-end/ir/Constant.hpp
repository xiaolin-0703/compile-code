#pragma once

#include "Value.hpp"

#include <cstdint>

namespace ir {

class IRContext;

// 整数常量保存位模式。相同 Context 中，同类型、同位模式的常量复用。
class ConstantInt final : public Value {
public:
    std::uint32_t getBits() const { return bits; }
    std::int64_t getSignedValue() const {
        const unsigned width = getType()->getBitWidth();
        const std::uint32_t signBit = std::uint32_t{1} << (width - 1);
        if ((bits & signBit) == 0) return bits;
        return static_cast<std::int64_t>(bits) - (std::int64_t{1} << width);
    }

private:
    friend class IRContext; // 只允许 Context 创建、复用整数常量。
    ConstantInt(const Type* type, std::uint32_t bits)
        : Value(type), bits(bits) {}

    static std::uint32_t normalize(unsigned width, std::int64_t value) {
        const auto bits = static_cast<std::uint32_t>(value);
        return width == 32 ? bits : bits & ((std::uint32_t{1} << width) - 1);
    }

    std::uint32_t bits;
};

} // namespace ir
