#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mips {

struct Register { std::string_view name; };
struct VirtualRegister { std::size_t id; };
struct Label { std::string name; };

inline constexpr std::array<Register, 4> argumentRegisters{{{"$a0"}, {"$a1"}, {"$a2"}, {"$a3"}}};
inline constexpr Register returnRegister{"$v0"};
inline constexpr Register zeroRegister{"$zero"};

inline constexpr bool fitsSignedImmediate(std::int64_t value) {
    return value >= -32768 && value <= 32767;
}

enum class StackArea { Local, Incoming, Outgoing };
struct StackAddress {
    StackArea area;
    std::size_t index;
};

using Destination = std::variant<Register, VirtualRegister>;
using Value = std::variant<Register, VirtualRegister, std::int64_t, StackAddress, Label>;
struct Memory { Value address; };
using Operand = std::variant<Value, Memory, std::int64_t, Label>;

struct Instruction {
    std::string_view opcode;
    std::optional<Destination> destination;
    std::vector<Operand> operands;
};

using Statement = std::variant<Label, Instruction>;

struct StackObject {
    std::int64_t size;
    std::int64_t alignment;
};

struct Function {
    Label label;
    Label exit;
    std::vector<Statement> body;
    std::vector<StackObject> locals;
    std::size_t virtualRegisters = 0;
    std::size_t outgoingWords = 0;
    bool savesReturnAddress = false;
};

struct GlobalData {
    Label label;
    bool isWord;
    std::vector<std::uint32_t> values;
};

struct Program {
    std::vector<GlobalData> globals;
    std::vector<Function> functions;
};

} // namespace mips
