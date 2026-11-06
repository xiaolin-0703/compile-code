# FuncStmt 与 elif 文法测试

测试源码：`grammar-funcstmt-elif.sy`。

## 覆盖范围

| 测试内容 | 源码中的例子 |
| --- | --- |
| 带参函数声明 | `int classify(int x);` |
| 无参函数声明 | `void ping();` |
| 声明与定义交替出现 | classify 声明 → ping 定义 → ping 声明 → classify 定义 |
| 多个 elif，且有 else | classify 函数 |
| 没有 elif，没有 else | main 中第一个 if |
| 一个 elif，没有 else | main 中第二个 if |
| 条件分支的不同结果 | classify(-3)、classify(0)、classify(5)、classify(12) |

`ping` 在定义后再出现一次声明是有意安排的，符合 `{FuncDef | FuncStmt}` 的语法，用于检查顶层解析是否允许交替出现。

## 预期输出文件

| 文件 | 含义 |
| --- | --- |
| `grammar-funcstmt-elif.expected-error.txt` | 空文件，正确程序不产生词法或语法错误 |
| `grammar-funcstmt-elif.expected-lexer.txt` | 完整词法分析输出，共 148 行 |
| `grammar-funcstmt-elif.expected-parser.txt` | 完整语法分析输出，共 366 行 |
| `grammar-funcstmt-elif.expected-stdout.txt` | 测试程序将来被执行时的标准输出 |

关键检查项：

- lexer 输出中有 3 行 `ELIFTK elif`。
- parser 输出中有 2 行 `<FuncStmt>`。
- parser 输出中有 2 行 `<FuncDef>`。
- parser 最后一个非空行是 `<CompUnit>`。
- 不应额外输出 `<Elif>` 标签。

测试程序将来被执行时，预期标准输出为：

```text
ready
-1 0 1 2
if only
elif only
```

当前 Compile.cpp 只执行词法和语法分析，不会执行测试源码中的 printf。因此运行 Compiler.exe 时，应对照 lexer.txt、parser.txt、error.txt；控制台不应被要求出现上面的四行。

## 当前源码中需要先修正的两处问题

这次只添加测试文件，没有改动编译器源码，也没有覆盖原 testfile.txt。

### 1. 函数声明分支没有消费分号

位置：front-end/Parser.hpp 的 parseFuncDef()，构造 FuncStmt 的 else 分支。

在构造节点之前增加：

```cpp
expect(TokenType::SEMICN);
```

否则解析第一行 `int classify(int x);` 后，当前 token 仍是分号，顶层函数循环提前结束，并报 `missing main function`，还没有到写输出文件的阶段。

### 2. IfStmt 构造函数缺少分支列表的参数名

位置：front-end/AST/Stmt.hpp。

目前 `std::vector<elifBranch>` 参数没有名字，初始化列表中的 `elseifBranches` 因而引用了尚未完成初始化的成员自身。

可改为：

```cpp
IfStmt(ExprPtr condition, StmtPtr thenBranch,
       std::vector<elifBranch> elifBranches,
       StmtPtr elseBranch = nullptr)
    : condition(std::move(condition)),
      thenBranch(std::move(thenBranch)),
      elseifBranches(std::move(elifBranches)),
      elseBranch(std::move(elseBranch)) {}
```

## 验证方式与结果

- 使用当前源码重新编译，在捕获异常的测试驱动中运行：得到 `missing main function`。
- 仅在临时源码副本中修正上述两处，再编译运行：成功生成空 error.txt 和完整 lexer.txt、parser.txt，已保存为本目录的预期输出。
- 同时检查 AST：四个顶层函数项依次为 FuncStmt、FuncDef、FuncStmt、FuncDef；三处 IfStmt 的 elif 数量依次为 2、0、1；最终 else 的存在情况也符合测试源码。
- 标准输出通过临时副本验证：把语法位置的 elif 改为标准 C++ 的 else if，添加 cstdio 后编译执行，输出与 expected-stdout.txt 一致。此步骤用于核对程序逻辑，不表示当前编译器已经实现代码生成。

## 在当前项目里使用

1. 将测试源码的内容复制到项目 code 目录下的 testfile.txt。
2. 修正上述两处后，重新构建并运行 Compiler。
3. 当前程序读取 `../testfile.txt`；从 cmake-build-debug 目录运行时，输入对应 code/testfile.txt。
4. 在运行工作目录中查看 lexer.txt、parser.txt、error.txt，与预期文件对照。

error.txt 为空是正确结果，不代表没有生成文件。
