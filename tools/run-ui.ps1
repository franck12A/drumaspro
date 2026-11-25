param(
    [switch]$BuildOnly
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cmake = 'D:\visualcos\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$qtBin = Join-Path $repositoryRoot '.tools\Qt\6.8.3\msvc2022_64\bin'
$windeployqt = Join-Path $qtBin 'windeployqt.exe'
$qmlDirectory = Join-Path $repositoryRoot 'src\ui\qt'
$application = Join-Path $repositoryRoot 'build\vs2026\src\ui\qt\Release\drumaspro.exe'

if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
    throw "CMake executable not found: $cmake"
}
if (-not (Test-Path -LiteralPath $qtBin -PathType Container)) {
    throw "Qt 6.8.3 bin directory not found: $qtBin"
}
if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
    throw "Qt deployment tool not found: $windeployqt"
}

Push-Location $repositoryRoot
try {
    & $cmake --preset vs2026 -DDRUMAS_BUILD_UI=ON
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }

    & $cmake --build --preset vs2026-release --target drumaspro_qt --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "UI build failed with exit code $LASTEXITCODE"
    }

    if (-not (Test-Path -LiteralPath $application -PathType Leaf)) {
        throw "UI executable not found: $application"
    }

    & $windeployqt --qmldir $qmlDirectory --no-translations $application
    if ($LASTEXITCODE -ne 0) {
        throw "Qt deployment failed with exit code $LASTEXITCODE"
    }

    $env:PATH = "$qtBin;$env:PATH"
    if ($BuildOnly) {
        Write-Output "UI is ready to launch: $application"
        return
    }

    & $application
    if ($LASTEXITCODE -ne 0) {
        throw "UI exited with code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
