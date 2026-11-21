"""Buduje paczkę wydania: zip do rozpakowania w katalogu gry Gothic II (system/Autorun + _work/Data).

Użycie:  python scripts/package.py [wersja]      (najpierw: powershell -File scripts/build.ps1)
Wynik:   release/GothicSurveying-v<wersja>-G2A.zip
"""
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION = sys.argv[1] if len(sys.argv) > 1 else "0.1.2"
BIN = ROOT / "out" / "install" / "x86-release" / "bin"

README = r"""GothicSurveying v{version} - niwelacja geometryczna w Gothic II: Noc Kruka (plugin Union)

WYMAGANIA
- Gothic II: Noc Kruka z zainstalowanym Union (w folderze system musi byc plik Union.patch).
  Przetestowane na wersji Steam (Gold Classic).
- Sciezka do gry BEZ polskich znakow. Inaczej Union pokaze "Union Fatal". Jesli folder gry ma polskie znaki,
  utworz dowiazanie (PowerShell):
      New-Item -ItemType Junction -Path C:\GothicII -Target "<prawdziwy folder gry>"
  i uruchamiaj C:\GothicII\system\Gothic2.exe (skrot zrob recznie do tej sciezki).

INSTALACJA
1. Rozpakuj to archiwum w folderze gry, tam gdzie leza foldery "system" i "_work"
   (zgodz sie na scalenie folderow).
2. Uruchom Gothic2.exe, wczytaj zapis i nacisnij Home.

STEROWANIE
- Home: menu geodezyjne (Gora/Dol - wybor, Enter - zatwierdz, Home - zamknij; F12 to zrzut ekranu Steam, F11 - GD3D11). "Rozpocznij robote" ->
  typ roboty (ciag niwelacyjny) -> rodzaj ciagu (dowiazany/zamkniety/otwarty) -> tryb liczenia dziennika.
- Sprzet staje pod stopami bohatera. Kroki: reper A, niwelator, lata wstecz, odczyt, lata wprzod, odczyt,
  "Przeloz na nastepne stanowisko", ..., punkt koncowy, "Zakoncz robote".
- F10: widok przez lunete (strzalki - obrot, Shift - powoli, Enter - odczyt; wpisz odczyt
  srodkowej kreski w mm cyframi 0-9; dalmierz liczy sie sam; Z / X - obrot na late wstecz / wprzod).
- Wynik: tabela w grze oraz pliki dziennik_N.docx i dziennik_N.csv w katalogu roboczym gry
  (przerwana robota: dziennik_N_przerwany.csv).

ODINSTALOWANIE
Usun: system\Autorun\GothicSurveying.dll, system\Autorun\UnionAPI.dll, system\Autorun\Signatures\Signatures_G2A.tsv,
pliki LVL_* z _work\Data\Meshes i _work\Data\Textures (oraz z ich podfolderow _compiled).
Uwaga: UnionAPI.dll i Signatures moga byc uzywane przez inne pluginy Union - nie nadpisuj nowszych wersji bez potrzeby.
"""


def main():
    needed = ["GothicSurveying.dll", "UnionAPI.dll", "Signatures/Signatures_G2A.tsv"]
    for f in needed:
        if not (BIN / f).exists():
            sys.exit(f"Brak {BIN / f} - najpierw: powershell -File scripts/build.ps1")
    stage = Path(tempfile.mkdtemp(prefix="gs_release_"))
    try:
        autorun = stage / "system" / "Autorun"
        (autorun / "Signatures").mkdir(parents=True)
        shutil.copy(BIN / "GothicSurveying.dll", autorun)
        shutil.copy(BIN / "UnionAPI.dll", autorun)
        shutil.copy(BIN / "Signatures" / "Signatures_G2A.tsv", autorun / "Signatures")
        sys.path.insert(0, str(ROOT / "assets"))
        import make_models
        sys.argv = ["make_models.py", str(stage)]
        make_models.main()                                     # modele i tekstury -> stage/_work/Data
        (stage / "INSTALACJA.txt").write_text(README.format(version=VERSION), encoding="utf-8-sig")
        out = ROOT / "release" / f"GothicSurveying-v{VERSION}-G2A.zip"
        out.parent.mkdir(exist_ok=True)
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
            for p in sorted(stage.rglob("*")):
                if p.is_file():
                    z.write(p, p.relative_to(stage).as_posix())  # ukośniki '/', jak chce standard ZIP
        print(f"{out} ({out.stat().st_size / 1024 / 1024:.1f} MB)")
    finally:
        shutil.rmtree(stage, ignore_errors=True)


if __name__ == "__main__":
    main()
