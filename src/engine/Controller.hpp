// Kontroler: menu startowe (typ roboty -> rodzaj -> tryb liczenia), menu aktywnej roboty, luneta, HUD kroku. Bez strażnika include.
#include <memory>

namespace GOTHIC_NAMESPACE {
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
    ScopeLeave(); PoleFineLeave(); TachyViewLeave(); MenuClose(); gJob.reset(); CtrlCloseJobHud();
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
  // Tachimetria: przerwana robota / zmiana swiata zapisuje zmierzone punkty do nowego folderu; "" gdy nie bylo punktow.
  inline std::string CtrlSavePartialTachy( const TachyReport& part ) {
    if ( part.points.empty() && !FileExistsRel( gReadingLog ) ) return "";
    const std::string dir = MakeJobDir();
    if ( !part.points.empty() ) ExportTachy( part, dir, true );
    MoveReadingLog( gReadingLog, dir );
    return dir;
  }

  inline void CtrlFinishTachy() {
    const TachyReport r = gJob->FinishTachy( true );
    gJobDir = MakeJobDir();
    MoveReadingLog( gReadingLog, gJobDir );             // log punktow zapisywany na biezaco -> odczyty.csv w folderze roboty
    const bool ok = ExportTachy( r, gJobDir, false );
    std::vector<std::string> lines = TachySummaryLines( r );
    lines.push_back( "" );
    for ( const TachyPoint& p : r.points ) {
      char b[220];
      sprintf( b, "%s  Hz %.4f g  V %.4f g  D %.3f m  N %.3f  E %.3f  H %.3f", p.name.c_str(), p.hzGon, p.vGon, p.slopeM, p.n, p.e, p.h );
      lines.push_back( b );
      if ( r.showTrueError && p.hasTrue ) {
        sprintf( b, "    blad: N %.3f  E %.3f  H %.3f  2D %.3f m", p.dNm, p.dEm, p.dHm, p.d2Dm );
        lines.push_back( b );
      }
    }
    TextPanelShow( "Tachimetria - podsumowanie", lines, ok ? "Zapisano w folderze: " + gJobDir : "BLAD ZAPISU plikow dziennika" );
    SvTrace( "Tachimetria zakonczona" );
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
    CtrlEndJob();
  }

  inline void CtrlFinish() {
    if ( !gJob ) return;
    if ( gJob->Kind() == JobKind::Gnss ) { CtrlFinishGnss(); return; }
    if ( gJob->Kind() == JobKind::Tachy ) { CtrlFinishTachy(); return; }
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
      if ( JobTypes()[idx].gnss || JobTypes()[idx].tachy ) CtrlOpenTrueErrorMenu( idx ); else CtrlOpenKindMenu( idx );
    } );
  }

  inline void CtrlConfirmAbort() {                     // domyślnie „Nie”, żeby jeden Enter nie kasował roboty
    MenuOpen( MenuView{ "Przerwac robote?", { { "no", "Nie, wracam do roboty", "", true },
                                              { "yes", "Tak, przerwij (zapisze to, co zmierzono)", "", true } } },
              []( const std::string& id ) {
      if ( id != "yes" || !gJob ) return;
      const std::string dir = gJob->Kind() == JobKind::Gnss ? CtrlSavePartialGnss( gJob->FinishGnss( false ) )
                            : gJob->Kind() == JobKind::Tachy ? CtrlSavePartialTachy( gJob->FinishTachy( false ) ) : CtrlSavePartial( gJob->Finish( false ) );
      HudMessage( dir.empty() ? "Robota przerwana" : ( "Robota przerwana, zapisano w folderze: " + dir ).c_str() );
      CtrlEndJob();
    } );
  }

  // Lista wyboru punktu osnowy (stanowisko / nawiazanie): wybor wywoluje OnMenu("<pozycja>:<punkt>") w robocie.
  inline void CtrlOpenChoice( const std::string& menuId ) {
    std::vector<MenuItem> items = gJob->TachyChoices( menuId );
    if ( items.empty() ) {
      HudMessage( menuId == "station" ? "Brak punktu osnowy w promieniu 2 m od ciebie" : "Brak wolnych punktow osnowy 3-300 m od stanowiska" );
      return;
    }
    MenuOpen( MenuView{ menuId == "station" ? "Wybierz punkt osnowy na stanowisko" : "Wybierz punkt osnowy na nawiazanie", items },
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
      char b[220]; sprintf( b, "%s  %.0f m %s  N %.3f  E %.3f  H %.3f  %s", c.id.c_str(), sqrt( dx * dx + dz * dz ) / 100.0, ControlCompass( dx, dz ), c.n, c.e, c.h, c.solution.c_str() );
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
      char b[200]; sprintf( b, "%s  %s  %.0f m %s", c.id.c_str(), ControlPlaceName( c ).c_str(), sqrt( dx * dx + dz * dz ) / 100.0, ControlCompass( dx, dz ) );
      v.items.push_back( { "tp:" + c.id, b, "", true } );
    }
    if ( page + 1 < pages ) v.items.push_back( { "tp_next", "Nastepna strona", "", true } );
    if ( page > 0 ) v.items.push_back( { "tp_prev", "Poprzednia strona", "", true } );
    MenuOpen( v, [page]( const std::string& id ) {
      if ( id == "tp_next" ) { CtrlOpenTeleport( page + 1 ); return; }
      if ( id == "tp_prev" ) { CtrlOpenTeleport( page - 1 ); return; }
      for ( const ControlPoint& c : gControl )
        if ( "tp:" + c.id == id && ControlWorldKey( c.world ) == ControlWorldKey( ControlWorldName() ) ) {
          if ( player ) player->SetPositionWorld( zVEC3( (float)c.x, (float)c.y + 100.0f, (float)c.z ) );   // 1 m nad znakiem, bohater opada na teren
          HudMessage( ( "Teleport: punkt " + c.id ).c_str() );
          return;
        }
    } );
  }

  inline void CtrlOpenMenu() {
    if ( !gJob ) {
      MenuOpen( MenuView{ "Menu geodezyjne", { { "start", "Rozpocznij robote", "", true }, { "world", "Zaladuj osnowe geodezyjna swiata", "", true }, { "control", "Pokaz osnowe", "", true }, { "map", "Mapa osnowy", "", true }, { "tp", "Teleport do punktu osnowy", "", true } } },
                []( const std::string& id ) { if ( id == "control" ) CtrlShowControl(); else if ( id == "world" ) CtrlLoadWorldControl(); else if ( id == "map" ) MapOpen(); else if ( id == "tp" ) CtrlOpenTeleport( 0 ); else CtrlOpenStartMenu(); } );
      return;
    }
    MenuView v{ gJob->Name(), gJob->Menu() };
    if ( gJob->Kind() == JobKind::Leveling ) v.items.push_back( { "scope", "Patrz przez lunete (" LV_KEYNAME_SCOPE ")", "najpierw niwelator i lata", gWorld.HasInstrument() && !gJob->ActiveRodPoints().empty() } );
    v.items.push_back( { "map", "Mapa osnowy", "", true } );
    v.items.push_back( { "tp", "Teleport do punktu osnowy", "", true } );
    v.items.push_back( { "abort", "Przerwij robote (bez dziennika)", "", true } );
    MenuOpen( v, []( const std::string& id ) {
      if ( !gJob ) return;
      if ( id == "finish" ) CtrlFinish();
      else if ( id == "scope" ) CtrlEnterScope();
      else if ( id == "fine" ) PoleFineEnter();
      else if ( id == "view" ) { gJob->OnMenu( id ); CtrlEnterTachyView(); }
      else if ( gJob->NeedsChoice( id ) ) CtrlOpenChoice( id );
      else if ( id == "map" ) MapOpen();
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

  inline void ControllerTick() {
    zCWorld* w = ogame->GetWorld();
    if ( w != gCtrlWorld || ( w && gWorld.Stale() ) ) {   // inny świat albo ten sam obiekt świata po wczytaniu zapisu (voby zniknęły)                           // zmiana świata (wczytanie zapisu): zapisz, co się zmierzyło, i posprzątaj
      if ( gJob ) {
        if ( gJob->Kind() == JobKind::Gnss ) CtrlSavePartialGnss( gJob->FinishGnss( false ) );
        else if ( gJob->Kind() == JobKind::Tachy ) CtrlSavePartialTachy( gJob->FinishTachy( false ) );
        else CtrlSavePartial( gJob->Finish( false ) );
      }
      if ( FieldBookEditActive() ) { gPendingFb.complete = false; ExportFieldBook( gPendingFb, gJobDir, true ); }
      FieldBookEditCancel(); TextPanelClose(); FieldBookClose(); MapClose(); CtrlForgetJob(); gCtrlWorld = w;
    }
    if ( !w ) return;
    ControlTick();                                     // znaki osnowy: nowy swiat / wczytany zapis
    static DWORD lastClock = GetTickCount();
    const DWORD nowClock = GetTickCount();
    double dtClock = ( nowClock - lastClock ) / 1000.0; lastClock = nowClock;
    if ( dtClock > 0.2 ) dtClock = 0.2;
    const bool gamePaused = MenuIsOpen() || ScopeActive() || FieldBookVisible() || TextPanelVisible() || FieldBookEditActive() || PoleFineActive() || TachyViewActive() || MapActive();
    if ( !gamePaused ) { gWorld.AdvanceClock( dtClock ); if ( gJob ) gJob->Tick(); }   // zegar symulacji stoi, gdy gra zatrzymana
    if ( !MenuIsOpen() && !ScopeActive() && !PoleFineActive() && !TachyViewActive() ) gWorld.DressTick();   // przy zatrzymanej grze nic się nie rusza
    ScopeTick();
    PoleFineTick();
    TachyViewTick();
    HudTick();
    if ( ScopeActive() || PoleFineActive() || TachyViewActive() ) { CtrlCloseJobHud(); return; }
    if ( FieldBookEditActive() ) { if ( FieldBookEditTick() ) CtrlFinishManual(); return; }
    if ( MapActive() ) { MapTick(); return; }
    if ( TextPanelVisible() ) { TextPanelTick(); return; }
    if ( FieldBookVisible() ) { FieldBookTick(); return; }
    if ( MenuIsOpen() ) { MenuTick(); return; }
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { CtrlOpenMenu(); return; }
    if ( gJob && gJob->Kind() == JobKind::Leveling && zinput->KeyToggled( LV_KEY_SCOPE ) ) CtrlEnterScope();
    if ( gJob && gJob->Kind() == JobKind::Tachy && zinput->KeyToggled( LV_KEY_SCOPE ) ) {
      bool canView = false; for ( const MenuItem& m : gJob->Menu() ) if ( m.id == "view" && m.enabled ) canView = true;
      if ( canView ) { gJob->OnMenu( "view" ); CtrlEnterTachyView(); } else HudMessage( "Najpierw orientacja i tyczka z lustrem" );
    }
    CtrlUpdateJobHud();
    CtrlUpdateStatusPanel();
  }
}
