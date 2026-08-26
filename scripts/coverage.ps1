# 代码覆盖率生成脚本（Windows MinGW + gcov）
# 使用方法：在项目根目录运行 .\scripts\coverage.ps1

$ErrorActionPreference = "Stop"

$BuildDir = "build_coverage"
$QtPath = "F:/Qt/6.11.0/mingw_64"

Write-Host "=== 代码覆盖率生成 ===" -ForegroundColor Cyan

# 1. 清理并创建构建目录
if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null

# 2. 配置CMake（启用覆盖率）
Write-Host "[1/4] 配置CMake..." -ForegroundColor Yellow
cmake -G "MinGW Makefiles" `
    -DCMAKE_PREFIX_PATH="$QtPath" `
    -DCMAKE_BUILD_TYPE=Debug `
    -DENABLE_COVERAGE=ON `
    -S . -B $BuildDir

# 3. 构建
Write-Host "[2/4] 构建项目..." -ForegroundColor Yellow
cmake --build $BuildDir --parallel

# 4. 运行测试（生成gcov数据）
Write-Host "[3/4] 运行单元测试..." -ForegroundColor Yellow
Push-Location $BuildDir
ctest --output-on-failure
Pop-Location

# 5. 生成覆盖率报告
Write-Host "[4/4] 生成覆盖率报告..." -ForegroundColor Yellow
$GcdaFiles = Get-ChildItem -Path $BuildDir -Recurse -Filter "*.gcda"
if ($GcdaFiles.Count -eq 0) {
    Write-Host "警告: 未找到.gcda文件，覆盖率数据可能未生成" -ForegroundColor Red
    exit 1
}

# 使用gcov生成报告
$CoverageDir = "$BuildDir/coverage_report"
New-Item -ItemType Directory -Force -Path $CoverageDir | Out-Null

Push-Location $CoverageDir
foreach ($Gcda in $GcdaFiles) {
    $Gcno = $Gcda.FullName -replace '\.gcda$', '.gcno'
    if (Test-Path $Gcno) {
        gcov -b -c $Gcda.FullName 2>&1 | Out-Null
    }
}
Pop-Location

# 统计结果
$GcovFiles = Get-ChildItem -Path $CoverageDir -Filter "*.gcov"
$TotalLines = 0
$CoveredLines = 0
foreach ($File in $GcovFiles) {
    $Content = Get-Content $File.FullName
    foreach ($Line in $Content) {
        if ($Line -match '^\s*(\d+):') {
            $Count = [int]$Matches[1]
            $TotalLines++
            if ($Count -gt 0) { $CoveredLines++ }
        }
    }
}

if ($TotalLines -gt 0) {
    $Percent = [math]::Round(($CoveredLines / $TotalLines) * 100, 2)
    Write-Host "`n=== 覆盖率统计 ===" -ForegroundColor Green
    Write-Host "总行数: $TotalLines"
    Write-Host "已覆盖: $CoveredLines"
    Write-Host "覆盖率: $Percent%"
}

Write-Host "`n覆盖率报告已生成到: $CoverageDir" -ForegroundColor Cyan
Write-Host "提示: 如需HTML报告，可安装lcov后运行 genhtml" -ForegroundColor Gray
