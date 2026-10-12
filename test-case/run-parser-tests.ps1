$ErrorActionPreference = "Stop"

$codeRoot = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $codeRoot "cmake-build-codex/Compiler.exe"
if (-not (Test-Path -LiteralPath $compiler)) {
    throw "Compiler.exe 不存在，请先构建 cmake-build-codex。"
}

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("compiler-parser-tests-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testRoot | Out-Null

function Invoke-ParserCase {
    param(
        [string]$Source,
        [scriptblock]$Assert
    )

    Copy-Item -LiteralPath $Source -Destination (Join-Path $testRoot "testfile.txt")
    Push-Location $testRoot
    try {
        & $compiler
        if ($LASTEXITCODE -ne 0) {
            throw "编译器退出码为 $LASTEXITCODE"
        }
        & $Assert
    }
    finally {
        Pop-Location
    }
}

try {
    Invoke-ParserCase (Join-Path $PSScriptRoot "parser-correct.sy") {
        $errors = Get-Content -LiteralPath (Join-Path $testRoot "error.txt")
        if ($errors.Count -ne 0) {
            throw "正确程序产生了错误：$($errors -join ', ')"
        }
        $parserOutput = Get-Content -LiteralPath (Join-Path $testRoot "parser.txt")
        if ($parserOutput.Count -eq 0 -or $parserOutput[-1] -ne "<CompUnit>") {
            throw "正确程序没有生成完整的 parser.txt。"
        }
        $requiredMarkers = @(
            "<CompUnit>", "<ConstDecl>", "<ConstDef>", "<ConstInitVal>",
            "<VarDecl>", "<VarDef>", "<InitVal>", "<FuncDef>",
            "<MainFuncDef>", "<FuncType>", "<FuncFParams>", "<FuncFParam>",
            "<Block>", "<Stmt>", "<CaseStmt>", "<Exp>", "<Cond>",
            "<LVal>", "<PrimaryExp>", "<Number>", "<UnaryExp>",
            "<UnaryOp>", "<FuncRParams>", "<MulExp>", "<AddExp>",
            "<RelExp>", "<EqExp>", "<LAndExp>", "<LOrExp>", "<ConstExp>"
        )
        foreach ($marker in $requiredMarkers) {
            if ($marker -notin $parserOutput) {
                throw "parser.txt 缺少语法成分 $marker。"
            }
        }
    }

    Invoke-ParserCase (Join-Path $PSScriptRoot "parser-missing-symbols.sy") {
        $actual = Get-Content -LiteralPath (Join-Path $testRoot "error.txt")
        $expected = Get-Content -LiteralPath (Join-Path $PSScriptRoot "parser-missing-symbols.expected-error.txt")
        if (Compare-Object $expected $actual -SyncWindow 0) {
            throw "缺符号程序的错误列表不符合预期。实际输出：$($actual -join ', ')"
        }
    }

    Invoke-ParserCase (Join-Path $PSScriptRoot "parser-combined-errors.sy") {
        $actual = Get-Content -LiteralPath (Join-Path $testRoot "error.txt")
        $expected = Get-Content -LiteralPath (Join-Path $PSScriptRoot "parser-combined-errors.expected-error.txt")
        if (Compare-Object $expected $actual -SyncWindow 0) {
            throw "词法/语法错误没有正确合并。实际输出：$($actual -join ', ')"
        }
    }

    Write-Output "Parser tests passed."
}
finally {
    $tempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    $resolvedTestRoot = [System.IO.Path]::GetFullPath($testRoot)
    if (-not $resolvedTestRoot.StartsWith($tempBase, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "拒绝清理临时目录之外的路径：$resolvedTestRoot"
    }
    Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
}
