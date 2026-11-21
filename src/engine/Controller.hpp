// Kontroler: menu startowe (typ roboty -> rodzaj -> tryb liczenia), menu aktywnej roboty, luneta, HUD kroku. Bez strażnika include.
#include <memory>

namespace GOTHIC_NAMESPACE {
  inline EngineWorld        gWorld;
  inline std::unique_ptr<Job> gJob;
  inline zCWorld*           gCtrlWorld = nullptr;
  inline zCView*            gJobHud = nullptr;       // stały panel z bieżącym krokiem roboty
  inline std::string        gJobHudText;
  inline std::string        gReadingLog;              // GothicSurveying/odczyty_biezace.csv: log odczytów bieżącej roboty (po końcu trafia do folderu roboty)
  inline std::string        gJobDir;                  // folder zakończonej roboty: GothicSurveying/<data_godzina>

  inline FieldBook          gPendingFb;               // dziennik oczekujący na wpisy gracza (tryb samodzielny)

  inline zCView*            gStatusPanel = nullptr;  // panel odbiornika GNSS (linie ze Job::StatusLines)
  inline std::string        gStatusText;

  inline void CtrlCloseJobHud() { PanelClose( gJobHud ); gJobHudText.clear(); PanelClose( gStatusPanel ); gStatusText.clear(); }
  inline void CtrlEndJob() { gJob.reset(); gWorld.ClearAll(); CtrlCloseJobHud(); }
  inline void CtrlForgetJob() {                        // po zmianie świata: obiekty zniknęły razem ze światem
    gWorld.ForgetAll();                                // najpierw oddaj vob-y, potem callbacki lunety nie dotkną starych obiektów
    ScopeLeave(); MenuClose(); gJob.reset(); CtrlCloseJobHud();
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
      sprintf( b, "%s  %s  N %.3f  E %.3f  H %.3f  PDOP %.1f  RMS %.0f/%.0f mm", p.name.c_str(), p.solution.c_str(), p.n, p.e, p.h, p.pdop, p.hRmsMm, p.vRmsMm );
      lines.push_back( b );
      if ( r.showTrueError && p.hasTrue ) {
        sprintf( b, "    blad: N %.1f  E %.1f  H %.1f  2D %.1f mm", p.dNmm, p.dEmm, p.dHmm, p.d2Dmm );
        lines.push_back( b );
      }
    }
    TextPanelShow( "Pomiar GNSS - podsumowanie", lines, ok ? "Zapisano w folderze: " + gJobDir : "BLAD ZAPISU plikow dziennika" );
    SvTrace( "Pomiar GNSS zakonczony" );
    CtrlEndJob();
  }

  inline void CtrlFinish() {
    if ( !gJob ) return;
    if ( gJob->Kind() == JobKind::Gnss ) { CtrlFinishGnss(); return; }
    FieldBook fb = gJob->Finish( true );
    gJobDir = MakeJobDir();                            // nazwa folderu = czas zakończenia roboty
    MoveReadingLog( gReadingLog, gJobDir );
    if ( fb.mode == CalcMode::Manual ) {               // tryb samodzielny: gracz sam liczy dziennik w edytorze
      gPendingFb = fb; CtrlEndJob(); FieldBookEditStart( fb );
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
    gJob = JobTypes()[typeIdx].create( gWorld, JobOptions{ kind, mode, showTrueError } );
    gReadingLog = PrepareReadingLog();
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

  inline void CtrlOpenTrueErrorMenu( int typeIdx ) {   // opcja robót GNSS: pokaz prawdziwy błąd względem punktu w grze
    MenuView v{ "Pokazywac prawdziwy blad po pomiarze?", { { "no", "Nie (tylko wynik odbiornika)", "", true },
                                                           { "yes", "Tak (blad wzgledem punktu w grze)", "", true } } };
    MenuOpen( v, [typeIdx]( const std::string& id ) { CtrlStartJob( typeIdx, TraverseKind::Open, CalcMode::Auto, id == "yes" ); } );
  }

  inline void CtrlOpenStartMenu() {
    MenuView v{ "Rozpocznij robote - typ roboty", {} };
    const auto& types = JobTypes();
    for ( size_t i = 0; i < types.size(); i++ ) v.items.push_back( { std::to_string( i ), types[i].name, "", true } );
    MenuOpen( v, []( const std::string& id ) {
      const int idx = std::stoi( id );
      if ( JobTypes()[idx].gnss ) CtrlOpenTrueErrorMenu( idx ); else CtrlOpenKindMenu( idx );
    } );
  }

  inline void CtrlConfirmAbort() {                     // domyślnie „Nie”, żeby jeden Enter nie kasował roboty
    MenuOpen( MenuView{ "Przerwac robote?", { { "no", "Nie, wracam do roboty", "", true },
                                              { "yes", "Tak, przerwij (zapisze to, co zmierzono)", "", true } } },
              []( const std::string& id ) {
      if ( id != "yes" || !gJob ) return;
      const std::string dir = gJob->Kind() == JobKind::Gnss ? CtrlSavePartialGnss( gJob->FinishGnss( false ) ) : CtrlSavePartial( gJob->Finish( false ) );
      HudMessage( dir.empty() ? "Robota przerwana" : ( "Robota przerwana, zapisano w folderze: " + dir ).c_str() );
      CtrlEndJob();
    } );
  }

  inline void CtrlOpenMenu() {
    if ( !gJob ) {
      MenuOpen( MenuView{ "Menu geodezyjne", { { "start", "Rozpocznij robote", "", true } } }, []( const std::string& ) { CtrlOpenStartMenu(); } );
      return;
    }
    MenuView v{ gJob->Name(), gJob->Menu() };
    if ( gJob->Kind() == JobKind::Leveling ) v.items.push_back( { "scope", "Patrz przez lunete (" LV_KEYNAME_SCOPE ")", "najpierw niwelator i lata", gWorld.HasInstrument() && !gJob->ActiveRodPoints().empty() } );
    v.items.push_back( { "abort", "Przerwij robote (bez dziennika)", "", true } );
    MenuOpen( v, []( const std::string& id ) {
      if ( !gJob ) return;
      if ( id == "finish" ) CtrlFinish();
      else if ( id == "scope" ) CtrlEnterScope();
      else if ( id == "abort" ) CtrlConfirmAbort();
      else {
        gJob->OnMenu( id );
        if ( id == "store" && gJob->Kind() == JobKind::Gnss ) {   // każdy zapisany punkt od razu do logu (przetrwa awarię gry)
          const GnssReport r = gJob->FinishGnss( false );
          if ( !r.points.empty() ) AppendGnssLog( gReadingLog, r.points.back(), (int)r.points.size(), r.showTrueError );
        }
      }
    } );
  }

  inline void CtrlUpdateJobHud() {
    if ( !gJob ) { CtrlCloseJobHud(); return; }
    std::string text = gJob->Hud();
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
    gStatusPanel = PanelOpen( 150, 150, 3700, 1650 );
    for ( size_t i = 0; i < lines.size(); i++ ) gStatusPanel->Print( 400, 700 + (int)i * 1700, zSTRING( lines[i].c_str() ) );
    gStatusText = text;
  }

  inline void ControllerTick() {
    zCWorld* w = ogame->GetWorld();
    if ( w != gCtrlWorld || ( w && gWorld.Stale() ) ) {   // inny świat albo ten sam obiekt świata po wczytaniu zapisu (voby zniknęły)                           // zmiana świata (wczytanie zapisu): zapisz, co się zmierzyło, i posprzątaj
      if ( gJob ) { if ( gJob->Kind() == JobKind::Gnss ) CtrlSavePartialGnss( gJob->FinishGnss( false ) ); else CtrlSavePartial( gJob->Finish( false ) ); }
      if ( FieldBookEditActive() ) { gPendingFb.complete = false; ExportFieldBook( gPendingFb, gJobDir, true ); }
      FieldBookEditCancel(); TextPanelClose(); FieldBookClose(); CtrlForgetJob(); gCtrlWorld = w;
    }
    if ( !w ) return;
    static DWORD lastClock = GetTickCount();
    const DWORD nowClock = GetTickCount();
    double dtClock = ( nowClock - lastClock ) / 1000.0; lastClock = nowClock;
    if ( dtClock > 0.2 ) dtClock = 0.2;
    const bool gamePaused = MenuIsOpen() || ScopeActive() || FieldBookVisible() || TextPanelVisible() || FieldBookEditActive();
    if ( !gamePaused ) { gWorld.AdvanceClock( dtClock ); if ( gJob ) gJob->Tick(); }   // zegar symulacji stoi, gdy gra zatrzymana
    if ( !MenuIsOpen() && !ScopeActive() ) gWorld.DressTick();   // przy zatrzymanej grze nic się nie rusza
    ScopeTick();
    HudTick();
    if ( ScopeActive() ) { CtrlCloseJobHud(); return; }
    if ( FieldBookEditActive() ) { if ( FieldBookEditTick() ) CtrlFinishManual(); return; }
    if ( TextPanelVisible() ) { TextPanelTick(); return; }
    if ( FieldBookVisible() ) { FieldBookTick(); return; }
    if ( MenuIsOpen() ) { MenuTick(); return; }
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { CtrlOpenMenu(); return; }
    if ( gJob && gJob->Kind() == JobKind::Leveling && zinput->KeyToggled( LV_KEY_SCOPE ) ) CtrlEnterScope();
    CtrlUpdateJobHud();
    CtrlUpdateStatusPanel();
  }
}
