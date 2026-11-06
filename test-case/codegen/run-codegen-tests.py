"""生成器端到端验证：源程序 -> Compiler -> LLVM 校验 -> JIT 执行。

需要 Python 与 llvmlite。只在临时目录运行，不覆盖 code/testfile.txt。
运行：python test-case/codegen/run-codegen-tests.py --compiler <Compiler.exe>
"""
import argparse
import ctypes
import io
from pathlib import Path
import subprocess
import tempfile

from llvmlite import binding as llvm


class Runtime:
    """用课程接口的 Python 回调提供输入输出，保留回调对象防止被回收。"""
    def __init__(self, data):
        self.input = io.BytesIO(data)
        self.output = bytearray()
        self.errors = []
        self.callbacks = []

    def bind(self, name, result, parameters, function):
        callback = ctypes.CFUNCTYPE(result, *parameters)(function)
        self.callbacks.append(callback)
        llvm.add_symbol(name, ctypes.cast(callback, ctypes.c_void_p).value)

    def install(self):
        def get_int():
            try:
                return int(self.input.readline().strip())
            except ValueError:
                self.errors.append("get_int: invalid test input")
                return 0

        def get_char():
            value = self.input.read(1)
            if not value:
                self.errors.append("get_char: EOF")
                return 0
            return value[0]

        def get_string(address, count):
            # 与随课 libsysy.c 一致，读取一行并去掉换行符。
            data = self.input.readline().rstrip(b"\r\n")
            if count > 0:
                data = data[:count - 1] + b"\0"
                ctypes.memmove(address, data, len(data))

        self.bind("get_int", ctypes.c_int32, [], get_int)
        self.bind("get_char", ctypes.c_uint8, [], get_char)
        self.bind("get_string", None, [ctypes.c_void_p, ctypes.c_int32], get_string)
        self.bind("put_int", None, [ctypes.c_int32],
                  lambda value: self.output.extend(str(value).encode()))
        self.bind("put_char", None, [ctypes.c_uint8],
                  lambda value: self.output.append(value))
        self.bind("put_string", None, [ctypes.c_void_p],
                  lambda address: self.output.extend(ctypes.string_at(address)))


def compile_source(compiler, folder, source):
    (folder / "testfile.txt").write_text(source, encoding="utf-8")
    result = subprocess.run([str(compiler)], cwd=folder, capture_output=True, timeout=20)
    if result.returncode:
        raise AssertionError(result.stderr.decode("utf-8", errors="replace"))
    return (folder / "error.txt").read_text(), (folder / "llvm_ir.txt").read_text()


def run_case(compiler, source, input_data, expected, runtime_text):
    with tempfile.TemporaryDirectory(prefix="compiler-codegen-") as temporary:
        errors, text = compile_source(compiler, Path(temporary), source)
        assert not errors, "Unexpected front-end errors: " + errors
        module = llvm.parse_assembly(text)
        module.verify()
        if runtime_text:
            linked = llvm.parse_assembly(text)
            llvm.link_modules(linked, llvm.parse_assembly(runtime_text))
            linked.verify()
        runtime = Runtime(input_data)
        runtime.install()
        target = llvm.Target.from_default_triple().create_target_machine()
        module.triple = llvm.get_default_triple()
        module.data_layout = str(target.target_data)
        with llvm.create_mcjit_compiler(module, target) as engine:
            engine.finalize_object()
            main = ctypes.CFUNCTYPE(ctypes.c_int32)(engine.get_function_address("main"))
            exit_code = main()
            assert exit_code == 0, f"main returned {exit_code}"
        assert not runtime.errors, runtime.errors
        actual = bytes(runtime.output).replace(b"\r\n", b"\n")
        assert actual == expected.replace(b"\r\n", b"\n"), f"Expected {expected!r}, got {actual!r}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    arguments = parser.parse_args()
    compiler = arguments.compiler.resolve()
    llvm.initialize_native_target()
    llvm.initialize_native_asmprinter()
    folder = Path(__file__).resolve().parent
    runtime_file = folder.parents[2] / "文法解读" / "libsysy.ll"
    runtime_text = runtime_file.read_text(encoding="utf-8") if runtime_file.exists() else ""
    sources = sorted(folder.glob("*.sy"))
    for source_file in sources:
        input_file = source_file.with_suffix(".in")
        try:
            run_case(compiler, source_file.read_text(encoding="utf-8"),
                     input_file.read_bytes() if input_file.exists() else b"",
                     source_file.with_suffix(".out").read_bytes(), runtime_text)
        except Exception as error:
            raise AssertionError(f"{source_file.stem}: {error}") from error
        print("PASS", source_file.stem)
    # 同一工作目录复用：发生前端错误时必须清空旧 IR。
    with tempfile.TemporaryDirectory(prefix="compiler-error-") as temporary:
        directory = Path(temporary)
        errors, text = compile_source(compiler, directory, "int main(){return 0;}")
        assert not errors and text
        errors, text = compile_source(compiler, directory, "int main(){missing=1;return 0;}")
        assert " c" in errors and text == "", "Stale IR must be cleared on front-end errors"
    print(f"PASS {len(sources)} execution cases + stale-output check")
    for source_file in sorted(folder.parent.glob("parser-*.sy")):
        with tempfile.TemporaryDirectory(prefix="compiler-frontend-") as temporary:
            directory = Path(temporary)
            errors, text = compile_source(compiler, directory, source_file.read_text(encoding="utf-8"))
            expected_errors = source_file.with_suffix(".expected-error.txt")
            # 原 golden 只列词法/语法错误；集成编译还保留既有的语义错误。
            semantic_errors = source_file.with_suffix(".expected-semantic-error.txt")
            if semantic_errors.exists():
                expected_errors = semantic_errors
            if expected_errors.exists():
                assert errors == expected_errors.read_text(encoding="utf-8"), source_file.stem + ": changed errors"
                assert not text
            else:
                assert not errors and text, source_file.stem + ": unexpected errors"
                parser_text = (directory / "parser.txt").read_text(encoding="utf-8")
                expected_parser = source_file.with_suffix(".expected-parser.txt")
                assert parser_text == expected_parser.read_text(encoding="utf-8"), "Changed parser output"
        print("PASS frontend regression", source_file.stem)


if __name__ == "__main__":
    main()
