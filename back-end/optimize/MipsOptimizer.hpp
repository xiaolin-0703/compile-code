#pragma once

#include "../machine-ir/MachineIR.hpp"

#include <utility>

namespace mips {

inline void optimize(Program& program) {
    for (auto& function : program.functions) {
        std::vector<Statement> body;
        body.reserve(function.body.size());
        for (std::size_t i = 0; i < function.body.size(); ++i) {
            auto& statement = function.body[i];
            const auto* jump = std::get_if<Instruction>(&statement);
            if (jump && jump->opcode == "j" && i + 1 < function.body.size()) {
                const auto* next = std::get_if<Label>(&function.body[i + 1]);
                if (next && next->name == std::get<Label>(jump->operands.front()).name) continue;
            }
            body.push_back(std::move(statement));
        }
        function.body = std::move(body);
    }
}

} // namespace mips
