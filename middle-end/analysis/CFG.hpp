1#pragma once

#include "../ir/Instructions.hpp"
#include <unordered_set>

namespace ir {
// CFG 从终结指令推导，每次分析重新构造，避免使用过期缓存。
class CFG {
public:
    std::unordered_map<const BasicBlock*, std::vector<const BasicBlock*>> predecessors;
    std::unordered_map<const BasicBlock*, std::vector<const BasicBlock*>> successors;
    std::unordered_set<const BasicBlock*> reachable;
    std::unordered_map<const BasicBlock*, std::unordered_set<const BasicBlock*>> dominators;

    explicit CFG(const Function& function) {
        for (const auto& block : function.getBlocks()) {
            predecessors[block.get()];
            successors[block.get()];
        }
        for (const auto& block : function.getBlocks()) {
            const auto* branch = dynamic_cast<const BranchInst*>(block->getTerminator());
            if (branch == nullptr) continue;
            for (std::size_t i = branch->isConditional() ? 1 : 0; i < branch->getOperandCount(); ++i) {
                auto* target = dynamic_cast<const BasicBlock*>(branch->getOperand(i));
                if (target == nullptr || target->getParent() != &function) throw std::logic_error("Invalid branch target");
                auto& next = successors[block.get()];
                if (std::find(next.begin(), next.end(), target) != next.end()) continue;
                next.push_back(target);
                predecessors[target].push_back(block.get());
            }
        }
        if (function.isDeclaration()) return;
        std::vector<const BasicBlock*> work = {function.getEntry()};
        while (!work.empty()) {
            const BasicBlock* block = work.back();
            work.pop_back();
            if (!reachable.insert(block).second) continue;
            for (const BasicBlock* next : successors[block]) work.push_back(next);
        }
        for (const BasicBlock* block : reachable) {
            dominators[block] = block == function.getEntry()
                ? std::unordered_set<const BasicBlock*>{block} : reachable;
        }
        bool changed = true;
        while (changed) {
            changed = false;
            for (const BasicBlock* block : reachable) {
                if (block == function.getEntry()) continue;
                auto intersection = reachable;
                for (const BasicBlock* previous : predecessors[block]) {
                    if (!reachable.count(previous)) continue;
                    for (auto it = intersection.begin(); it != intersection.end();) {
                        if (!dominators[previous].count(*it)) it = intersection.erase(it);
                        else ++it;
                    }
                }
                intersection.insert(block);
                if (intersection != dominators[block]) {
                    dominators[block] = std::move(intersection);
                    changed = true;
                }
            }
        }
    }
};
} // namespace ir
