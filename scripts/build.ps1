# Buduje plugin (MSVC x86, preset x86-release) i instaluje wyniki do out/install/x86-release. Uzycie: powershell -File scripts/build.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
$bat = Join-Path $env:TEMP "gs_build.bat"
@(
    "@echo off",
    "call `"$vcvars`" x86 >nul 2>&1",
    "set PATH=%PATH%;C:\msys64\ucrt64\bin",
    "cmake --preset x86-release || exit /b 1",
    "cmake --build out/build/x86-release || exit /b 1",
    "cmake --install out/build/x86-release --prefix out/install/x86-release || exit /b 1"
) | Set-Content -Encoding ASCII $bat
Set-Location $root          # katalog roboczy dziedziczy cmd (sciezka z polskimi znakami nie trafia do .bat)
& cmd /c $bat
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED"; exit 1 }
Write-Host "BUILD OK"
