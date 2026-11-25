// Kontroler: menu startowe (typ roboty -> rodzaj -> tryb liczenia), menu aktywnej roboty, luneta, HUD kroku. Bez strażnika include.
#include <memory>

namespace GOTHIC_NAMESPACE {
  inline void ProjUiOpenHome();                        // engine/ProjectUi.hpp (dolaczane po tym pliku)
  inline void ProjUiInstallMenuHooks();
  inline std::unique_ptr<Job> gJob;
  inline zCWorld*           gCtrlWorld = nullptr;
  inline zCView*            gJobHud = nullptr;       // stały panel z bieżącym krokiem roboty
  inline std::string        gJobHudText;
  inline JobOptions         gJobOpts{ TraverseKind::Open, CalcMode::Auto, false };   // opcje biezacej roboty (do zapisu stanu)
  inline int                gJobTypeIdx = -1;         // indeks typu biezacej roboty w JobTypes() (do zapisu stanu)
  inline int                gPendingLoadSlot = -2;    // -2 = brak; slot wczytywanego zapisu (ustawia hook LoadSavegame, zuzywa ControllerTick)
  inline std::string        gReadingLog;              // GothicSurveying/odczyty_biezace.csv: log odczytów bieżącej roboty (po końcu trafia do folderu roboty)
  inline std::string        gJobDir;                  // folder zakończonej roboty: GothicSurveying/<data_godzina>

  inline bool               gStartGuided = false;     // czy nastepny pomiar startuje w trybie prowadzonym (kreator)

  // Rejestracja konca pomiaru w projekcie (zakonczenie / przerwanie / zmiana swiata); wolac przed CtrlEndJob.
  inline void CtrlMarkEnd( MeasState s, const std::string& dir, const std::string& summary ) {
    if ( gJobMeasId.empty() ) return;
    SetState( gProject, gJobMeasId, s, dir, summary );
    ProjSave();
    gJobMeasId.clear();
  }
  inline std::string CtrlFirst( const std::vector<std::string>& v ) { return v.empty() ? std::string() : v[0]; }

  // Punkty wysokosciowe pomiaru (nazwa, N, E, H) do projekty/<nazwa>/pomiar_<id>.csv: z nich zamkniecie projektu liczy porownanie wysokosci.
  inline void CtrlStorePoints( const std::vector<PointH>& pts ) {
    if ( gJobMeasId.empty() || pts.empty() ) return;
    const std::string file = "pomiar_" + gJobMeasId + ".csv", dir = ProjRoot() + "/projekty/" + gProject.name;
    std::error_code ec; std::filesystem::create_directories( dir, ec );
    WriteBytes( dir + "/" + file, PointsCsv( pts ) );
    for ( Measurement& m : gProject.items ) if ( m.id == gJobMeasId ) m.pointsFile = file;
  }

  inline FieldBook          gPendingFb;               // dziennik oczekujący na wpisy gracza (tryb samodzielny)

  inline zCView*            gStatusPanel = nullptr;  // panel odbiornika GNSS (linie ze Job::StatusLines)
  inline std::string        gStatusText;

  inline void CtrlCloseJobHud() { PanelClose( gJobHud ); gJobHudText.clear(); PanelClose( gStatusPanel ); gStatusText.clear(); }
  inline void CtrlEndJob() { gJob.reset(); gWorld.ClearAll(); CtrlCloseJobHud(); }
  inline void CtrlForgetJob() {                        // po zmianie świata: obiekty zniknęły razem ze światem
    gWorld.ForgetAll();                                // najpierw oddaj vob-y, potem callbacki lunety nie dotkną starych obiektów
    ScopeLeave(); PoleFineLeave(); TachyViewLeave(); LevelClose(); MenuClose(); gJob.reset(); CtrlCloseJobHud();
  }

  // Stan poziomowania instrumentu roboty: "" gdy bez libelli albo bez instrumentu.
  inline std::string CtrlLevelState() {
    Libella* l = gJob ? gJob->GetLibella() : nullptr;
    if ( !l || !l->enabled || !gWorld.HasInstrument() ) return "";
    const double lim = gJob->Kind() == JobKind::Leveling || gJob->Kind() == JobKind::Scan ? kLevelBubbleArcmin : kTachyLevelArcmin;
    char b[64]; sprintf( b, l->Within( lim ) ? "poziomo (%.1f')" : "NIEWYPOZIOMOWANY (%.1f')", l->Mag() );
    return b;
  }

  inline void CtrlEnterScope() {
    if ( !gJob || !gWorld.HasInstrument() || gJob->ActiveRodPoints().empty() ) { HudMessage( "Najpierw postaw niwelator i late (" LV_KEYNAME_MENU ")" ); return; }
    ScopeCallbacks cb;
    cb.targets = []() {
      std::vector<RodTarget> t;
      if ( !gJob ) return t;
      for ( int pid : gJob->ActiveRodPoints() ) {
        const bool back = gJob->RodPointFor( RodRole::Back ) == pid, fore = gJob->RodPointFor( RodRole::Fore ) == pid;
        t.push_back( { pid, gWorld.PointFoot( pid ), back && fore ? "wstecz/wprzod" : fore ? "wprzod" : "wstecz" } );
      }
      return t;
    };
    cb.rodFoot = []( bool fore, zVEC3& foot ) {
      const int pid = gJob ? gJob->RodPointFor( fore ? RodRole::Fore : RodRole::Back ) : -1;
      if ( pid < 0 ) return false;
      foot = gWorld.PointFoot( pid );
      return true;
    };
    cb.onReading = []( int p, int m ) {
      if ( !gJob || !gJob->OnReading( p, m ) ) return false;
      AppendReadingLog( gReadingLog, p, m );
      return true;
    };
    cb.block = []() { return gJob ? gJob->InstrumentBlock() : std::string(); };
    cb.levelState = []() { return gJob ? CtrlLevelState() : std::string(); };
    cb.onEnter = []() { gWorld.TurnRodsToInstrument(); gWorld.SetInstrumentVisible( false ); };
    cb.onLeave = []() { gWorld.SetInstrumentVisible( true ); };
    ScopeEnter( gWorld.InstrumentGround(), cb );
  }

  // Przerwana robota (Przerwij / wczytanie zapisu): zapisuje, co zmierzono, do nowego folderu; zwraca go albo "" gdy nie było czego zapisać.
  inline std::string CtrlSavePartial( const FieldBook& part ) {
    if ( part.rows.empty() && !FileExistsRel( gReadingLog ) ) return "";
    const std::string dir = MakeJobDir();
    if ( !part.rows.empty() ) ExportFieldBook( part, dir, true );
    MoveReadingLog( gReadingLog, dir );
    return dir;
  }

  // Przerwana robota GNSS (Przerwij / wczytanie zapisu): zapisuje zmierzone punkty do nowego folderu; "" gdy nie bylo punktow.
  // Tachimetria: przerwana robota / zmiana swiata zapisuje zmierzone punkty do nowego folderu; "" gdy nie bylo punktow.
  inline std::string CtrlSavePartialTachy( const TachyReport& part ) {
    if ( part.points.empty() && !FileExistsRel( gReadingLog ) ) return "";
    const std::string dir = MakeJobDir();
    if ( !part.points.empty() ) ExportTachy( part, dir, true );
    MoveReadingLog( gReadingLog, dir );
    return dir;
  }

  // Ciag poligonowy: przerwana robota / zmiana swiata zapisuje ukonczone stanowiska do nowego folderu; "" gdy nie bylo czego zapisac.
  inline std::string CtrlSavePartialPolygon( const PolygonReport& part ) {
    if ( part.stations.empty() && !FileExistsRel( gReadingLog ) ) return "";
    const std::string dir = MakeJobDir();
    if ( !part.stations.empty() ) ExportPolygon( part, dir, true );
    MoveReadingLog( gReadingLog, dir );
    return dir;
  }

  inline void CtrlFinishPolygon() {
    const PolygonReport r = gJob->FinishPolygon( true );
    gJobDir = MakeJobDir();
    MoveReadingLog( gReadingLog, gJobDir );
    const bool ok = ExportPolygon( r, gJobDir, false );
    const std::vector<std::string> lines = PolygonSummaryLines( r );
    TextPanelShow( "Ciag poligonowy - podsumowanie", lines, ok ? "Zapisano w folderze: " + gJobDir : "BLAD ZAPISU plikow raportu" );
    SvTrace( "Ciag poligonowy zakonczony" );
    CtrlMarkEnd( MeasState::Done, gJobDir, lines.size() > 1 ? lines[1] : CtrlFirst( lines ) );
    CtrlEndJob();
  }

  inline void CtrlFinishTachy() {
    const TachyReport r = gJob->FinishTachy( true );
    gJobDir = MakeJobDir();
    MoveReadingLog( gReadingLog, gJobDir );             // log punktow zapisywany na biezaco -> odczyty.csv w folderze roboty
    const bool ok = ExportTachy( r, gJobDir, false );
    std::vector<std::string> lines = TachySummaryLines( r );
    lines.push_back( "" );
    for ( const TachyPoint& p : r.points ) {
      if ( r.trig ) break;                             // niwelacja trygonometryczna: podsumowanie odcinkow wystarcza
      char b[220];
      sprintf( b, "%s  Hz %.4f g  V %.4f g  D %.3f m  N %.3f  E %.3f  H %.3f", p.name.c_str(), p.hzGon, p.vGon, p.slopeM, p.n, p.e, p.h );
      lines.push_back( b );
      if ( r.showTrueError && p.hasTrue ) {
        sprintf( b, "    blad: N %.3f  E %.3f  H %.3f  2D %.3f m", p.dNm, p.dEm, p.dHm, p.d2Dm );
        lines.push_back( b );
      }
    }
    TextPanelShow( r.trig ? "Niwelacja trygonometryczna - podsumowanie" : "Tachimetria - podsumowanie", lines, ok ? "Zapisano w folderze: " + gJobDir : "BLAD ZAPISU plikow dziennika" );
    SvTrace( "Tachimetria zakonczona" );
    CtrlStorePoints( PointsOf( r ) );
    CtrlMarkEnd( MeasState::Done, gJobDir, CtrlFirst( lines ) );
    CtrlEndJob();
  }

  // Widok lunety tachimetru; wymaga stanowiska. Enter w widoku wykonuje orientacje albo pomiar (wg trybu roboty).
  inline void CtrlEnterTachyView() {
    if ( !gJob || !gWorld.HasInstrument() ) { HudMessage( "Najpierw postaw tachimetr" ); return; }
    TachyCallbacks cb;
    cb.fire = []( double az, double pitch ) {
      if ( !gJob ) return std::string();
      const size_t before = gJob->FinishTachy( false ).points.size();
      const std::string msg = gJob->TachyFire( az, pitch );
      const TachyReport r = gJob->FinishTachy( false );
      if ( r.points.size() > before ) AppendTachyLog( gReadingLog, r.points.back(), (int)r.points.size(), r.showTrueError );   // punkt od razu do logu
      return msg;
    };
    cb.flip = []() { return gJob ? gJob->TachyFlip() : std::string(); };
    cb.panel = []( double az, double pitch ) { return gJob ? gJob->TachyPanel( az, pitch ) : std::vector<std::string>(); };
    TachyViewEnter( cb );
  }

  inline std::string CtrlSavePartialGnss( const GnssReport& part ) {
    if ( part.points.empty() && !FileExistsRel( gReadingLog ) ) return "";
    const std::string dir = MakeJobDir();
    if ( !part.points.empty() ) ExportGnss( part, dir, true );
    MoveReadingLog( gReadingLog, dir );
    return dir;
  }

  inline void CtrlFinishGnss() {
    const GnssReport r = gJob->FinishGnss( true );
    gJobDir = MakeJobDir();
    MoveReadingLog( gReadingLog, gJobDir );             // log punktów zapisywany na bieżąco -> odczyty.csv w folderze roboty
    const bool ok = ExportGnss( r, gJobDir, false );
    std::vector<std::string> lines = GnssSummaryLines( r );
    lines.push_back( "" );
    for ( const GnssPoint& p : r.points ) {
      char b[220];
      sprintf( b, "%s  %s  N %.3f  E %.3f  H %.3f  PDOP %.1f  RMS %.3f/%.3f m", p.name.c_str(), p.solution.c_str(), p.n, p.e, p.h, p.pdop, p.hRmsMm / 1000.0, p.vRmsMm / 1000.0 );
      lines.push_back( b );
      if ( r.showTrueError && p.hasTrue ) {
        sprintf( b, "    blad: N %.3f  E %.3f  H %.3f  2D %.3f m", p.dNmm / 1000.0, p.dEmm / 1000.0, p.dHmm / 1000.0, p.d2Dmm / 1000.0 );
        lines.push_back( b );
      }
    }
    TextPanelShow( "Pomiar GNSS - podsumowanie", lines, ok ? "Zapisano w folderze: " + gJobDir : "BLAD ZAPISU plikow dziennika" );
    SvTrace( "Pomiar GNSS zakonczony" );
    CtrlStorePoints( PointsOf( r ) );
    CtrlMarkEnd( MeasState::Done, gJobDir, CtrlFirst( lines ) );
    CtrlEndJob();
  }

  inline void CtrlFinish() {
    if ( !gJob ) return;
    if ( gJob->Kind() == JobKind::Gnss ) { CtrlFinishGnss(); return; }
    if ( gJob->Kind() == JobKind::Tachy ) { CtrlFinishTachy(); return; }
    if ( gJob->Kind() == JobKind::Polygon ) { CtrlFinishPolygon(); return; }
    if ( gJob->Kind() == JobKind::Scan ) { TextPanelShow( "Skaner laserowy - podsumowanie", gJob->SummaryLines(), "Pliki .pts i .las w folderze GothicSurveying" ); SvTrace( "Skaner zakonczony" ); CtrlMarkEnd( MeasState::Done, gJob->ResultDir(), CtrlFirst( gJob->SummaryLines() ) ); CtrlEndJob(); return; }
    if ( gJob->Kind() == JobKind::Aerial || gJob->Kind() == JobKind::Photo ) { TextPanelShow( gJob->Name() + " - podsumowanie", gJob->SummaryLines(), gJob->Kind() == JobKind::Photo ? "Zdjecia PNG, zdjecia.csv i raport wyrownania wiazki w folderze GothicSurveying (..._lot)" : "Pliki LAS, PTS, trajektoria i opis lotu w folderze GothicSurveying (..._lot)" ); SvTrace( "Robota lotnicza zakonczona" ); CtrlMarkEnd( MeasState::Done, gJob->ResultDir(), CtrlFirst( gJob->SummaryLines() ) ); CtrlEndJob(); return; }
    FieldBook fb = gJob->Finish( true );
    gJobDir = MakeJobDir();                            // nazwa folderu = czas zakończenia roboty
    MoveReadingLog( gReadingLog, gJobDir );
    if ( fb.mode == CalcMode::Manual ) {               // tryb samodzielny: gracz sam liczy dziennik w edytorze
      gPendingFb = fb; CtrlStorePoints( PointsOf( fb ) ); CtrlMarkEnd( MeasState::Done, gJobDir, "dziennik w trybie samodzielnym: " + std::to_string( (int)fb.rows.size() ) + " stan." ); CtrlEndJob(); FieldBookEditStart( fb );
      return;
    }
    char b[200];
    if ( fb.sum.hasClosure )
      sprintf( b, "Dziennik gotowy: %d stan., fh = %.1f mm (dop. %.1f mm) - %s", (int)fb.rows.size(), fb.sum.fhMm, fb.sum.fhDopMm,
               fb.sum.withinTolerance ? "w tolerancji" : "PRZEKROCZONY" );
    else sprintf( b, "Dziennik gotowy: %d stan., ciag otwarty (brak kontroli)", (int)fb.rows.size() );
    const bool ok = ExportFieldBook( fb, gJobDir, false );
    FieldBookShow( fb, ok ? "Zapisano w folderze: " + gJobDir : "BLAD ZAPISU plikow dziennika" );
    SvTrace( b );
    CtrlStorePoints( PointsOf( fb ) );
    CtrlMarkEnd( MeasState::Done, gJobDir, b );
    CtrlEndJob();
  }

  // Koniec edycji w trybie samodzielnym: sprawdź wpisy, zapisz pliki z werdyktem, pokaż wynik.
  inline void CtrlFinishManual() {
    ManualVerdict v = CheckManual( gPendingFb, FieldBookEditResult() );
    const bool ok = ExportFieldBook( gPendingFb, gJobDir, false, &v );
    std::vector<std::string> lines;
    for ( const auto& r : VerdictRows( v ) ) {
      if ( r[4] != "nie" ) continue;
      lines.push_back( r[0] + ": poprawnie " + r[1] + ( r[2].empty() ? ", nie wpisano" : ", wpisano " + r[2] + " (roznica " + r[3] + " mm)" ) );
    }
    if ( lines.empty() ) lines.push_back( "Wszystkie komorki poprawne!" );
    TextPanelShow( "Sprawdzenie: trafione " + std::to_string( v.ok ) + " z " + std::to_string( v.total ), lines,
                   ok ? "Zapisano w folderze: " + gJobDir : "BLAD ZAPISU plikow dziennika" );
  }

  inline void CtrlStartJob( int typeIdx, TraverseKind kind, CalcMode mode, bool showTrueError = false ) {
    ProjEnsure();
    { std::string why; if ( !CanAddMeasurement( gProject, why ) ) { HudMessage( ( "Nie mozna zaczac pomiaru: " + why ).c_str() ); return; } }
    gJobOpts = JobOptions{ kind, mode, showTrueError };
    gJob = JobTypes()[typeIdx].create( gWorld, gJobOpts ); gJobTypeIdx = typeIdx;
    gReadingLog = PrepareReadingLog();
    { Measurement& m = AddMeasurement( gProject, JobTypes()[typeIdx].id, JobTypes()[typeIdx].name ); m.guided = gStartGuided; gJobMeasId = m.id; m.state = MeasState::Running; ProjSave(); }
    gStartGuided = false;
    gWorld.DressOn();                                  // zdejmuje CtrlEndJob (Zakończ / Przerwij / zmiana świata)
    HudMessage( ( "Robota: " + gJob->Name() + ". " LV_KEYNAME_MENU " - menu" ).c_str() );
  }

  inline void CtrlOpenModeMenu( int typeIdx, TraverseKind kind ) {
    MenuView v{ "Liczenie dziennika", { { "auto", "Dziennik liczony automatycznie", "", true },
                                        { "manual", "Dziennik licze sam (tryb samodzielny)", "", true } } };
    MenuOpen( v, [typeIdx, kind]( const std::string& id ) { CtrlStartJob( typeIdx, kind, id == "auto" ? CalcMode::Auto : CalcMode::Manual ); } );
  }

  inline void CtrlOpenKindMenu( int typeIdx ) {
    MenuView v{ "Rodzaj ciagu", { { "anchored", "Ciag dowiazany (reper A -> reper B)", "", true },
                                  { "closed", "Ciag zamkniety (z A do A)", "", true },
                                  { "open", "Ciag otwarty (bez kontroli)", "", true } } };
    MenuOpen( v, [typeIdx]( const std::string& id ) {
      CtrlOpenModeMenu( typeIdx, id == "anchored" ? TraverseKind::Anchored : id == "closed" ? TraverseKind::Closed : TraverseKind::Open );
    } );
  }

  inline void CtrlOpenTrueErrorMenu( int typeIdx, TraverseKind kind = TraverseKind::Open ) {   // opcja robót GNSS: pokaz prawdziwy błąd względem punktu w grze
    MenuView v{ "Pokazywac prawdziwy blad po pomiarze?", { { "no", "Nie (tylko wynik odbiornika)", "", true },
                                                           { "yes", "Tak (blad wzgledem punktu w grze)", "", true } } };
    MenuOpen( v, [typeIdx, kind]( const std::string& id ) { CtrlStartJob( typeIdx, kind, CalcMode::Auto, id == "yes" ); } );
  }

  inline void CtrlOpenPolygonKindMenu( int typeIdx ) {  // rodzaj ciagu poligonowego, potem pytanie o prawdziwy blad
    MenuView v{ "Rodzaj ciagu poligonowego", { { "anchored", "Ciag nawiazany obustronnie (N1, A ... B, N2)", "kontrola katowa i liniowa; potrzebne 4 punkty osnowy", true },
                                               { "closed", "Poligon zamkniety (z A do A)", "kontrola z samej figury; punkt A i N1 z osnowy", true },
                                               { "open", "Ciag wiszacy (jednostronnie nawiazany)", "bez kontroli: krotki, tylko do sprawdzenia", true } } };
    MenuOpen( v, [typeIdx]( const std::string& id ) {
      CtrlOpenTrueErrorMenu( typeIdx, id == "anchored" ? TraverseKind::Anchored : id == "closed" ? TraverseKind::Closed : TraverseKind::Open );
    } );
  }

  inline void CtrlOpenStartMenu() {
    MenuView v{ "Rozpocznij robote - typ roboty", {} };
    const auto& types = JobTypes();
    for ( size_t i = 0; i < types.size(); i++ ) v.items.push_back( { std::to_string( i ), types[i].name, "", true } );
    MenuOpen( v, []( const std::string& id ) {
      const int idx = std::stoi( id );
      if ( JobTypes()[idx].scan ) CtrlStartJob( idx, TraverseKind::Open, CalcMode::Auto );
      else if ( JobTypes()[idx].polygon ) CtrlOpenPolygonKindMenu( idx );
      else if ( JobTypes()[idx].gnss || JobTypes()[idx].tachy || JobTypes()[idx].aerial ) CtrlOpenTrueErrorMenu( idx ); else CtrlOpenKindMenu( idx );
    } );
  }

  inline void CtrlConfirmAbort() {                     // domyślnie „Nie”, żeby jeden Enter nie kasował roboty
    MenuOpen( MenuView{ "Przerwac robote?", { { "no", "Nie, wracam do roboty", "", true },
                                              { "yes", "Tak, przerwij (zapisze to, co zmierzono)", "", true } } },
              []( const std::string& id ) {
      if ( id != "yes" || !gJob ) return;
      const std::string dir = gJob->Kind() == JobKind::Gnss ? CtrlSavePartialGnss( gJob->FinishGnss( false ) )
                            : gJob->Kind() == JobKind::Tachy ? CtrlSavePartialTachy( gJob->FinishTachy( false ) )
                            : gJob->Kind() == JobKind::Polygon ? CtrlSavePartialPolygon( gJob->FinishPolygon( false ) ) : CtrlSavePartial( gJob->Finish( false ) );
      HudMessage( dir.empty() ? "Robota przerwana" : ( "Robota przerwana, zapisano w folderze: " + dir ).c_str() );
      CtrlMarkEnd( MeasState::Aborted, dir.empty() ? gJob->ResultDir() : dir, "" );
      CtrlEndJob();
    } );
  }

  // Lista wyboru punktu osnowy (stanowisko / nawiazanie): wybor wywoluje OnMenu("<pozycja>:<punkt>") w robocie.
  inline void CtrlOpenChoice( const std::string& menuId ) {
    std::vector<MenuItem> items = gJob->TachyChoices( menuId );
    if ( items.empty() ) {
      if ( menuId.compare( 0, 4, "ref_" ) == 0 ) { HudMessage( "Brak wolnych punktow osnowy w tym swiecie (zaladuj osnowe albo zaloz punkty w robocie GNSS)" ); return; }
      HudMessage( menuId == "station" ? "Brak punktu osnowy w promieniu 2 m od ciebie" : menuId == "stake" ? "Brak punktow osnowy 3-300 m od stanowiska do wytyczenia" : "Brak wolnych punktow osnowy 3-300 m od stanowiska" );
      return;
    }
    MenuOpen( MenuView{ menuId == "redo" ? "Wybierz stanowisko do powtorzenia" : menuId == "ref_n1" ? "Wybierz nawiazanie poczatkowe N1" : menuId == "ref_a" ? "Wybierz pierwszy punkt ciagu A" : menuId == "ref_b" ? "Wybierz ostatni punkt ciagu B" : menuId == "ref_n2" ? "Wybierz nawiazanie koncowe N2" :
                        menuId == "station" ? "Wybierz punkt osnowy na stanowisko" : menuId == "stake" ? "Wybierz punkt osnowy do wytyczenia" : "Wybierz punkt osnowy na nawiazanie", items },
              []( const std::string& id ) { if ( gJob ) gJob->OnMenu( id ); } );
  }

  // Podglad osnowy biezacego swiata (z pliku GothicSurveying/osnowa.csv): od najblizszych, z odlegloscia i kierunkiem od bohatera.
  inline void CtrlShowControl() {
    ControlEnsureLoaded();
    const std::string wn = ControlWorldName();
    const zVEC3 hero = player ? player->GetPositionWorld() : zVEC3( 0, 0, 0 );
    std::vector<ControlPoint> pts = ControlNear( gControl, wn, hero[VX], hero[VZ], 0.0, 1e9 );    // posortowane rosnaco wg odleglosci
    std::vector<std::string> lines;
    for ( const ControlPoint& c : pts ) {
      const double dx = c.x - hero[VX], dz = c.z - hero[VZ];
      char b[220]; sprintf( b, "%s  %.0f m %s  N %.3f  E %.3f  H %.3f  %s", c.id.c_str(), sqrt( dx * dx + dz * dz ) / 100.0, ControlCompass( dx, dz ), c.n, c.e, c.h, ControlDisplaySolution( c ).c_str() );
      lines.push_back( b );
    }
    if ( lines.empty() ) lines.push_back( "Brak punktow osnowy w tym swiecie. Zaladuj osnowe swiata albo zaloz punkty w robocie GNSS." );
    TextPanelShow( "Osnowa - swiat " + wn + " (od najblizszych)", lines, "Plik: GothicSurveying/osnowa.csv" );
  }

  // "Zaladuj osnowe geodezyjna swiata": wzorcowe punkty swiata trafiaja do osnowy (znaki w swiecie), potem lista od najblizszych.
  inline void CtrlLoadWorldControl() {
    const std::string wn = ControlWorldName();
    const size_t total = WorldControlFor( wn ).size();
    SvTrace( ( "osnowa wzorcowa: swiat \"" + wn + "\" (klucz " + ControlWorldKey( wn ) + "), punktow w zestawie: " + std::to_string( total ) ).c_str() );
    if ( total == 0 ) { HudMessage( ( "Brak osnowy wzorcowej dla swiata " + wn ).c_str() ); return; }
    const int added = ControlLoadWorldSet();
    char b[160];
    if ( added > 0 ) sprintf( b, "Zaladowano osnowe swiata: %d punktow (G01...)", added );
    else sprintf( b, "Osnowa swiata jest juz zaladowana (%d punktow)", (int)total );
    HudMessage( b );
    CtrlShowControl();
  }

  // Teleport do punktu osnowy: lista punktow biezacego swiata (wg numerow z mapy), 10 na strone.
  inline void CtrlOpenTeleport( int page ) {
    ControlEnsureLoaded();
    const std::string wn = ControlWorldName();
    std::vector<ControlPoint> pts = ControlNear( gControl, wn, 0.0, 0.0, 0.0, 1e12 );
    ControlSortById( pts );
    if ( pts.empty() ) { HudMessage( "Brak punktow osnowy w tym swiecie (Zaladuj osnowe geodezyjna swiata)" ); return; }
    const int per = 10, pages = ( (int)pts.size() + per - 1 ) / per;
    page = ( std::max )( 0, ( std::min )( page, pages - 1 ) );
    const zVEC3 hero = player ? player->GetPositionWorld() : zVEC3( 0, 0, 0 );
    MenuView v{ "Teleport do punktu osnowy (" + std::to_string( page + 1 ) + "/" + std::to_string( pages ) + ")", {} };
    for ( int i = page * per; i < (int)pts.size() && i < ( page + 1 ) * per; i++ ) {
      const ControlPoint& c = pts[i]; const double dx = c.x - hero[VX], dz = c.z - hero[VZ];
      char b[200]; sprintf( b, "%s  %.0f m %s", c.id.c_str(), sqrt( dx * dx + dz * dz ) / 100.0, ControlCompass( dx, dz ) );
      v.items.push_back( { "tp:" + c.id, b, "", true } );
    }
    if ( page + 1 < pages ) v.items.push_back( { "tp_next", "Nastepna strona", "", true } );
    if ( page > 0 ) v.items.push_back( { "tp_prev", "Poprzednia strona", "", true } );
    MenuOpen( v, [page]( const std::string& id ) {
      if ( id == "tp_next" ) { CtrlOpenTeleport( page + 1 ); return; }
      if ( id == "tp_prev" ) { CtrlOpenTeleport( page - 1 ); return; }
      for ( const ControlPoint& c : gControl )
        if ( "tp:" + c.id == id && ControlWorldKey( c.world ) == ControlWorldKey( ControlWorldName() ) ) {
          if ( player ) {
            zVEC3 to( (float)c.x, (float)c.y + 100.0f, (float)c.z );     // 1 m nad znakiem, bohater opada na teren
            const zVEC3 from = player->GetPositionWorld();
            player->ResetPos( to );                                       // jak w silniku: przestawia tez AI/fizyke bohatera (samo SetPositionWorld wraca)
            const zVEC3 now = player->GetPositionWorld();
            char t[200]; sprintf( t, "teleport %s: z (%.0f,%.0f,%.0f) cel (%.0f,%.0f,%.0f) po (%.0f,%.0f,%.0f)", c.id.c_str(), from[VX], from[VY], from[VZ], to[VX], to[VY], to[VZ], now[VX], now[VY], now[VZ] );
            SvTrace( t );
          }
          HudMessage( ( "Teleport: punkt " + c.id ).c_str() );
          return;
        }
    } );
  }

  // Usuwanie osnowy: potwierdzenie (domyslnie "Nie"), potem skasowanie punktow z pliku i znakow ze swiata.
  inline void CtrlConfirmRemoveControl( const std::vector<std::string>& ids, const std::string& what ) {
    if ( ids.empty() ) { HudMessage( "Brak takich punktow osnowy w tym swiecie" ); return; }
    MenuOpen( MenuView{ "Usunac " + what + "?", { { "no", "Nie, zostaw", "", true },
                                                   { "yes", "Tak, usun (" + std::to_string( ids.size() ) + " pkt, nieodwracalne)", "", true } } },
              [ids]( const std::string& id ) {
      if ( id != "yes" ) return;
      HudMessage( ( "Usunieto punktow osnowy: " + std::to_string( ControlRemoveIds( ids ) ) ).c_str() );
    } );
  }

  inline void CtrlOpenRemoveList( int page ) {
    ControlEnsureLoaded();
    std::vector<ControlPoint> pts = ControlNear( gControl, ControlWorldName(), 0.0, 0.0, 0.0, 1e12 );
    ControlSortById( pts );
    if ( pts.empty() ) { HudMessage( "Brak punktow osnowy w tym swiecie" ); return; }
    const int per = 10, pages = ( (int)pts.size() + per - 1 ) / per;
    page = ( std::max )( 0, ( std::min )( page, pages - 1 ) );
    const zVEC3 hero = player ? player->GetPositionWorld() : zVEC3( 0, 0, 0 );
    MenuView v{ "Usun punkt osnowy (" + std::to_string( page + 1 ) + "/" + std::to_string( pages ) + ")", {} };
    for ( int i = page * per; i < (int)pts.size() && i < ( page + 1 ) * per; i++ ) {
      const ControlPoint& c = pts[i]; const double dx = c.x - hero[VX], dz = c.z - hero[VZ];
      char b[200]; sprintf( b, "%s  %.0f m %s", c.id.c_str(), sqrt( dx * dx + dz * dz ) / 100.0, ControlCompass( dx, dz ) );
      v.items.push_back( { "rm:" + c.id, b, "", true } );
    }
    if ( page + 1 < pages ) v.items.push_back( { "rm_next", "Nastepna strona", "", true } );
    if ( page > 0 ) v.items.push_back( { "rm_prev", "Poprzednia strona", "", true } );
    MenuOpen( v, [page]( const std::string& id ) {
      if ( id == "rm_next" ) { CtrlOpenRemoveList( page + 1 ); return; }
      if ( id == "rm_prev" ) { CtrlOpenRemoveList( page - 1 ); return; }
      if ( id.compare( 0, 3, "rm:" ) == 0 ) CtrlConfirmRemoveControl( { id.substr( 3 ) }, "punkt " + id.substr( 3 ) );
    } );
  }

  inline void CtrlOpenRemoveMenu() {
    ControlEnsureLoaded();
    MenuView v{ "Usun osnowe (biezacy swiat)", { { "one", "Usun jeden punkt (z listy)", "", true },
                                                  { "g", "Usun punkty wzorcowe swiata (G..)", "", true },
                                                  { "o", "Usun punkty zalozone GNSS (O..)", "", true },
                                                  { "all", "Usun cala osnowe swiata", "", true } } };
    MenuOpen( v, []( const std::string& id ) {
      if ( id == "one" ) { CtrlOpenRemoveList( 0 ); return; }
      std::vector<std::string> ids;
      for ( const ControlPoint& c : ControlNear( gControl, ControlWorldName(), 0.0, 0.0, 0.0, 1e12 ) )
        if ( id == "all" || ( id == "g" && c.id[0] == 'G' ) || ( id == "o" && c.id[0] == 'O' ) ) ids.push_back( c.id );
      CtrlConfirmRemoveControl( ids, id == "all" ? "cala osnowe swiata" : id == "g" ? "punkty wzorcowe swiata" : "punkty zalozone GNSS" );
    } );
  }

  // Narzedzia niezalezne od projektu: osnowa swiata, mapa, teleport, usuwanie osnowy, poziomowanie instrumentow. Lewo wraca na ekran glowny.
  inline void CtrlOpenToolsMenu() {
    MenuOpen( MenuView{ "Osnowa i narzedzia", { { "world", "Zaladuj osnowe geodezyjna swiata", "", true }, { "control", "Pokaz osnowe", "", true }, { "map", "Mapa osnowy", "", true }, { "tp", "Teleport do punktu osnowy", "", true }, { "rm", "Usun osnowe", "", true },
                                             { "lvl", gLevelingOn ? "Poziomowanie instrumentow: wlaczone (zmien)" : "Poziomowanie instrumentow: wylaczone (zmien)", "dotyczy nastepnych stanowisk", true } }, "GEODEZJA > Narzedzia" },
              []( const std::string& id ) { if ( id == "lvl" ) { gLevelingOn = !gLevelingOn; HudMessage( gLevelingOn ? "Poziomowanie instrumentow wlaczone" : "Poziomowanie instrumentow wylaczone" ); } else if ( id == "control" ) CtrlShowControl(); else if ( id == "world" ) CtrlLoadWorldControl(); else if ( id == "map" ) MapOpen(); else if ( id == "tp" ) CtrlOpenTeleport( 0 ); else if ( id == "rm" ) CtrlOpenRemoveMenu(); },
              []() { ProjUiOpenHome(); } );
  }

  inline void CtrlOpenMenu() {
    ProjUiInstallMenuHooks();                           // F1 i pamiec kursora dzialaja tez, gdy robote przywrocono z zapisu bez ekranu glownego
    if ( !gJob ) { ProjUiOpenHome(); return; }
    MenuView v{ gJob->Name(), gJob->Menu() };
    if ( gJob->Kind() == JobKind::Leveling ) v.items.push_back( { "scope", "Patrz przez lunete (" LV_KEYNAME_SCOPE ")", "najpierw niwelator i lata", gWorld.HasInstrument() && !gJob->ActiveRodPoints().empty() } );
    if ( gJob->Kind() == JobKind::Leveling ) { char b[64]; sprintf( b, "Wysun laty: teraz %.0f m (zmien na %.0f m)", gRodLenCm / 100.0f, gRodLenCm >= 500.0f ? 3.0f : gRodLenCm / 100.0f + 1.0f ); v.items.push_back( { "rodlen", b, "3, 4 albo 5 m", true } ); }
    if ( gJob->GetLibella() && gJob->GetLibella()->enabled && gWorld.HasInstrument() )
      v.items.push_back( { "level", "Poziomuj instrument (libella)", "", true } );
    if ( gJob->Kind() == JobKind::Aerial || gJob->Kind() == JobKind::Photo ) v.items.push_back( { "preview", "Podglad planu nalotu", "najpierw zdefiniuj obszar (min. 3 wierzcholki)", !gJob->MapOverlay().empty() } );
    v.items.push_back( { "map", "Mapa osnowy", "", true } );
    v.items.push_back( { "tp", "Teleport do punktu osnowy", "", true } );
    v.items.push_back( { "abort", "Przerwij robote (bez dziennika)", "", true } );
    v.items = Arrange( gJob->Kind(), v.items );                   // sekcje, stala kolejnosc, powody niedostepnosci; "Przerwij" zawsze ostatnie
    const Next nx = NextLine( v.items, gJob->NextStep() );        // rekomendowany krok
    v.nextText = nx.text; v.nextId = nx.id; v.crumbs = "GEODEZJA > Projekt: " + gProject.name + " > Pomiar";
    v.helpContext = HelpContextFor( gJob->Kind() ); v.screenKey = v.helpContext;
    bool guided = false; for ( const Measurement& m : gProject.items ) if ( m.id == gJobMeasId ) guided = m.guided;
    if ( guided && !nx.id.empty() ) v.items = GuidedItems( v.items, nx.id );   // tryb prowadzony: nastepny krok + zakonczenie/koniec ciagu, "Pokaz wszystko", Przerwij
    MenuOpen( v, []( const std::string& id ) {
      if ( !gJob ) return;
      if ( id == "showall" ) { for ( Measurement& m : gProject.items ) if ( m.id == gJobMeasId ) m.guided = false; ProjSave(); CtrlOpenMenu(); return; }
      if ( id == "finish" ) CtrlFinish();
      else if ( id == "scope" ) CtrlEnterScope();
      else if ( id == "fine" ) PoleFineEnter();
      else if ( id == "view" ) { gJob->OnMenu( id ); CtrlEnterTachyView(); }
      else if ( gJob->NeedsChoice( id ) ) CtrlOpenChoice( id );
      else if ( id == "rodlen" ) { gRodLenCm = gRodLenCm >= 500.0f ? 300.0f : gRodLenCm + 100.0f; gWorld.RefreshRods(); HudMessage( ( "Lata wysunieta na " + std::to_string( (int)( gRodLenCm / 100.0f ) ) + " m" ).c_str() ); }
      else if ( id == "level" ) LevelOpen( gJob->GetLibella(), gJob->Kind() == JobKind::Leveling || gJob->Kind() == JobKind::Scan ? kLevelBubbleArcmin : kTachyLevelArcmin, gJob->Kind() == JobKind::Leveling ? "niwelatora" : gJob->Kind() == JobKind::Scan ? "skanera" : "tachimetru" );
      else if ( id == "map" ) MapOpen();
      else if ( id == "preview" ) MapOpenPlan( gJob->MapOverlay() );
      else if ( id == "area_map" ) MapOpen( true, []( double e1, double n1, double e2, double n2, double ang ) {   // wspolrzedne mapy (metry ukladu gry) -> siatka N/E
        if ( !gJob ) return;
        char b[200]; sprintf( b, "area_rect:%.3f:%.3f:%.3f:%.3f:%.3f", n1 + GNSS_N0, e1 + GNSS_E0, n2 + GNSS_N0, e2 + GNSS_E0, ang );
        gJob->OnMenu( b ); HudMessage( "Obszar zaznaczony na mapie" );
      } );
      else if ( id == "tp" ) CtrlOpenTeleport( 0 );
      else if ( id == "abort" ) CtrlConfirmAbort();
      else {
        const size_t before = gJob->Kind() == JobKind::Gnss ? gJob->FinishGnss( false ).points.size() : 0;
        gJob->OnMenu( id );
        if ( gJob->Kind() == JobKind::Gnss && ( id == "store" || id == "control" ) ) {   // każdy zapisany punkt od razu do logu (przetrwa awarię gry)
          const GnssReport r = gJob->FinishGnss( false );
          if ( r.points.size() > before ) AppendGnssLog( gReadingLog, r.points.back(), (int)r.points.size(), r.showTrueError );
        }
      }
    } );
  }

  inline void CtrlUpdateJobHud() {
    if ( !gJob ) { CtrlCloseJobHud(); return; }
    std::string text = gJob->Hud();
    { const std::string ls = CtrlLevelState(); if ( !ls.empty() ) text += "   |   Instrument: " + ls; }
    if ( gJobHud && text == gJobHudText ) return;
    PanelClose( gJobHud );
    gJobHud = PanelOpen( 600, 7350, 7600, 8000 );
    gJobHud->PrintCXY( zSTRING( text.c_str() ) );
    gJobHudText = text;
  }

  inline void CtrlUpdateStatusPanel() {                // odbiornik GNSS: linie statusu w lewym górnym rogu, odświeżane po zmianie
    const std::vector<std::string> lines = gJob ? gJob->StatusLines() : std::vector<std::string>();
    if ( lines.empty() ) { PanelClose( gStatusPanel ); gStatusText.clear(); return; }
    std::string text; for ( const std::string& l : lines ) text += l + "|";
    if ( gStatusPanel && text == gStatusText ) return;
    PanelClose( gStatusPanel );
    const int H = 130 + (int)lines.size() * 235 + 150;   // stały odstęp wierszy [jednostki ekranu]; Print liczy y w 8192 na wysokość panelu
    gStatusPanel = PanelOpen( 150, 150, 3900, 150 + H );
    for ( size_t i = 0; i < lines.size(); i++ ) gStatusPanel->Print( 300, ( 130 + (int)i * 235 ) * 8192 / H, zSTRING( lines[i].c_str() ) );
    gStatusText = text;
  }

  // --- zapis gry w trakcie roboty: stan roboty i sprzetu w pliku GothicSurveying/stan_slot<N>.txt ---
  inline std::string CtrlStatePath( int slot ) { return std::string( LV_OUT_DIR ) + "/stan_slot" + std::to_string( slot ) + ".txt"; }

  inline void CtrlSaveJobState( int slot ) {          // wywoluje hook zapisu gry; bez roboty kasuje stary stan tego slotu
    namespace fs = std::filesystem; std::error_code ec;
    const std::string path = CtrlStatePath( slot );
    if ( !gJob || gJobTypeIdx < 0 || !gJob->CanSaveState() ) { fs::remove( path, ec ); return; }
    StateKV job, world, top;
    gJob->SaveState( job ); gWorld.SaveState( world );
    top.Int( "ver", 1 ); top.Str( "world", ControlWorldKey( ControlWorldName() ) ); top.Str( "type", JobTypes()[gJobTypeIdx].id );
    top.Int( "kind", (int)gJobOpts.kind ); top.Int( "mode", (int)gJobOpts.mode ); top.Bool( "trueErr", gJobOpts.showTrueError );
    top.Str( "job", job.ToText() ); top.Str( "wld", world.ToText() );
    top.Str( "project", gProject.name ); top.Str( "pomiar", gJobMeasId );        // opcjonalne (stare zapisy ich nie maja)
    fs::create_directories( LV_OUT_DIR, ec );
    WriteBytes( path, top.ToText() );
    SvTrace( ( "stan roboty zapisany: " + path ).c_str() );
  }

  // Po wczytaniu zapisu: odtwarza robote i sprzet ze stanu slotu. false = brak stanu albo inny swiat/typ.
  inline bool CtrlRestoreJobState( int slot ) {
    std::ifstream f( CtrlStatePath( slot ), std::ios::binary );
    if ( !f.good() ) return false;
    const std::string text( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
    const StateKV top = StateKV::FromText( text );
    if ( top.GetInt( "ver" ) != 1 || top.GetStr( "world" ) != ControlWorldKey( ControlWorldName() ) ) return false;
    int idx = -1; for ( size_t i = 0; i < JobTypes().size(); i++ ) if ( JobTypes()[i].id == top.GetStr( "type" ) ) idx = (int)i;
    if ( idx < 0 ) return false;
    gJobOpts = JobOptions{ (TraverseKind)top.GetInt( "kind" ), (CalcMode)top.GetInt( "mode" ), top.GetBool( "trueErr" ) };
    std::unique_ptr<Job> job = JobTypes()[idx].create( gWorld, gJobOpts );
    if ( !job->LoadState( StateKV::FromText( top.GetStr( "job" ) ) ) ) return false;
    gJob = std::move( job ); gJobTypeIdx = idx;
    {                                                   // projekt: zapis wskazuje projekt i pomiar; stare zapisy (bez kluczy) trafiaja do projektu "Luzem_<swiat>"
      const std::string pname = top.GetStr( "project" ), pid = top.GetStr( "pomiar" );
      Project p; bool found = false;
      if ( !pname.empty() && LoadProject( ProjRoot(), pname, p ) && p.world == ProjWorldKey() && !p.closed ) found = true;   // zamkniety projekt nie przyjmuje wznowionego pomiaru
      if ( !found && !LoadProject( ProjRoot(), "Luzem_" + ProjSanitize( ProjWorldKey(), 24 ), p ) ) p = ProjMake( "Luzem_" + ProjSanitize( ProjWorldKey(), 24 ) );
      gProject = p;
      gJobMeasId = AttachRestored( gProject, found ? pid : std::string(), JobTypes()[idx].id, JobTypes()[idx].name );   // gotowy pomiar nie wraca do "w toku": powstaje nowy
      Reconcile( gProject, gJobMeasId ); ProjSave(); UiRememberProject( gProject.name );
    }
    gWorld.LoadState( StateKV::FromText( top.GetStr( "wld" ) ) );
    gReadingLog = PrepareReadingLog();
    gWorld.DressOn();
    HudMessage( ( "Wznowiono robote: " + gJob->Name() ).c_str() );
    SvTrace( ( "robota wznowiona ze stanu: " + CtrlStatePath( slot ) ).c_str() );
    return true;
  }

  inline void ControllerTick() {
    zCWorld* w = ogame->GetWorld();
    const int loadSlot = gPendingLoadSlot; gPendingLoadSlot = -2;     // wczytanie zapisu zakonczone (hook zadzialal przed ta klatka)
    if ( loadSlot != -2 || w != gCtrlWorld || ( w && gWorld.Stale() ) ) {   // inny świat albo ten sam obiekt świata po wczytaniu zapisu (voby zniknęły)                           // zmiana świata (wczytanie zapisu): zapisz, co się zmierzyło, i posprzątaj
      if ( gJob ) {
        std::string dir;
        if ( gJob->Kind() == JobKind::Gnss ) dir = CtrlSavePartialGnss( gJob->FinishGnss( false ) );
        else if ( gJob->Kind() == JobKind::Tachy ) dir = CtrlSavePartialTachy( gJob->FinishTachy( false ) );
        else if ( gJob->Kind() == JobKind::Polygon ) dir = CtrlSavePartialPolygon( gJob->FinishPolygon( false ) );
        else dir = CtrlSavePartial( gJob->Finish( false ) );
        CtrlMarkEnd( MeasState::Aborted, dir.empty() ? gJob->ResultDir() : dir, "" );
      }
      if ( FieldBookEditActive() ) { gPendingFb.complete = false; ExportFieldBook( gPendingFb, gJobDir, true ); }
      gMenuAfterPanel = nullptr; TextInputClose(); FieldBookEditCancel(); TextPanelClose(); FieldBookClose(); MapClose(); LevelClose(); CtrlForgetJob(); gCtrlWorld = w;
      if ( loadSlot != -2 && w ) CtrlRestoreJobState( loadSlot );
      if ( w && !gJob ) { gProject = Project(); ProjEnsure(); Reconcile( gProject, "" ); ProjSave(); }   // brak roboty po zmianie swiata/zapisu: "w toku" w manifescie jest nieaktualne
    }
    if ( !w ) return;
    ControlTick();                                     // znaki osnowy: nowy swiat / wczytany zapis
    static DWORD lastClock = GetTickCount();
    const DWORD nowClock = GetTickCount();
    double dtClock = ( nowClock - lastClock ) / 1000.0; lastClock = nowClock;
    if ( dtClock > 0.2 ) dtClock = 0.2;
    const bool gamePaused = MenuIsOpen() || ScopeActive() || FieldBookVisible() || TextPanelVisible() || TextInputActive() || FieldBookEditActive() || PoleFineActive() || TachyViewActive() || MapActive() || LevelActive();
    gWorld.ScanVisualTick();                           // wiazki skanera laserowego
    ControlLabelTick( gamePaused );                    // etykieta najblizszego punktu osnowy
    if ( !gamePaused ) { gWorld.AdvanceClock( dtClock ); if ( gJob ) gJob->Tick(); gWorld.FollowCamTick(); }   // zegar symulacji stoi, gdy gra zatrzymana
    if ( !MenuIsOpen() && !ScopeActive() && !PoleFineActive() && !TachyViewActive() ) gWorld.DressTick();   // przy zatrzymanej grze nic się nie rusza
    ScopeTick();
    PoleFineTick();
    TachyViewTick();
    HudTick();
    if ( ScopeActive() || PoleFineActive() || TachyViewActive() ) { CtrlCloseJobHud(); return; }
    if ( FieldBookEditActive() ) { if ( FieldBookEditTick() ) CtrlFinishManual(); return; }
    if ( MapActive() ) { MapTick(); return; }
    if ( LevelActive() ) { LevelTick(); return; }
    if ( TextInputActive() ) { TextInputTick(); return; }
    if ( TextPanelVisible() ) { TextPanelTick(); return; }
    if ( gMenuAfterPanel && !FieldBookVisible() ) { auto reopen = gMenuAfterPanel; gMenuAfterPanel = nullptr; reopen(); return; }   // panel z menu (F1, lista plikow) zamkniety: wroc do menu
    if ( FieldBookVisible() ) { FieldBookTick(); return; }
    if ( MenuIsOpen() ) { MenuTick(); return; }
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { CtrlOpenMenu(); return; }
    if ( gJob && gJob->Kind() == JobKind::Leveling && zinput->KeyToggled( LV_KEY_SCOPE ) ) CtrlEnterScope();
    if ( gJob && ( gJob->Kind() == JobKind::Tachy || gJob->Kind() == JobKind::Polygon ) && zinput->KeyToggled( LV_KEY_SCOPE ) ) {
      bool canView = false; for ( const MenuItem& m : gJob->Menu() ) if ( m.id == "view" && m.enabled ) canView = true;
      if ( canView ) { gJob->OnMenu( "view" ); CtrlEnterTachyView(); } else HudMessage( gJob->Kind() == JobKind::Polygon ? "Najpierw tachimetr i lustro (menu)" : "Najpierw orientacja i tyczka z lustrem" );
    }
    CtrlUpdateJobHud();
    CtrlUpdateStatusPanel();
  }
}
