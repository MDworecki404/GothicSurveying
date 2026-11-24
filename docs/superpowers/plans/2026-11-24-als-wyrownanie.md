# ALS: kalibracja boresightu i wyrownanie szeregow - plan

> Wykonanie: natywnie (jak poprzednio), TDD, male commity na galezi `als-wyrownanie`, bez Co-Authored-By, bez push/merge/wydan bez polecenia.

**Spec:** `docs/superpowers/specs/2026-11-24-als-wyrownanie-design.md`

## Global Constraints
- `powershell -File tests/run.ps1` -> `ALL OK`, `powershell -File scripts/build.ps1` -> `BUILD OK` przed kazdym commitem.
- Teksty w grze ASCII; makra windows.h (`(std::min)`); duze chmury strumieniowo; wspolrzedne siatki z offsetem.

## Review Focus
1. Jeden szereg / brak nakladek / brak plikow roboczych: komunikat, bez awarii (T4, T5).
2. Plaski teren: pitch/yaw "nieokreslony", wynik nadal poprawny dla roll i szeregow (T3, T5).
3. Wznowienie po zapisie nie moze brac `lot_surowy.bin` za plik wznowienia (T1).
4. Rekord 34 B: stare stale 26 B w resume (T1).
5. Rozjazd czasow CSV i LAS (stala 345600) przy odczycie trajektorii (T3).

### T1: rekord surowy 34 B, zachowanie plikow
Pliki: `src/calc/AlsLas.hpp`, `src/job/AerialJob.hpp`, testy `test_alslas.cpp`, `test_aerial_job.cpp`.
- `AlsPoint` dostaje `float theta = 0, range = 0;`; `inline const int kAlsRawBytes = 34;` zapis/odczyt/`Open(append)` (rozmiar / kAlsRawBytes); `AerialJob` uzywa `kAlsRawBytes` zamiast 26 (3 miejsca) i ustawia `theta` i `range` (zmierzona odleglosc z szumem) w `EmitPulse`.
- `FinishFlight`: bez `keepRaw` plik `lot.raw` jest przenoszony do `lot_surowy.bin` (nadpisuje); z `keepRaw` zostaje `lot.raw` (wznowienie). Wznowienie sprawdza tylko `lot.raw`.
- Testy: rekord ma 34 B i theta/range wracaja z odczytu; po locie istnieje `lot_surowy.bin`, nie ma `lot.raw`; wznowienie po zakonczonym locie nadal idzie do nowego folderu.

### T2: szeregi kalibracyjne w planie
Pliki: `src/calc/Flight.hpp`, `src/job/AerialJob.hpp`, testy.
- `FlightParams::crossStrips = 0` (0 = wylaczone, w menu 0/3); `PlanFlight` dodaje `crossStrips` szeregow prostopadlych (azymut + 90), rownomiernie w obszarze, kierunki naprzemienne, id dalej, `sortie` jak reszta (wymiana baterii); pole `PlanFlight::crossCount`.
- Menu `calib` ("Szeregi kalibracyjne: wylaczone/3 poprzeczne"), zapis w stanie.
- Testy: kwadrat 200 m, `crossStrips = 3`: 3 dodatkowe szeregi, kazdy prostopadly do pierwszego (iloczyn skalarny kierunkow ~ 0), leza wewnatrz obszaru, dluzszy czas lotu; wielokat wklesly nie psuje planu; menu przelacza.

### T3: trajektoria, wiazanie, kalibracja boresightu (`src/calc/AlsAdjust.hpp`)
- `struct EstSample { double t; Vec3 pos; double roll, pitch, yaw; };` `std::vector<EstSample> AlsLoadTrajectory(const std::string& csv)` (kolumny gnss + imu; `t` w pliku = czas lotu, gps = kGpsT0 + t); `EstSample AlsEstAt(const std::vector<EstSample>&, double t)`: interpolacja liniowa z zawinieciem yaw, poza zakresem skrajna.
- `struct AdjustParams { double cellM = 4; int minPerStrip = 8, maxPerCell = 24, maxCells = 3000; Vec3 lever{0.0, 0.15, -0.30}; double gpsT0 = 345600.0; };`
- `class AlsAdjuster { AlsAdjuster(rawPath, trajCsv, originN, originE, originH, std::vector<ControlPoint>, AdjustParams = {}); bool Step(int budgetMs); const AdjustResult& Result() const; std::string Progress() const; }`; fazy: indeksowanie, zbieranie, kalibracja (1 iteracja na Step), szeregi, zapis, koniec. `Step` zwraca true po zakonczeniu. `AdjustResult { bool ok; std::string msg; double bore[3], sigma[3]; bool determined[3]; double rmsBefore, rmsAfter; int cells, strips; std::vector<StripShift> shifts; std::vector<ControlQc> ctrlBefore, ctrlAfter; std::string outRaw; }`.
- Sztuczna scena w teście (analityczny teren z nachyleniem i wzgorzami, przeciecie promienia z terenem przez bisekcje; lot `FlightPath` + `TrajectoryNoise` z zerowymi szumami i samym boresightem 0,2 st.; zapis surowy i CSV): kalibracja odzyskuje `TrueBoresight()` z bledem < 0,02 st. (kat miedzy macierzami), `rmsAfter < 0,2 * rmsBefore`, `determined` dla roll i dla pitch/yaw na terenie z nachyleniem; teren plaski: roll odzyskany, pitch/yaw `!determined`, wynik 0; jeden szereg: `!ok` z komunikatem; brak plikow: `!ok`.

### T4: wyrownanie szeregow, osnowa, zapis wyniku
- Faza szeregow w `AlsAdjuster`: LS dla (sE, sN, sH) na szereg z warunkami z T3 (gradient plaszczyzny), wiezami osnowy i slabym warunkiem sredniej; niepewnosci z odwrotnosci macierzy normalnej; faza zapisu pisze `lot_wyrownany.raw` (ponowne georeferowanie z `bore` i przesunieciami).
- Testy: sztuczny przesuniety szereg (+0,15 m w H) jest poprawiany: `meanDz` po < 0,01; z wiezem osnowy wysokosc bezwzgledna zgadza sie z osnowa (< 0,02 m), bez osnowy srednie przesuniecie ~ 0; przesuniecie poziome 0,3 m na stoku jest odzyskane (< 0,05 m); `ctrlAfter` lepsze niz `ctrlBefore`.

### T5: integracja z robota
- `AerialJob`: menu `adjust` ("Wyrownaj szeregi i skalibruj boresight", wlaczone po locie, gdy istnieje `lot_surowy.bin`) i `adjust_cancel`; w `Tick` kolejne `Step(kBudgetMs)`; Hud pokazuje faze; po zakonczeniu `AlsConvert` -> `lot_wyrownany.las/.pts`, raport `lot_wyrownanie.txt` (katy +- sigma lub "nieokreslony", przesuniecia, rms przed/po, osnowa przed/po; w trybie prawdziwego bledu: prawdziwy boresight i bias IMU, ich suma); komunikaty `Say`.
- `TrajectoryNoise::BiasDeg(r, p, y)`; prawdziwy boresight z `TrajectoryNoise(seed_, profile)`.
- Testy (falszywy swiat, teren plaski): po locie z dwoma szeregami i profilem z boresightem pliki powstaja, roll odzyskany z tolerancja, pitch/yaw "nieokreslony" w raporcie, `rmsAfter < rmsBefore`; menu wylaczone przed lotem i w trakcie; przerwanie wyrownania nie zostawia polowicznych plikow wyniku.

### T6: dokumentacja
README (sekcja ALS, ograniczenia), `INSTALACJA` bez zmian, spec: status.
