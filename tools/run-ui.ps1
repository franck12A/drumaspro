param(
    [switch]$BuildOnly,
    [string]$Preset = 'vs2026'
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$qtBin = Join-Path $repositoryRoot '.tools\Qt\6.8.3\msvc2022_64\bin'
$windeployqt = Join-Path $qtBin 'windeployqt.exe'
$qmlDirectory = Join-Path $repositoryRoot 'src\ui\qt'
$application = Join-Path $repositoryRoot "build\$Preset\src\ui\qt\Release\drumaspro.exe"

$cmake = $null
$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($null -ne $cmakeCommand) {
    $cmake = $cmakeCommand.Source
}
if ([string]::IsNullOrWhiteSpace($cmake)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installation = & $vswhere -latest -products * `
            -requires Microsoft.Component.MSBuild -property installationPath
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($installation)) {
            $candidate = Join-Path $installation.Trim() `
                'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                $cmake = $candidate
            }
        }
    }
}

if ([string]::IsNullOrWhiteSpace($cmake) -or
    -not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
    throw 'CMake executable not found. Add CMake to PATH or install the Visual Studio CMake workload.'
}
if (-not (Test-Path -LiteralPath $qtBin -PathType Container)) {
    throw "Qt 6.8.3 bin directory not found: $qtBin"
}
if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
    throw "Qt deployment tool not found: $windeployqt"
}

Push-Location $repositoryRoot
try {
    & $cmake --preset $Preset -DDRUMAS_BUILD_UI=ON
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }

    & $cmake --build --preset "$Preset-release" --target drumaspro_qt --parallel
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

    if ([string]::IsNullOrWhiteSpace($env:QT_QUICK_BACKEND)) {
        $env:QT_QUICK_BACKEND = 'software'
    }
    if ([string]::IsNullOrWhiteSpace($env:QSG_RHI_BACKEND)) {
        $env:QSG_RHI_BACKEND = 'software'
    }

    & $application
    if ($LASTEXITCODE -ne 0) {
        throw "UI exited with code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
