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
VERSION = sys.argv[1] if len(sys.argv) > 1 else "0.6.0"
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
- Osnowa GNSS: w robocie GNSS "Zaloz punkt osnowy" (fix, 20 epok) zapisuje punkt do GothicSurveying\\osnowa.csv i stawia znak; "Pokaz osnowe" w menu glownym.
  Tachimetria moze stac na punktach osnowy, do 5 nawiazan, orientacja ze sredniej, odchylki kontrolne w dzienniku.
- Tachimetria: "Rozpocznij robote" -> "Tachimetria (pomiar biegunowy)"; tachimetr na statywie, punkt nawiazania z lustrem, orientacja (Enter w lunecie na lustrze),
  tyczka z lustrem o wybranej wysokosci albo pomiar bezlustrowy (F10 luneta, strzalki obracaja i pochylaja); wynik: pomiar_tachimetryczny.csv i .docx w folderze roboty.
- Luneta: PgUp / PgDn - powiekszenie (3x-40x), cienkie kreski wlosowe; w niwelacji menu "Wysun laty" (3 / 4 / 5 m); w tachimetrii Z - szybkie najechanie na lustro,
  K - przerzut lunety (opcja "Polozenia lunety: dwa" = KL+KP, usuwa bledy kolimacji i indeksu), "Stanowisko dowolne" (wciecie wstecz), "Wytycz punkt osnowy", "Nowe stanowisko" (wiele stanowisk w robocie).
- Poziomowanie niwelatora i tachimetru (libella, sruby 1-6) jest domyslnie WLACZONE (menu roboty: Poziomuj instrument; wylaczysz w menu glownym). Tachimetr domyslnie mierzy w dwoch polozeniach lunety (KL+KP).
- Zapis gry w trakcie roboty zapisuje tez stan roboty i sprzetu (GothicSurveying\stan_slot<N>.txt); wczytanie zapisu wznawia robote.
- Skaner laserowy: "Rozpocznij robote" -> "Skaner laserowy"; postaw i wypoziomuj skaner, wybierz rozdzielczosc, skanuj; wyniki (.pts, .las) w GothicSurveying\<data>_skan.
  Model skanera: wlasny (statyw, korpus, obrotowa glowica z oknem wiazki). Rejestracja skanow: min. 3 kule referencyjne na znakach osnowy (menu skanera), stanowisko i obrot wyznaczane z kul.
- Osnowa swiata: "Zaladuj osnowe geodezyjna swiata" (wzorcowe punkty G.. dla Khorinis, Doliny Gornickiej i Jarkendaru), "Mapa osnowy" (mapa swiata z punktami),
  "Teleport do punktu osnowy". Pomiar GNSS: wlasna baza (poprawki RTK) i pomiar statyczny.
- Pomiar GNSS (RTK): "Rozpocznij robote" -> "Pomiar GNSS (RTK)"; tyczka z odbiornikiem, panel w lewym gornym rogu,
  "Zapisz punkt" po fixie; wynik: pomiar_gnss.csv i pomiar_gnss.docx w folderze roboty.
- Wynik: tabela w grze oraz pliki dziennik.docx, dziennik.csv i odczyty.csv w folderze
  GothicSurveying\RRRR-MM-DD_GG-MM-SS (czas zakonczenia roboty) w katalogu gry (przerwana robota: dziennik_przerwany.csv).

LICENCJE MODELI
Kask: "Construction Hat" by ermalkoci, kamizelka: "safety vest" by teacherap123 (Sketchfab, CC BY 4.0,
https://creativecommons.org/licenses/by/4.0/); zmienione: uproszczone i skonwertowane do .3DS. Zrodla: assets/imported/ATTRIBUTION.txt w repozytorium.

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
