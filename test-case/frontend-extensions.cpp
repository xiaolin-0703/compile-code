#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <variant>
#include "../front-end/Lexer.hpp"
#include "../front-end/Parser.hpp"

namespace {
int checks = 0;
void check(bool ok, const std::string& message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
CompUnit program(const std::string& source, LexerMode mode = LexerMode::CSubset) {
    Lexer lexer(source, mode);
    check(lexer.getErrors().empty(), "unexpected lexical errors: " + source);
    Parser parser(lexer.getTokens());
    auto ast = parser.parse();
    check(parser.getErrors().empty(), "unexpected syntax errors: " + source);
    check(!parser.getOutput().empty() && parser.getOutput().back() == "<CompUnit>",
          "incomplete parser output");
    return ast;
}
CompUnit body(const std::string& source) { return program("int main(){" + source + "}"); }
template<class T> T& node(BlockItemPtr& item) {
    auto* p = dynamic_cast<T*>(item.get());
    check(p != nullptr, "unexpected statement node");
    return *p;
}
template<class T> T& expr(ExprPtr& item) {
    auto* p = dynamic_cast<T*>(item.get());
    check(p != nullptr, "unexpected expression node");
    return *p;
}
void rejects(const std::string& source) {
    bool rejected = false;
    try {
        Lexer lexer(source, LexerMode::CSubset);
        Parser parser(lexer.getTokens());
        parser.parse();
        rejected = !parser.getErrors().empty() || !lexer.getErrors().empty();
    } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "invalid syntax accepted: " + source);
}
std::string read(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) throw std::runtime_error("cannot read fixture: " + file.string());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void lexerTests() {
    using T = TokenType;
    Lexer lexer("++ -- += -= *= /= %= &= |= ^= <<= >>= << >> & | ^ ~ ?", LexerMode::CSubset);
    const std::vector<T> expected = {T::INC,T::DEC,T::PLUSASSIGN,T::MINUSASSIGN,
        T::MULTASSIGN,T::DIVASSIGN,T::MODASSIGN,T::ANDASSIGN,T::ORASSIGN,T::XORASSIGN,
        T::SHLASSIGN,T::SHRASSIGN,T::SHL,T::SHR,T::BITAND,T::BITOR,T::BITXOR,T::BITNOT,T::QUEST};
    check(lexer.getErrors().empty(), "operators must be legal in C subset mode");
    check(lexer.getTokens().size() == expected.size() + 1, "longest-match token count");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        check(lexer.getTokens()[i].getType() == expected[i], "operator longest match");
        check(tokenTypeToString(expected[i]) != "UNKNOWN", "missing token output mapping");
    }
    Lexer course("a & b | c");
    check(course.getErrors().size() == 2, "retain course error a for single & and |");
    check(course.getTokens()[1].getType() == T::AND, "retain course recovery token");
    Lexer numbers("0 42 0x2A 052 42U 42LL", LexerMode::CSubset);
    for (std::size_t i = 1; i < 6; ++i)
        check(std::get<int>(numbers.getTokens()[i].getValue()) == 42, "integer literal base/suffix");
    Lexer strings("\"++ -= for do\" // ++\n /* -- */ i++", LexerMode::CSubset);
    check(strings.getTokens().size() == 4 && strings.getTokens()[0].getType() == T::STRCON &&
          strings.getTokens()[2].getType() == T::INC, "comments and strings are atomic");
    for (const auto* literal : {"09", "0x", "12abc", "1uuu"}) {
        Lexer invalid(literal, LexerMode::CSubset);
        check(invalid.getTokens()[0].getType() == T::INVALID, "invalid integer literal");
    }
}

void expressionTests() {
    auto ast = body("i++; ++i; i--; --i; i+=2; i-=3; i*=4; i/=2; i%=3;");
    auto& items = ast.mainFunction.body.blockItems;
    for (std::size_t i = 0; i < 4; ++i) {
        auto& update = expr<UpdateExpr>(node<ExprStmt>(items[i]).expression);
        check(update.isPrefix == (i % 2 == 1), "prefix/postfix must be retained");
        check(update.op.getType() == (i < 2 ? TokenType::INC : TokenType::DEC), "update op");
    }
    check(node<AssignStmt>(items[4]).op.getType() == TokenType::PLUSASSIGN, "compound assignment AST");
    auto chain = body("a=b=c=1;");
    auto& first = node<AssignStmt>(chain.mainFunction.body.blockItems[0]);
    auto& second = expr<AssignmentExpr>(first.value);
    check(second.op.getType() == TokenType::ASSIGN, "chained assignment");
    expr<AssignmentExpr>(second.value);
    auto precedence = body("x=1+2*3<<1<9==0&7^2|1&&a||b;");
    auto& root = expr<BinaryExpr>(node<AssignStmt>(precedence.mainFunction.body.blockItems[0]).value);
    check(root.op.getType() == TokenType::OR, "logical OR is outermost");
    check(expr<BinaryExpr>(root.left).op.getType() == TokenType::AND, "logical AND precedence");
    auto bitOrder = body("x=1|2^3&4==5<6<<1+2*3;");
    Expr* cursor = node<AssignStmt>(bitOrder.mainFunction.body.blockItems[0]).value.get();
    for (auto op : {TokenType::BITOR, TokenType::BITXOR, TokenType::BITAND, TokenType::EQL,
                    TokenType::LSS, TokenType::SHL, TokenType::PLUS, TokenType::MULT}) {
        auto* binary = dynamic_cast<BinaryExpr*>(cursor);
        check(binary && binary->op.getType() == op, "C precedence chain");
        cursor = binary->right.get();
    }
    auto ternary = body("x=a?b:c?d:e;");
    auto& choice = expr<ConditionalExpr>(node<AssignStmt>(ternary.mainFunction.body.blockItems[0]).value);
    expr<ConditionalExpr>(choice.elseExpr);
    auto comma = body("f(a++,b+=2,(a,b)); a=1,b=2;");
    auto& call = expr<CallExpr>(node<ExprStmt>(comma.mainFunction.body.blockItems[0]).expression);
    check(call.arguments.size() == 3, "argument separators must not be comma operators");
    check(expr<BinaryExpr>(call.arguments[2]).op.getType() == TokenType::COMMA, "parenthesized comma operator");
    check(expr<BinaryExpr>(node<ExprStmt>(comma.mainFunction.body.blockItems[1]).expression).op.getType()
          == TokenType::COMMA, "comma after assignment");
    auto cComma = body("a,b=1,2;");
    auto& outerComma = expr<BinaryExpr>(node<ExprStmt>(cComma.mainFunction.body.blockItems[0]).expression);
    auto& innerComma = expr<BinaryExpr>(outerComma.left);
    auto& onlyAssignment = expr<AssignmentExpr>(innerComma.right);
    check(expr<LValExpr>(onlyAssignment.target).ident.getName() == "b",
          "a,b=1,2 is a C comma expression, not a parallel assignment");
    body("a[i=0]; a[i=1]+=2; a[(i++,j)]--; x=(a<b)&&(b!=c); x=a?(b=1,c):d;");
    body("x<<=1; x>>=1; x&=3; x|=2; x^=1; x=~x; (int)(a+b); f(\"text\");");
    auto once = body("a[i++] += f(); sizeof(int); sizeof a[0];");
    auto& compound = node<AssignStmt>(once.mainFunction.body.blockItems[0]);
    expr<UpdateExpr>(compound.target->index);
    expr<CallExpr>(compound.value);
    check(expr<SizeofExpr>(node<ExprStmt>(once.mainFunction.body.blockItems[1]).expression).type
          == BType::Int, "sizeof type AST");
    check(expr<SizeofExpr>(node<ExprStmt>(once.mainFunction.body.blockItems[2]).expression).operand
          != nullptr, "sizeof expression AST");
}

void loopTests() {
    auto ast = body("for(int i=0,j=9;i<j;++i,j--) {a+=i;} for(;;); while(i-- > 0) continue; do ++i; while(i<3);");
    auto& items = ast.mainFunction.body.blockItems;
    auto& counted = node<ForStmt>(items[0]);
    auto* init = dynamic_cast<VarDecl*>(counted.initializer.get());
    check(init && init->definitions.size() == 2, "for declaration initializer");
    check(counted.condition && counted.step && counted.body, "for fields must be retained");
    check(expr<BinaryExpr>(counted.step).op.getType() == TokenType::COMMA, "for comma step");
    auto& infinite = node<ForStmt>(items[1]);
    check(!infinite.initializer && !infinite.condition && !infinite.step, "for omitted clauses");
    node<WhileStmt>(items[2]);
    auto& post = node<DoWhileStmt>(items[3]);
    check(post.body && post.condition, "do-while order and fields");
    body("for(i=0;i<5;i++) while(j--) do {if(i) continue; else break;} while(k++<2);");
    for (const auto& loop : {"for(;i<3;);", "for(i=0;;);", "for(;;i++);", "for(i=0;i<3;);"}) body(loop);
    body("while((c=getchar())!=0) {c--;} do ; while(0);");
}

void declarationTests() {
    auto ast = program("int f(int, char); int global=1; int main(void){int a[][2]={{1,2},{3,4},}; a[0][1]++; return f(a[0][1], 'x');} int f(int x,char y){return x+y;}");
    check(ast.functionDeclarations.size() == 1 && ast.functions.size() == 1, "function prototypes and definitions");
    auto& array = node<VarDecl>(ast.mainFunction.body.blockItems[0]).definitions[0];
    check(array.isArray && !array.arraySize && array.extraDimensions.size() == 1, "multidimensional unsized array");
    check(std::holds_alternative<std::vector<Init>>(array.initValue->value), "nested initializer AST");
    auto& update = expr<UpdateExpr>(node<ExprStmt>(ast.mainFunction.body.blockItems[1]).expression);
    check(expr<LValExpr>(update.operand).extraIndices.size() == 1, "multidimensional indexing AST");
    program("void ping(void); int main(){ping();} int after=2; void ping(void){return;}");
    program("int sum(int a[][3]); int main(){return 0;} int sum(int a[][3]){return a[0][1];}");
    body("const int a[2][2]={{1,2},{3,4}}; int x=1,y=2; if(x) y++; else if(y) --x;");
    body("switch(x){case -1: x++; break; case 1+2: {int y=2; x+=y;} break; default: x=0;}");
}

void errorTests() {
    for (const std::string stmt : {"1++;", "++(a+b);", "f()--;", "(a+b)=2;", "a?b;", "for(;;", "do ; (1);", "for(int i=0;i<3;i++){"})
        rejects("int main(){" + stmt + "}");
    rejects("int f(int){return 0;} int main(){return 0;}");
    rejects("int main(){struct S x;}");
    rejects("int main(){ int *p; }");
    rejects("int main(){");
    rejects("");
    Lexer lexer("int main(){\nint a[2];\na[0=1;\ndo a++; while(a<3)\nreturn 0;\n}");
    Parser parser(lexer.getTokens());
    parser.parse();
    check(parser.getErrors().size() == 2 && parser.getErrors()[0].getLineNum() == 3 &&
          parser.getErrors()[0].getErrorType() == 'k' && parser.getErrors()[1].getLineNum() == 4 &&
          parser.getErrors()[1].getErrorType() == 'i', "missing bracket and do-while semicolon recovery");
}

void regressionTests() {
    const auto dir = std::filesystem::path(__FILE__).parent_path();
    for (const std::string name : {"parser-correct", "parser-missing-symbols", "parser-combined-errors"}) {
        Lexer lexer(read(dir / (name + ".sy"))); // 保持课设模式。
        Parser parser(lexer.getTokens());
        parser.parse();
        std::vector<compileError> errors = lexer.getErrors();
        errors.insert(errors.end(), parser.getErrors().begin(), parser.getErrors().end());
        std::stable_sort(errors.begin(), errors.end(), [](const auto& a, const auto& b) {
            return a.getLineNum() < b.getLineNum();
        });
        std::string actual;
        for (const auto& error : errors) actual += std::to_string(error.getLineNum()) + " " + error.getErrorType() + "\n";
        const auto expectedFile = dir / (name + ".expected-error.txt");
        check(actual == (std::filesystem::exists(expectedFile) ? read(expectedFile) : ""), "legacy regression: " + name);
        check(parser.getOutput().back() == "<CompUnit>", "legacy output completion");
        const auto parserFile = dir / (name + ".expected-parser.txt");
        if (std::filesystem::exists(parserFile)) {
            std::string actualParser;
            for (const auto& line : parser.getOutput()) actualParser += line + "\n";
            check(actualParser == read(parserFile), "legacy parser output changed: " + name);
        }
    }
}
}

int main() {
    try {
        lexerTests(); expressionTests(); loopTests(); declarationTests(); errorTests(); regressionTests();
        std::cout << checks << " frontend checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
