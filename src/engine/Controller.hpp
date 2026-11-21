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

  inline void CtrlCloseJobHud() { PanelClose( gJobHud ); gJobHudText.clear(); }
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

  inline void CtrlFinish() {
    if ( !gJob ) return;
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

  inline void CtrlStartJob( int typeIdx, TraverseKind kind, CalcMode mode ) {
    gJob = JobTypes()[typeIdx].create( gWorld, JobOptions{ kind, mode } );
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

  inline void CtrlOpenStartMenu() {
    MenuView v{ "Rozpocznij robote - typ roboty", {} };
    const auto& types = JobTypes();
    for ( size_t i = 0; i < types.size(); i++ ) v.items.push_back( { std::to_string( i ), types[i].name, "", true } );
    MenuOpen( v, []( const std::string& id ) { CtrlOpenKindMenu( std::stoi( id ) ); } );
  }

  inline void CtrlConfirmAbort() {                     // domyślnie „Nie”, żeby jeden Enter nie kasował roboty
    MenuOpen( MenuView{ "Przerwac robote?", { { "no", "Nie, wracam do roboty", "", true },
                                              { "yes", "Tak, przerwij (zapisze to, co zmierzono)", "", true } } },
              []( const std::string& id ) {
      if ( id != "yes" || !gJob ) return;
      FieldBook part = gJob->Finish( false );
      const std::string dir = CtrlSavePartial( part );
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
    v.items.push_back( { "scope", "Patrz przez lunete (" LV_KEYNAME_SCOPE ")", "najpierw niwelator i lata", gWorld.HasInstrument() && !gJob->ActiveRodPoints().empty() } );
    v.items.push_back( { "abort", "Przerwij robote (bez dziennika)", "", true } );
    MenuOpen( v, []( const std::string& id ) {
      if ( !gJob ) return;
      if ( id == "finish" ) CtrlFinish();
      else if ( id == "scope" ) CtrlEnterScope();
      else if ( id == "abort" ) CtrlConfirmAbort();
      else gJob->OnMenu( id );
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

  inline void ControllerTick() {
    zCWorld* w = ogame->GetWorld();
    if ( w != gCtrlWorld || ( w && gWorld.Stale() ) ) {   // inny świat albo ten sam obiekt świata po wczytaniu zapisu (voby zniknęły)                           // zmiana świata (wczytanie zapisu): zapisz, co się zmierzyło, i posprzątaj
      if ( gJob ) CtrlSavePartial( gJob->Finish( false ) );
      if ( FieldBookEditActive() ) { gPendingFb.complete = false; ExportFieldBook( gPendingFb, gJobDir, true ); }
      FieldBookEditCancel(); TextPanelClose(); FieldBookClose(); CtrlForgetJob(); gCtrlWorld = w;
    }
    if ( !w ) return;
    if ( !MenuIsOpen() && !ScopeActive() ) gWorld.DressTick();   // przy zatrzymanej grze nic się nie rusza
    ScopeTick();
    HudTick();
    if ( ScopeActive() ) { CtrlCloseJobHud(); return; }
    if ( FieldBookEditActive() ) { if ( FieldBookEditTick() ) CtrlFinishManual(); return; }
    if ( TextPanelVisible() ) { TextPanelTick(); return; }
    if ( FieldBookVisible() ) { FieldBookTick(); return; }
    if ( MenuIsOpen() ) { MenuTick(); return; }
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { CtrlOpenMenu(); return; }
    if ( gJob && zinput->KeyToggled( LV_KEY_SCOPE ) ) CtrlEnterScope();
    CtrlUpdateJobHud();
  }
}
