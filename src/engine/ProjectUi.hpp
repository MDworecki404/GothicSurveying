// Ekran glowny projektu (Home bez aktywnej roboty): okruszki, rekomendowany krok, lista pomiarow, projekty i narzedzia. Bez strażnika include.
// Dolaczane po Controller.hpp (uzywa CtrlOpenStartMenu, CtrlOpenToolsMenu); Controller.hpp ma tylko deklaracje ProjUiOpenHome.
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  inline const char* ProjMark( MeasState s ) {
    switch ( s ) { case MeasState::Done: return "[x]"; case MeasState::Running: return "[~]"; case MeasState::Aborted: return "[!]"; default: return "[ ]"; }
  }
  inline const char* ProjStateText( MeasState s ) {
    switch ( s ) { case MeasState::Done: return "gotowy"; case MeasState::Running: return "w toku"; case MeasState::Aborted: return "przerwany"; default: return "nowy"; }
  }

  inline void ProjUiShowMeasurement( const Measurement& m ) {
    std::vector<std::string> lines;
    lines.push_back( std::string( "Stan: " ) + ProjStateText( m.state ) + ( m.guided ? ", tryb prowadzony" : "" ) );
    lines.push_back( "Rodzaj: " + m.type );
    if ( !m.dir.empty() ) lines.push_back( "Folder wynikow: " + m.dir );
    if ( !m.summary.empty() ) lines.push_back( "Podsumowanie: " + m.summary );
    TextPanelShow( m.name, lines, "Projekt: " + gProject.name );
  }

  inline void ProjUiOpenList();
  inline void ProjUiOpenHome();

  // Kreator: pomiar z gotowymi ustawieniami w trybie prowadzonym (kolejne kroki podpowiada linia DALEJ).
  inline void ProjUiStartWizard( const Wizard& wz ) {
    int idx = -1; for ( size_t i = 0; i < JobTypes().size(); i++ ) if ( JobTypes()[i].id == wz.typeId ) idx = (int)i;
    if ( idx < 0 ) return;
    gStartGuided = true;
    CtrlStartJob( idx, wz.opts.kind, wz.opts.mode, wz.opts.showTrueError );
    if ( gJob ) ApplyWizardSetup( *gJob, wz );
    gStartGuided = false;
  }

  inline void ProjUiOpenNew() {
    MenuView v; v.title = "Nowy pomiar"; v.crumbs = "GEODEZJA > Projekt: " + gProject.name + " > Nowy pomiar";
    for ( const Wizard& wz : Wizards() ) { MenuItem it{ "w:" + wz.id, wz.name, wz.desc, true }; it.section = "Kreatory"; v.items.push_back( it ); }
    { MenuItem it{ "manual", "Reczny wybor rodzaju pomiaru...", "pelne menu bez kreatora: wszystkie rodzaje i ustawienia", true }; it.section = "Inne"; v.items.push_back( it ); }
    MenuOpen( v, []( const std::string& id ) {
      if ( id == "manual" ) { CtrlOpenStartMenu(); return; }
      for ( const Wizard& wz : Wizards() ) if ( "w:" + wz.id == id ) { ProjUiStartWizard( wz ); return; }
    }, []() { ProjUiOpenHome(); } );
  }

  inline void ProjUiOpenHome() {
    ProjEnsure();
    ControlEnsureLoaded();
    const std::string wn = ControlWorldName();
    const int ctrl = (int)ControlNear( gControl, wn, 0.0, 0.0, 0.0, 1e12 ).size();
    MenuView v; v.title = "GEODEZJA";
    v.crumbs = "Projekt: " + gProject.name + ( gProject.closed ? " (zamkniety)" : "" ) + "   swiat: " + wn + "   osnowa: " + std::to_string( ctrl ) + " pkt   pomiary: " + std::to_string( gProject.items.size() );
    for ( const Measurement& m : gProject.items ) {
      std::string label = std::string( ProjMark( m.state ) ) + " " + m.name;
      if ( !m.summary.empty() ) label += "   " + m.summary.substr( 0, m.summary.find( '\n' ) ).substr( 0, 36 );
      MenuItem it{ "m:" + m.id, label, std::string( ProjStateText( m.state ) ) + ( m.dir.empty() ? "" : ", folder " + m.dir ), true }; it.section = "Pomiary";
      v.items.push_back( it );
    }
    std::string why; const bool canAdd = CanAddMeasurement( gProject, why );
    { MenuItem it{ "new", "+ Nowy pomiar...", "kreator albo reczny wybor rodzaju", canAdd }; it.section = "Pomiary"; it.why = why; v.items.push_back( it ); }
    if ( gProject.closed ) { MenuItem it{ "reopen", "Otworz projekt ponownie", "pozwoli dodawac pomiary", true }; it.section = "Projekt"; v.items.push_back( it ); }
    { MenuItem it{ "projects", "Projekty: zmien / nowy", "projekty tego swiata", true }; it.section = "Projekt"; v.items.push_back( it ); }
    { MenuItem it{ "tools", "Osnowa, mapa, teleport, poziomowanie", "narzedzia niezalezne od projektu", true }; it.section = "Narzedzia"; v.items.push_back( it ); }
    const Next nx = ProjectNext( gProject, Next{} );
    for ( const MenuItem& it : v.items ) if ( it.id == nx.id ) { v.nextId = nx.id; v.nextText = nx.text; }
    if ( v.nextText.empty() ) v.nextText = nx.text;                                    // np. "create": bez pozycji, sam tekst
    MenuOpen( v, []( const std::string& id ) {
      if ( id == "new" ) ProjUiOpenNew();
      else if ( id == "reopen" ) { ReopenProject( gProject ); ProjSave(); HudMessage( "Projekt otwarty ponownie" ); ProjUiOpenHome(); }
      else if ( id == "projects" ) ProjUiOpenList();
      else if ( id == "tools" ) CtrlOpenToolsMenu();
      else if ( id.compare( 0, 2, "m:" ) == 0 ) for ( const Measurement& m : gProject.items ) if ( "m:" + m.id == id ) { ProjUiShowMeasurement( m ); break; }
    } );
  }

  inline void ProjUiOpenList() {
    MenuView v; v.title = "Projekty swiata " + ControlWorldName();
    const std::vector<Project> all = ProjListForWorld();
    for ( const Project& p : all ) {
      MenuItem it{ "p:" + p.name, std::string( p.name == gProject.name ? "* " : "  " ) + p.name + "  (" + std::to_string( p.items.size() ) + " pom." + ( p.closed ? ", zamkniety" : "" ) + ")", "Enter - przelacz na ten projekt", true };
      v.items.push_back( it );
    }
    v.items.push_back( { "newproj", "+ Nowy projekt", "nazwa nadawana automatycznie: " + ProjAutoName(), true } );
    MenuOpen( v, []( const std::string& id ) {
      if ( id == "newproj" ) {
        gProject = ProjMake( ProjAutoName() ); ProjSave();
        HudMessage( ( "Nowy projekt: " + gProject.name ).c_str() );
      } else if ( id.compare( 0, 2, "p:" ) == 0 ) {
        Project p; if ( LoadProject( ProjRoot(), id.substr( 2 ), p ) ) { gProject = p; HudMessage( ( "Projekt: " + gProject.name ).c_str() ); }
      }
      ProjUiOpenHome();
    }, []() { ProjUiOpenHome(); } );
  }
}
