#include "Symbol.hpp"
#include <memory>
#include <vector>
#include "SymbolTable.hpp"


class SymbolManager {
public:
    //初始符号为1
    SymbolManager() : currentId(1) {
        tables.emplace_back(1,0);
    };

    void pushScope() {
        int newId = tables.size() + 1;
        tables.emplace_back(newId,currentId);
        currentId = newId;
    }

    void popScope() {
        if (currentId == 1) {
            throw std::runtime_error("Scope is already entered");
        }
        currentId = tables[currentId - 1].fatherId;
    }

    bool declare(Symbol symbol) {
        return tables[currentId - 1].addSymbol(std::move(symbol));
    }

    const Symbol* lookup(std::string name) {
        int id = currentId;
        while (id != 0) {
            const SymbolTable& table = tables[id-1];

            if (table.directory.find(name) != table.directory.end()) {
                return &table.directory.at(name);
            }

            id = table.fatherId;
        }

        return nullptr;
    }

    void output(std::ostream& out) const {
        for (const auto& table : tables) {
            for (const auto& name : table.declarationOrder) {
                const Symbol& symbol = table.directory.at(name);
                out << table.tableid << ' '
                    << symbol.getName() << ' '
                    << symbol.getTypeName() << '\n';
            }
        }
    }



private:
    std::vector<SymbolTable> tables;
    int currentId;
};
