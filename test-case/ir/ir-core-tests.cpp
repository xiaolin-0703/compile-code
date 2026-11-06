#include "middle-end/ir/Constant.hpp"
#include "middle-end/ir/IRContext.hpp"
#include "middle-end/ir/User.hpp"
#include "middle-end/builder/IRBuilder.hpp"
#include "middle-end/output/IRPrinter.hpp"
#include "middle-end/output/IRVerifier.hpp"

#include <initializer_list>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sstream>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Exception, typename Action>
void expectException(Action action, const char* message) {
    try {
        action();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

// 只用于验证 IR 核心关系，不是已实现的 AddInst，也不执行运算。
class TestUser final : public ir::User {
public:
    TestUser(const ir::Type* type, std::initializer_list<ir::Value*> operands)
        : User(type) {
        for (ir::Value* operand : operands) addOperand(operand);
    }

    using User::addOperand;
    using User::eraseOperand;
};

void testTypeIdentityAndValidation() {
    ir::IRContext context;
    ir::IRContext other;
    const ir::Type* i32 = context.getInt32Type();
    const ir::Type* pointer = context.getPointerType(i32);
    const ir::Type* array = context.getArrayType(i32, 10);
    const ir::Type* function = context.getFunctionType(i32, {i32, pointer});
    check(pointer == context.getPointerType(i32), "Pointer types must be interned");
    check(array == context.getArrayType(i32, 10), "Array types must be interned");
    check(function == context.getFunctionType(i32, {i32, pointer}),
          "Function types must be interned");
    check(array != context.getArrayType(i32, 11), "Array length is part of type");
    check(function->toString() == "i32 (i32, i32*)", "Function type spelling");
    check(context.getPointerType(array)->toString() == "[10 x i32]*",
          "Array address must preserve the array type");
    expectException<std::invalid_argument>([&] {
        context.getArrayType(other.getInt32Type(), 10);
    }, "Cross-context types must be rejected");
    expectException<std::invalid_argument>([&] {
        context.getFunctionType(i32, {array});
    }, "Array parameters must be lowered to pointers");
    expectException<std::invalid_argument>([&] {
        context.getPointerType(context.getVoidType());
    }, "Typed void pointers must be rejected");
    expectException<std::logic_error>([&] { array->getBitWidth(); },
                                     "Invalid type query must fail");
}

void testConstantBitPatterns() {
    ir::IRContext context;
    auto* minusOne = context.getInt(context.getInt32Type(), -1);
    check(minusOne == context.getInt(context.getInt32Type(), 4294967295LL),
          "Equivalent i32 bit patterns must share a constant");
    check(minusOne->getSignedValue() == -1, "i32 sign extension");
    auto* byte = context.getInt(context.getInt8Type(), 511);
    check(byte->getBits() == 255 && byte->getSignedValue() == -1,
          "i8 constants must retain only eight bits");
    check(context.getInt(context.getInt1Type(), 3)->getBits() == 1,
          "i1 truncation");
    check(byte != minusOne, "Constant identity includes its type");
}

void testReplacingAndRemovingUses() {
    ir::IRContext context;
    auto* three = context.getInt(context.getInt32Type(), 3);
    auto* five = context.getInt(context.getInt32Type(), 5);
    auto sum = std::make_unique<TestUser>(context.getInt32Type(),
                                        std::initializer_list<ir::Value*>{three, three});
    TestUser next(context.getInt32Type(), {sum.get(), three});
    check(three->getUseCount() == 3, "Repeated operands are separate uses");

    for (const ir::Use* use : three->getUses()) {
        check(use->getUser()->getOperand(use->getOperandIndex()) == three,
              "Use must identify its exact operand slot");
    }
    sum->setOperand(0, five);
    check(three->getUseCount() == 2 && five->getUseCount() == 1,
          "setOperand must update both use lists");

    three->replaceAllUsesWith(five);
    check(three->useEmpty() && five->getUseCount() == 3,
          "RAUW must update every use, including repeated operands");
    check(sum->getOperand(0) == five && sum->getOperand(1) == five
              && next.getOperand(1) == five,
          "RAUW must change the actual operands");

    sum->replaceAllUsesWith(five);
    check(sum->useEmpty() && next.getOperand(0) == five,
          "Instruction-like results must also support RAUW");
    sum.reset();
    check(five->getUseCount() == 2, "Deleting a User must unregister operands");

    expectException<std::invalid_argument>([&] {
        five->replaceAllUsesWith(context.getInt(context.getInt8Type(), 5));
    }, "RAUW must reject type mismatches");
    check(five->getUseCount() == 2, "Failed RAUW must not change use lists");
    five->replaceAllUsesWith(five);
    check(five->getUseCount() == 2, "Self replacement must be a no-op");
    next.dropAllReferences();
    check(five->useEmpty(), "Dropping references must unregister every operand");
}

void testStableUseAddressesAndOperandRemoval() {
    ir::IRContext context;
    auto* value = context.getInt(context.getInt32Type(), 1);
    auto* replacement = context.getInt(context.getInt32Type(), 2);
    TestUser user(context.getInt32Type(), {value});
    ir::Use* firstUse = *value->getUses().begin();
    for (int i = 0; i < 100; ++i) user.addOperand(value);
    check(value->getUses().count(firstUse) == 1,
          "Operand growth must preserve Use addresses");
    user.eraseOperand(0);
    check(value->getUseCount() == 100, "Erasing an operand must unregister it");
    for (const ir::Use* use : value->getUses()) {
        check(use->getOperandIndex() < user.getOperandCount()
                  && user.getOperand(use->getOperandIndex()) == value,
              "Operand removal must update remaining indices");
    }
    value->replaceAllUsesWith(replacement);
    check(value->useEmpty() && replacement->getUseCount() == 100,
          "RAUW must work after operand storage grows and shrinks");

    ir::IRContext other;
    expectException<std::invalid_argument>([&] {
        user.setOperand(0, other.getInt(other.getInt32Type(), 1));
    }, "Cross-context operands must be rejected");
    expectException<std::invalid_argument>([&] { user.addOperand(nullptr); },
                                          "Null operands must be rejected");
}

void testCyclicTeardown() {
    ir::IRContext context;
    TestUser first(context.getInt32Type(), {});
    TestUser second(context.getInt32Type(), {&first});
    first.addOperand(&second);
    // 模拟将来的 phi 回边：整个图先断开引用，再按任意顺序销毁节点。
    first.dropAllReferences();
    second.dropAllReferences();
    check(first.useEmpty() && second.useEmpty(), "Cyclic teardown must be safe");
}

void testStructuredIRAndVerifier() {
    ir::IRContext context;
    ir::Module module(context);
    ir::Function* function = module.addFunction("main", context.getInt32Type(), {});
    ir::BasicBlock* entry = function->createBlock();
    ir::IRBuilder builder(context);
    builder.setBlock(entry);
    ir::Value* address = builder.createAlloca(context.getInt32Type());
    builder.createStore(context.getInt(context.getInt32Type(), 3), address);
    ir::Value* value = builder.createLoad(address);
    auto* sum = static_cast<ir::Instruction*>(builder.createBinary(
        ir::Opcode::Add, value, context.getInt(context.getInt32Type(), 2)));
    builder.createReturn(sum);
    ir::IRVerifier().verify(module);
    sum->replaceAllUsesWith(context.getInt(context.getInt32Type(), 5));
    entry->erase(sum);
    ir::IRVerifier().verify(module);
    std::ostringstream text;
    ir::IRPrinter().print(module, text);
    check(text.str().find("ret i32 5") != std::string::npos, "Printer must reflect IR replacement");
    expectException<std::logic_error>([&] { builder.createReturn(); }, "Cannot insert after terminator");

    ir::Module invalid(context);
    ir::Function* other = invalid.addFunction("invalid", context.getInt32Type(), {});
    builder.setBlock(other->createBlock());
    builder.createReturn();
    expectException<std::logic_error>([&] { ir::IRVerifier().verify(invalid); }, "Verifier must reject missing return values");
}

void testPhiAndDominance() {
    ir::IRContext context;
    ir::Module module(context);
    auto* function = module.addFunction("main", context.getInt32Type(), {});
    auto* entry = function->createBlock();
    auto* left = function->createBlock();
    auto* right = function->createBlock();
    auto* end = function->createBlock();
    ir::IRBuilder builder(context);
    builder.setBlock(entry);
    builder.createBranch(context.getInt(context.getInt1Type(), 1), left, right);
    builder.setBlock(left);
    auto* value = builder.createBinary(ir::Opcode::Add, context.getInt(context.getInt32Type(), 1),
                                      context.getInt(context.getInt32Type(), 2));
    builder.createBranch(end);
    builder.setBlock(right);
    builder.createBranch(end);
    builder.setBlock(end);
    auto* phi = builder.createPhi(context.getInt32Type());
    phi->addIncoming(value, left);
    phi->addIncoming(context.getInt(context.getInt32Type(), 0), right);
    builder.createReturn(phi);
    ir::IRVerifier().verify(module);
    phi->setOperand(0, context.getInt(context.getInt8Type(), 1));
    expectException<std::logic_error>([&] { ir::IRVerifier().verify(module); }, "Verifier must check phi types");
    phi->setOperand(0, value);
    // 返回值直接使用只在 left 定义的值，right 路径上没有定义，应当拒绝。
    end->getTerminator()->setOperand(0, value);
    expectException<std::logic_error>([&] { ir::IRVerifier().verify(module); }, "Verifier must check dominance");
}

} // namespace

int main() {
    try {
        testTypeIdentityAndValidation();
        testConstantBitPatterns();
        testReplacingAndRemovingUses();
        testStableUseAddressesAndOperandRemoval();
        testCyclicTeardown();
        testStructuredIRAndVerifier();
        testPhiAndDominance();
        std::cout << "IR core: 7 scenarios passed\n";
    } catch (const std::exception& error) {
        std::cerr << "IR core test failed: " << error.what() << '\n';
        return 1;
    }
}
