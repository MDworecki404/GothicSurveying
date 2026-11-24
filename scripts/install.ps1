<#
.SYNOPSIS
    Instaluje plugin GothicSurveying w grze Gothic II NotR: DLL do system\Autorun, modele i tekstury do _work\Data.
.EXAMPLE
    .\scripts\install.ps1 -Game C:\GothicII
    # po zbudowaniu: cmake --preset x86-release; cmake --build out/build/x86-release; cmake --install out/build/x86-release --prefix out/install/x86-release
#>
param(
    [Parameter(Mandatory = $true)][string]$Game,
    [string]$Install
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $Install) { $Install = Join-Path $root "out/install/x86-release/bin" }

if ($Game -match '[^\x00-\x7F]' -or (Resolve-Path $Game).Path -match '[^\x00-\x7F]') {
    Write-Warning "Sciezka gry zawiera znaki spoza ASCII - Union moze zglosic 'Union Fatal'. Uruchamiaj gre przez junction, np. New-Item -ItemType Junction -Path C:\GothicII -Target '<katalog gry>'"
}
$autorun = Join-Path $Game "system\Autorun"
if (-not (Test-Path (Join-Path $Game "system\Gothic2.exe"))) { throw "Nie znaleziono system\Gothic2.exe w $Game" }
if (-not (Test-Path (Join-Path $Game "system\Union.patch"))) { Write-Warning "Brak system\Union.patch - Union chyba nie jest zainstalowany w tej grze." }
$dll = Join-Path $Install "GothicSurveying.dll"
if (-not (Test-Path $dll)) { throw "Brak $dll - najpierw zbuduj plugin (patrz README)." }

New-Item -ItemType Directory -Force $autorun | Out-Null
Copy-Item $dll, (Join-Path $Install "UnionAPI.dll") $autorun -Force
Copy-Item (Join-Path $Install "Signatures") $autorun -Recurse -Force

python (Join-Path $root "assets\make_models.py") $Game
if ($LASTEXITCODE -ne 0) { throw "Generowanie modeli nie powiodlo sie (potrzebny Python 3)." }

# kolory tekstur do skanera laserowego: miniaturki z archiwow VDF gry (raz; ok. 20 s)
if (-not (Test-Path (Join-Path $Game "GothicSurveying\texcolors.bin"))) {
    python -X utf8 (Join-Path $root "assets\make_texture_colors.py") $Game
    if ($LASTEXITCODE -ne 0) { Write-Warning "Nie udalo sie wygenerowac texcolors.bin - skaner uzyje zapasowych kolorow." }
}

# stare, skompilowane przez silnik kopie modeli i tekstur
Remove-Item (Join-Path $Game "_work\Data\Meshes\_compiled\LVL_*"), (Join-Path $Game "_work\Data\Textures\_compiled\LVL_*") -ErrorAction SilentlyContinue
Write-Host "Gotowe. Uruchom gre i wczytaj zapis. Home otwiera menu geodezyjne, F10 wchodzi do lunety."
