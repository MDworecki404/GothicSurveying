# GothicSurveying - notatki dla Claude

## Przewodnik PDF „Pomiary geodezyjne - jak wykonywać i obliczać (przewodnik krok po kroku)" (Downloads, 9 s., 2026-11-24)

Sprawdzone 2026-11-24: wszystkie wzory, kolejność etapów i **wszystkie przykłady liczbowe są poprawne** (przeliczone niezależnie, zgodność do 1 mm / 0,0001 g).
Wartości dopuszczalnych odchyłek PDF celowo podaje tylko jako odwołanie do instrukcji; kod wtyczki przyjmuje `fh_dop = 20 mm * sqrt(L[km])` (`calc/Traverse.hpp`) i to jest założenie projektu, nie wzór z PDF.

### Poprawiona wersja w repo
`docs/przewodnik-pomiary-geodezyjne.pdf` (źródło: `docs/przewodnik-pomiary-geodezyjne.html`; PDF z HTML: `msedge --headless --no-pdf-header-footer --print-to-pdf=...`). Usunięto wady 1-6 poniżej: wzory wyrenderowane, uwaga o kącie wysokościowym i zenitalnym (α = 100g − V) z wzorami dla V, rozdział o ciągu zamkniętym ze wzorami i obchodem przeciwnym/zgodnym ze wskazówkami zegara, wysokość celu oznaczona `s`, odwołanie do G-4 zastąpione ogólnym (bez numeru załącznika), źródła: tylko dwa zweryfikowane linki, reszta samym tytułem.

### Co zweryfikowano (OK)
- **Grady i zaokrąglanie:** 1g = 100c = 10000cc; α[g] = 10/9 · α[°]; 23g07c17cc = 23,0717g; Bradis-Kryłow (4,25→4,2; 4,15→4,2).
- **Azymut:** przyrosty ΔX = d cos A, ΔY = d sin A; tabela ćwiartek (φ, 200−φ, 200+φ, 400−φ) zgodna ze znakami ΔX, ΔY; azymut przeciwny ±200g.
- **Niwelacja geometryczna:** Δh = t − p; f_h = ΣΔh − (H_Rk − H_Rp); v = −f_h/n (lub proporcjonalnie do długości celowych, jak w `calc/Traverse.hpp`); kontrola Σt − Σp = ΣΔh. Przykład (4,813 − 3,610 = 1,203; f_h = +3 mm; B = 151,200) poprawny.
- **Niwelacja trygonometryczna:** Δh = d·tgα + i − l; h = d(tgα_g − tgα_d) → 20,13 m (tg40g = 0,72654; tg5g = 0,07870); poprawka p = (1−k)d²/(2R) → 17 mm dla d = 500 m (k = 0,13, R = 6371 km); ta sama poprawka jest w `TrigHeightDiffM` (`calc/Tachy.hpp`).
- **Tachimetria:** β = K_P − K_N (mod 400g), A_SP = A_SN + β, d = d'·cosα, H_P = H_S + i + d'·sinα − l. Przykład: d = 49,944; ΔX = −35,316; ΔY = +35,316; P(964,684; 2035,316); H = 102,355 poprawne.
- **Ciąg poligonowy nawiązany obustronnie (kąty lewe):** A_{i,i+1} = A_{i−1,i} + β − 200g; f_β = A_p + Σβ − n·200g − A_k; v_β = −f_β/n; f_x = X_A + ΣΔX − X_B; v_x,i = −f_x·d_i/Σd. Przykład: A_p = 70,4833g, A_k = 33,0499g, f_β = +0,0020g, f_x = +0,014, f_y = +0,009, f_d = 0,017 m, współrzędne końcowe = B (1460,000; 1560,000) poprawne.

### Wady oryginalnego PDF (nie błędy rachunkowe; poprawione w wersji z repo)
1. **Wzory nie są wyrenderowane:** w blokach kodu stoi surowy LaTeX (`\Delta h = t - p`, `\frac{10}{9}`…) w całym dokumencie; do wydruku/nauki trzeba je wyrenderować.
2. **Kąt pionowy = kąt wysokościowy α (0g poziomo).** Tachimetry i kod wtyczki używają kąta **zenitalnego V** (0g w zenit). Konwersja: α = 100g − V, więc d = d'·sinV, Δh = d'·cosV. PDF o tym nie wspomina, a to częsta pomyłka; przy przenoszeniu wzorów do kodu używaj V (`calc/Tachy.hpp`).
3. **Kąty wewnętrzne a kąty lewe:** „suma kątów wewnętrznych (n−2)·200g" w ciągu zamkniętym jest prawdziwa tylko gdy kąty lewe są wewnętrzne (obieg przeciwnie do ruchu wskazówek zegara); przy obiegu zgodnym lewe kąty są zewnętrzne (suma (n+2)·200g). PDF tego nie zaznacza.
4. **Ciąg zamknięty bez wzorów:** PDF opisuje go jednym zdaniem (suma kątów, ΣΔX = ΣΔY = 0), bez wzorów f_β i f_x/f_y dla tego przypadku.
5. **Oznaczenie „l"** (wysokość celu) wygląda jak wielkie „I" w użytym kroju; lepiej np. „w" albo „ℓ".
6. **Nie do sprawdzenia:** powołanie na „Instrukcję G-4 (załącznik 3)" i numer załącznika oraz aktualność tej instrukcji (przepisy techniczne mogły zostać zmienione); linki w źródłach nie były otwierane. PDF sam zastrzega, że podręczniki są „z pamięci" i dane w przykładach są wymyślone.

### Jak z tego korzystać
Traktuj PDF jako poprawny materiał dydaktyczny do wzorów i kolejności etapów; przy zmianach w `calc/` pilnuj konwencji (zenit V, grady vs stopnie/radiany, kąty lewe) i nie przenoś wartości dopuszczalnych z PDF, bo ich tam nie ma.
