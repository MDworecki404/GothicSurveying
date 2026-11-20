# Kompiluje i uruchamia kazdy tests/test_*.cpp (g++ -std=c++20), potem sprawdza tests/out/sample.docx (Python).
# Uzycie: powershell -File tests/run.ps1      Wynik: "ALL OK" albo kod wyjscia 1.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) { $env:PATH += ";C:\msys64\ucrt64\bin" }
$tmp = Join-Path $env:TEMP "gs_tests"
New-Item -ItemType Directory -Force $tmp | Out-Null
$fail = 0
Push-Location $root
foreach ($t in Get-ChildItem (Join-Path $root "tests") -Filter "test_*.cpp") {
    $exe = Join-Path $tmp ($t.BaseName + ".exe")
    & g++ -std=c++20 -Wall -static -I (Join-Path $root "src") $t.FullName -o $exe
    if ($LASTEXITCODE -ne 0) { $fail++; Write-Host "COMPILE FAIL $($t.Name)"; continue }
    & $exe
    if ($LASTEXITCODE -ne 0) { $fail++; Write-Host "FAIL $($t.Name) (exit $LASTEXITCODE)" } else { Write-Host "ok   $($t.Name)" }
}
$docx = Join-Path $root "tests\out\sample.docx"
if (Test-Path $docx) {
    & python (Join-Path $root "tests\check_docx.py") $docx
    if ($LASTEXITCODE -ne 0) { $fail++; Write-Host "FAIL check_docx.py" } else { Write-Host "ok   check_docx.py" }
}
$docxM = Join-Path $root "tests\out\sample_manual.docx"
if (Test-Path $docxM) {
    & python (Join-Path $root "tests\check_docx.py") $docxM --manual
    if ($LASTEXITCODE -ne 0) { $fail++; Write-Host "FAIL check_docx.py --manual" } else { Write-Host "ok   check_docx.py --manual" }
}
Pop-Location
if ($fail -eq 0) { Write-Host "ALL OK" } else { Write-Host "$fail FAILED"; exit 1 }
