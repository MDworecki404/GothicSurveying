// Kontroler: menu startowe (typ roboty -> rodzaj -> tryb liczenia), menu aktywnej roboty, luneta, HUD kroku. Bez strażnika include.
#include <memory>

namespace GOTHIC_NAMESPACE {
  inline EngineWorld        gWorld;
  inline std::unique_ptr<Job> gJob;
  inline zCWorld*           gCtrlWorld = nullptr;
  inline zCView*            gJobHud = nullptr;       // stały panel z bieżącym krokiem roboty
  inline std::string        gJobHudText;

  inline void CtrlCloseJobHud() { PanelClose( gJobHud ); gJobHudText.clear(); }
  inline void CtrlEndJob() { gJob.reset(); gWorld.ClearAll(); CtrlCloseJobHud(); }
  inline void CtrlForgetJob() {                        // po zmianie świata: obiekty zniknęły razem ze światem
    ScopeLeave(); MenuClose(); gJob.reset(); gWorld.ForgetAll(); CtrlCloseJobHud();
  }

  inline void CtrlEnterScope() {
    if ( !gJob || !gWorld.HasInstrument() || gJob->ActiveRodPoints().empty() ) { HudMessage( "Najpierw postaw niwelator i late (F12)" ); return; }
    ScopeCallbacks cb;
    cb.targets = []() { std::vector<RodTarget> t; if ( gJob ) for ( int pid : gJob->ActiveRodPoints() ) t.push_back( { pid, gWorld.PointFoot( pid ) } ); return t; };
    cb.onReading = []( int p, int m, int u, int l ) { return gJob && gJob->OnReading( p, m, u, l ); };
    cb.onEnter = []() { gWorld.TurnRodsToInstrument(); gWorld.SetInstrumentVisible( false ); };
    cb.onLeave = []() { gWorld.SetInstrumentVisible( true ); };
    ScopeEnter( gWorld.InstrumentGround(), cb );
  }

  inline void CtrlFinish() {
    if ( !gJob ) return;
    FieldBook fb = gJob->Finish( true );
    char b[200];
    if ( fb.sum.hasClosure )
      sprintf( b, "Dziennik gotowy: %d stan., fh = %.1f mm (dop. %.1f mm) - %s", (int)fb.rows.size(), fb.sum.fhMm, fb.sum.fhDopMm,
               fb.sum.withinTolerance ? "w tolerancji" : "PRZEKROCZONY" );
    else sprintf( b, "Dziennik gotowy: %d stan., ciag otwarty (brak kontroli)", (int)fb.rows.size() );
    HudMessage( b );
    CtrlEndJob();
  }

  inline void CtrlStartJob( int typeIdx, TraverseKind kind, CalcMode mode ) {
    gJob = JobTypes()[typeIdx].create( gWorld, JobOptions{ kind, mode } );
    HudMessage( ( "Robota: " + gJob->Name() + ". F12 - menu" ).c_str() );
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

  inline void CtrlOpenMenu() {
    if ( !gJob ) {
      MenuOpen( MenuView{ "Menu geodezyjne", { { "start", "Rozpocznij robote", "", true } } }, []( const std::string& ) { CtrlOpenStartMenu(); } );
      return;
    }
    MenuView v{ gJob->Name(), gJob->Menu() };
    v.items.push_back( { "scope", "Patrz przez lunete (F10)", "najpierw niwelator i lata", gWorld.HasInstrument() && !gJob->ActiveRodPoints().empty() } );
    v.items.push_back( { "abort", "Przerwij robote (bez dziennika)", "", true } );
    MenuOpen( v, []( const std::string& id ) {
      if ( !gJob ) return;
      if ( id == "finish" ) CtrlFinish();
      else if ( id == "scope" ) CtrlEnterScope();
      else if ( id == "abort" ) { HudMessage( "Robota przerwana" ); CtrlEndJob(); }
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
    if ( w != gCtrlWorld ) { CtrlForgetJob(); gCtrlWorld = w; }
    if ( !w ) return;
    ScopeTick();
    HudTick();
    if ( ScopeActive() ) { CtrlCloseJobHud(); return; }
    if ( MenuIsOpen() ) { MenuTick(); return; }
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { CtrlOpenMenu(); return; }
    if ( gJob && zinput->KeyToggled( KEY_F10 ) ) CtrlEnterScope();
    CtrlUpdateJobHud();
  }
}
