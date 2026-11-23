# LLVM IR 与 MIPS 代码生成

编译器使用 C++17，从工作目录读取 `testfile.txt`，生成提交用的 `mips.txt` 和调试用的 `llvm_ir.txt`。源程序有前端错误时，编译器写入 `error.txt`，清空两个代码文件。运行时输入由 MARS 执行汇编时读取。

IR 实现全部放在对应的 .hpp 中。类内成员定义隐式 inline；需要在类外定义的函数显式 inline。Value 与 Use 的完整定义集中在 Value.hpp，Use.hpp 保留独立的包含入口，以避免循环包含。

中端与后端都采用头文件实现。新增指令可以在对应的 `.hpp` 中同时写声明和实现，由使用处包含。

成员变量使用普通驼峰命名，不添加末尾下划线；与参数同名时通过 this-> 区分。代码中的 `= delete` 表示禁止调用对应函数：这里禁止 Type、Value、Use 和 IRContext 的复制/移动，以保持类型身份及 IR 引用地址稳定；它不同于释放动态内存的 `delete pointer`。

## 1. 生成中间代码的逻辑

```text
AST + SemanticModel
        ↓ IRGenerator：递归访问 AST，决定要生成什么操作
IRBuilder：创建指令、放进当前 BasicBlock
        ↓
Module → Function → BasicBlock → Instruction
        ↓ IRPrinter：遍历并打印
LLVM IR 文本
```

以源代码 `int a = 3; return a + 2;` 为例：

```llvm
%a.addr = alloca i32
store i32 3, i32* %a.addr
%v0 = load i32, i32* %a.addr
%v1 = add i32 %v0, 2
ret i32 %v1
```

生成过程是：

1. 声明 a 时创建 AllocaInst，结果类型为 i32*，把地址绑定到 a 的 SymbolId。
2. 为初始值 3 取得 ConstantInt，类型为 i32；创建 StoreInst 写到 a 的地址。
3. 处理 a + 2 时，递归处理左边 a：查地址并创建 LoadInst，得到 i32 的值。
4. 递归处理右边 2：取得 i32 的 ConstantInt。
5. 创建 BinaryInst(Opcode::Add)，两个操作数引用左值和常量 2，其结果类型为 i32。
6. ReturnInst 引用该加法指令，打印器最终输出上述文本。

这里的 %a.addr、%v0、%v1 是对象的打印名称，指令之间实际保存对象引用。

表达式生成采用递归下降；以下是已实现接口的简化说明，完整分派见 generation/IRGenerator.hpp：

```cpp
Value* IRGenerator::emitExpr(const Expr& expr) {
    if (是整数字面量) {
        return context.getInt(context.getInt32Type(), 字面量数值);
    }
    if (是变量读取) {
        Value* address = emitAddress(变量引用);
        return builder.createLoad(address);
    }
    if (是加法表达式) {
        Value* lhs = emitExpr(左表达式);
        Value* rhs = emitExpr(右表达式);
        // 本课程同型 char 运算保留 i8，不按普通 C 的 int 提升规则处理。
        return builder.createBinary(Opcode::Add, lhs, rhs);
    }
    // 继续处理其他表达式。
}
```

赋值左侧通过 emitAddress 求地址，右侧通过 emitExpr 求值。&&、|| 另用 emitCondition 生成短路分支，不能机械地先求完左右两边。

## 2. Type 描述什么

Type 描述 IR 运算和存储的类型，独立于 AST 中的 BasicType/ExprType。

| 对象 | Type |
| --- | --- |
| 整数常量 3 | i32 |
| 比较结果 | i1 |
| char 的存储值 | i8 |
| int 变量地址 | i32* |
| int a[10] 的数组对象类型 | [10 x i32] |
| 上述数组对象的地址 | [10 x i32]* |
| int 数组形参的元素指针 | i32* |
| 无结果的 store 指令 | void |

当前采用一个 Type 类加 TypeKind 标签，避免第一版为每种类型建立大量小类。所有类型通过 IRContext 工厂获取；类型创建后不可修改，在同一个 Context 内可以通过 Type* 相等判断类型相同。

```cpp
#include "middle-end/ir/IRContext.hpp"
#include "middle-end/ir/Constant.hpp"
#include <iostream>

int main() {
    ir::IRContext context;
    const ir::Type* i32 = context.getInt32Type();
    const ir::Type* addressType = context.getPointerType(i32);
    const ir::Type* arrayType = context.getArrayType(i32, 10);
    const ir::Type* functionType = context.getFunctionType(i32, {i32});
    ir::ConstantInt* three = context.getInt(i32, 3);

    std::cout << addressType->toString() << '\n';   // i32*
    std::cout << arrayType->toString() << '\n';     // [10 x i32]
    std::cout << functionType->toString() << '\n';  // i32 (i32)
    std::cout << three->getType()->toString()       // i32 3
              << ' ' << three->getSignedValue() << '\n';
}
```

当前工厂支持课程所需的 i1/i8/i32、void、label、带类型指针、数组和固定参数函数。函数的数组形参先转成指针；不同 Context 的类型不能混用。类型文本中的带类型指针写法，后续由 IRPrinter 配合课程工具链输出。

## 3. Value 描述什么

Value 是“可被引用的 IR 对象”的基类。常量、函数参数、变量地址和指令结果都可以是 Value；它不是源程序变量的另一份符号表，也不是物理寄存器。

```text
Value
├── ConstantInt               已实现
├── Argument                  已实现
├── GlobalVariable            已实现
├── Function / BasicBlock     已实现
└── User                      已实现
    └── Instruction           已实现
        ├── AllocaInst
        ├── LoadInst
        ├── StoreInst
        ├── BinaryInst        用 Opcode 区分 Add/Sub/Mul/除法/取模
        └── ReturnInst
```

Value 保存类型、可读名字提示和 Use 集合。User 继承 Value，额外持有操作数；每个操作数位置由一个 Use 表示。

例如 `%v1 = add %v0, %v0`：对应的 BinaryInst 是一个 User，也是一个结果类型为 i32 的 Value；它持有两个 Use，两个 Use 都指向 %v0。%v0 的使用集合中必须记录两个位置，不能只记录一个使用者。

常量折叠将某条计算替换成常量时，可调用 `instruction->replaceAllUsesWith(constant)`。这一步会同步修改使用者的操作数和新旧值的使用集合。删除该指令仍需另行判断副作用，并由后续 BasicBlock 的删除接口执行。

当前 Value 没有 setType：需要转换类型时应当建立 CastInst，而不是原地改变已有值的类型。setName 也不改变对象身份；最终打印编号交给 IRPrinter。

## 4. 生命周期与当前验证

IRContext 拥有 Type 和 ConstantInt。Module/Function/BasicBlock 分层拥有其他对象；Use 和操作数指针不拥有被引用的 Value。Context 必须最后销毁。

User 的操作数使用 vector<unique_ptr<Use>>，以保证扩容时 Use 地址稳定。Value 和 Use 禁止复制、移动。批量销毁有回边的 IR 图时，先调用所有 User 的 dropAllReferences，再释放定义；Value 的 Debug 析构断言可发现仍有使用者的错误删除。

ConstantInt 保存整数位模式：getBits 取得无符号位模式，getSignedValue 取得有符号解释。例如 i8 的 255 和 -1 是同一位模式；这并不决定语言层面的 char 符号规则，扩展指令要按语言语义选择。

测试位于 `test-case/ir/ir-core-tests.cpp`，覆盖类型/常量复用、重复使用、整体替换、操作数扩容和删除、跨 Context 误用、循环引用的批量断开。

当前结构测试还有完整 IR 构造、替换/删除指令、phi 类型和 SSA 支配关系验证。test-case/codegen/run-codegen-tests.py 执行 12 个源程序，检查 LLVM 合法性、课程运行库链接和实际输出。

## 5. 从 LLVM IR 到 MIPS

`MipsLowering` 遍历校验后的 `ir::Module`，为算术、访存、跳转和调用选择 MIPS 指令。中间结果由虚拟寄存器表示，局部对象和调用参数先保留为抽象栈位置。LLVM 文本由独立的 `IRPrinter` 从同一个模块生成。

- **数据和数组。** 全局变量与字符串进入 `.data`。函数的 `alloca` 对象放进当前栈帧。`getelementptr` 按元素大小缩放下标，`char` 使用 `lbu/sb`，`int` 使用 `lw/sw`。
- **控制流。** 基本块对应汇编标签。`phi` 在进入目标块的边上复制，先暂存所有来源，再写入目标，避免回边上的复制互相覆盖。
- **函数调用。** 前四个实参使用 `$a0–$a3`，其余放入调用者的栈帧，返回值用 `$v0`。调用其他函数的普通函数保存 `$ra`，因此递归每一层都有独立的局部对象和返回地址。课程 I/O 函数映射到 MARS 系统调用。
- **栈布局。** 每个虚拟寄存器使用独立的栈槽。`MipsPrinter` 在输出指令时加载所需的中间值，并将结果写回对应栈槽。它也负责计算局部对象和栈上传递参数的偏移，并展开超出 16 位立即数范围的地址。

在 `code` 目录下用 PowerShell 编译和运行。

```powershell
g++ -std=c++17 -O2 -I. -Ifront-end Compile.cpp front-end/Lexer.cpp -o Compiler.exe
.\Compiler.exe
java -jar ../mars-2026-BUAA.jar --no-copyright --messages-to-stderr mips.txt
```

MIPS 回归测试在独立的临时目录中生成并执行汇编，将程序的标准输出与预期结果比较。

```powershell
python test-case/mips/run-mips-tests.py --compiler .\Compiler.exe --mars ../mars-2026-BUAA.jar
```
