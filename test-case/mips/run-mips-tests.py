"""Compile the regression programs and run their MIPS in the course simulator."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def compile_source(compiler, workdir, source):
    (workdir / "testfile.txt").write_bytes(source.read_bytes())
    result = subprocess.run([compiler], cwd=workdir, stdin=subprocess.DEVNULL,
                            capture_output=True, timeout=20)
    assert result.returncode == 0, (source.name, result.stderr)
    return (workdir / "error.txt").read_bytes().replace(b"\r\n", b"\n")


def run_mars(jar, workdir, input_data):
    result = subprocess.run(
        ["java", "-jar", jar, "--no-copyright", "--messages-to-stderr",
         "--warnings-are-errors", "--assemble-error-exit-code=2",
         "--simulate-error-exit-code=3", "--max-steps=1000000", "mips.txt"],
        cwd=workdir, input=input_data, capture_output=True, timeout=20,
    )
    assert result.returncode == 0, result.stderr.decode(errors="replace")
    return result.stdout.replace(b"\r\n", b"\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--mars", type=Path, required=True)
    args = parser.parse_args()
    compiler, jar = args.compiler.resolve(), args.mars.resolve()
    folder = Path(__file__).resolve().parent
    cases = sorted((folder.parent / "codegen").glob("*.sy")) + sorted(folder.glob("*.sy"))
    with tempfile.TemporaryDirectory(prefix="compiler-mips-") as directory:
        workdir = Path(directory)
        assembly = workdir / "mips.txt"
        for case in cases:
            infile = case.with_suffix(".in")
            input_data = infile.read_bytes() if infile.exists() else b""
            expected = case.with_suffix(".out").read_bytes().replace(b"\r\n", b"\n")
            assert not compile_source(compiler, workdir, case), case.name
            assert assembly.stat().st_size > 0, case.name
            actual = run_mars(jar, workdir, input_data)
            assert actual == expected, (case.name, expected, actual)
            print("PASS", case.stem)

        malformed = folder.parent / "parser-combined-errors.sy"
        errors = compile_source(compiler, workdir, malformed)
        assert errors and assembly.read_bytes() == b"", (malformed.name, errors)
        print(f"PASS {len(cases)} MARS cases + stale-output check")


if __name__ == "__main__":
    main()
