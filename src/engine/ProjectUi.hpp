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

  inline void ProjUiOpenMeasurement( const std::string& mid );

  // Usuniecie pomiaru z wynikami: potwierdzenie (domyslnie "Nie"); folder wynikow i plik punktow znikaja, osnowa zostaje.
  inline void ProjUiConfirmDelete( const std::string& mid ) {
    const Measurement* mp = nullptr; for ( const Measurement& m : gProject.items ) if ( m.id == mid ) mp = &m;
    if ( !mp ) { ProjUiOpenHome(); return; }
    const std::string name = mp->name, hint = mp->dir.empty() ? "pomiar nie ma folderu wynikow; zniknie tylko z listy"
                                                              : "folder " + mp->dir + " (" + std::to_string( ResultFiles( mp->dir ).size() ) + " plikow); punktow osnowy zalozonych przez pomiar nie usuwa";
    MenuOpen( MenuView{ "Usunac pomiar " + name + "?", { { "no", "Nie, zostaw", "", true }, { "yes", "Tak, usun pomiar i jego wyniki (nieodwracalne)", hint, true } } },
              [mid, name]( const std::string& id ) {
      if ( id != "yes" ) { ProjUiOpenMeasurement( mid ); return; }
      std::string why;
      if ( RemoveMeasurementWithFiles( gProject, mid, ProjRoot(), why ) ) { ProjSave(); HudMessage( ( "Usunieto pomiar: " + name ).c_str() ); SvTrace( ( "pomiar usuniety: " + name ).c_str() ); }
      else HudMessage( ( "Nie usunieto: " + why ).c_str() );
      ProjUiOpenHome();
    }, [mid]() { ProjUiOpenMeasurement( mid ); } );
  }

  inline void ProjUiOpenFolder( const std::string& dir ) {
    if ( !SafeResultDir( dir ) ) { HudMessage( "Nie otwieram: folder poza GothicSurveying" ); return; }      // sciezka z manifestu jest nieufna
    std::error_code ec; const std::string abs = std::filesystem::absolute( dir, ec ).string();
    if ( !std::filesystem::is_directory( abs, ec ) ) { HudMessage( "Folder nie istnieje" ); return; }
    SvTrace( ( "otwieram folder: " + abs ).c_str() );
    ShellExecuteA( nullptr, "open", abs.c_str(), nullptr, nullptr, SW_SHOWNORMAL );
  }

  // Menu jednego pomiaru: szczegoly, pliki, folder, usuniecie. Lewo wraca na ekran glowny; panele wracaja tu po zamknieciu.
  inline void ProjUiOpenMeasurement( const std::string& mid ) {
    const Measurement* mp = nullptr; for ( const Measurement& m : gProject.items ) if ( m.id == mid ) mp = &m;
    if ( !mp ) { ProjUiOpenHome(); return; }
    const Measurement m = *mp;
    MenuView v; v.title = m.name; v.crumbs = "GEODEZJA > Projekt: " + gProject.name + " > " + m.name;
    { MenuItem it{ "info", "Szczegoly pomiaru", "stan, rodzaj, folder i podsumowanie", true }; it.section = "Pomiar"; v.items.push_back( it ); }
    { MenuItem it{ "files", "Pliki wynikowe", m.dir, !m.dir.empty() }; it.section = "Pomiar"; it.why = "pomiar nie ma folderu wynikow"; v.items.push_back( it ); }
    { MenuItem it{ "open", "Otworz folder wynikow", m.dir, !m.dir.empty() }; it.section = "Pomiar"; it.why = "pomiar nie ma folderu wynikow"; v.items.push_back( it ); }
    const bool canDel = !gProject.closed && m.state != MeasState::Running;
    { MenuItem it{ "delete", "Usun pomiar i jego wyniki", "kasuje folder wynikow i wpis w projekcie (z potwierdzeniem)", canDel }; it.section = "Usuwanie";
      it.why = gProject.closed ? "projekt zamkniety (otworz go ponownie)" : "ten pomiar trwa"; v.items.push_back( it ); }
    MenuOpen( v, [m, mid]( const std::string& id ) {
      if ( id == "info" ) { gMenuAfterPanel = [mid]() { ProjUiOpenMeasurement( mid ); }; ProjUiShowMeasurement( m ); }
      else if ( id == "files" ) {
        std::vector<std::string> files = ResultFiles( m.dir ); if ( files.empty() ) files.push_back( "(pusty folder)" );
        gMenuAfterPanel = [mid]() { ProjUiOpenMeasurement( mid ); }; TextPanelShow( m.name + " - pliki wynikowe", files, "Folder: " + m.dir );
      }
      else if ( id == "open" ) { ProjUiOpenFolder( m.dir ); ProjUiOpenMeasurement( mid ); }
      else if ( id == "delete" ) ProjUiConfirmDelete( mid );
    }, []() { ProjUiOpenHome(); } );
  }

  // Wyniki: pliki w folderach pomiarow i otwarcie folderu w Eksploratorze. Lewo wraca na ekran glowny.
  inline void ProjUiOpenResults() {
    MenuView v; v.title = "Wyniki i raporty"; v.crumbs = "GEODEZJA > Projekt: " + gProject.name + " > Wyniki";
    for ( const Measurement& m : gProject.items ) {
      if ( m.dir.empty() ) continue;
      const size_t n = ResultFiles( m.dir ).size();
      MenuItem a{ "r:" + m.id, m.name + "  (" + std::to_string( n ) + " plikow)", "Enter - lista plikow; folder: " + m.dir, true }; a.section = "Pliki"; v.items.push_back( a );
      MenuItem b{ "o:" + m.id, "Otworz folder: " + m.name, m.dir, true }; b.section = "Otworz folder"; v.items.push_back( b );
    }
    const std::string rep = ProjRoot() + "/projekty/" + gProject.name;
    if ( gProject.closed ) { MenuItem it{ "o:project", "Otworz folder projektu (raport zbiorczy)", rep, true }; it.section = "Otworz folder"; v.items.push_back( it ); }
    if ( v.items.empty() ) { HudMessage( "Brak wynikow: zaden pomiar nie zapisal jeszcze folderu" ); return; }
    MenuOpen( v, [rep]( const std::string& id ) {
      if ( id == "o:project" ) { ProjUiOpenFolder( rep ); ProjUiOpenResults(); return; }
      for ( const Measurement& m : gProject.items ) {
        if ( "o:" + m.id == id ) { ProjUiOpenFolder( m.dir ); ProjUiOpenResults(); return; }
        if ( "r:" + m.id == id ) {
          std::vector<std::string> files = ResultFiles( m.dir ); if ( files.empty() ) files.push_back( "(pusty folder)" );
          gMenuAfterPanel = []() { ProjUiOpenResults(); }; TextPanelShow( m.name + " - pliki wynikowe", files, "Folder: " + m.dir ); return;
        }
      }
    }, []() { ProjUiOpenHome(); } );
  }

  // Zamkniecie projektu: potwierdzenie (domyslnie "Nie"), potem raport zbiorczy CSV + DOCX z porownaniem wysokosci.
  inline void ProjUiConfirmClose() {
    MenuOpen( MenuView{ "Zamknac projekt " + gProject.name + "?", { { "no", "Nie, wracam do projektu", "", true },
                                                                  { "yes", "Tak, zamknij i utworz raport zbiorczy", "projekt stanie sie tylko do odczytu (mozna otworzyc ponownie)", true } } },
              []( const std::string& id ) {
      if ( id != "yes" ) { ProjUiOpenHome(); return; }
      std::string why;
      if ( !CloseProject( gProject, why ) ) { HudMessage( ( "Nie mozna zamknac: " + why ).c_str() ); ProjUiOpenHome(); return; }
      ProjSave();
      std::string err; const bool ok = WriteProjectReport( gProject, ProjRoot(), err );
      std::vector<std::string> lines = { "Pomiarow w projekcie: " + std::to_string( gProject.items.size() ) };
      lines.push_back( ok ? "Raport zbiorczy: projekt_raport.docx i projekt_raport.csv" : "BLAD raportu: " + err );
      lines.push_back( "Folder: " + ProjRoot() + "/projekty/" + gProject.name );
      TextPanelShow( "Projekt zamkniety: " + gProject.name, lines, "Otwarcie ponowne: ekran glowny projektu" );
      SvTrace( ( "projekt zamkniety: " + gProject.name + ( ok ? ", raport zapisany" : ", BLAD raportu " + err ) ).c_str() );
    }, []() { ProjUiOpenHome(); } );
  }

  inline void ProjUiInstallMenuHooks() {                          // F1 i pamiec kursora: menu (Menu.hpp) nie zna TextPanelShow ani ustawien
    gMenuHelpFn = []( const std::string& ctx ) { TextPanelShow( "Pomoc", HelpLines( ctx ), "Zamknij: Enter" ); };
    gMenuRemember = []( const std::string& key, int idx ) { UiEnsure(); if ( gUi.cursor[key] != idx ) { gUi.cursor[key] = idx; UiSave(); } };
    gMenuRecall = []( const std::string& key ) { UiEnsure(); auto it = gUi.cursor.find( key ); return it == gUi.cursor.end() ? -1 : it->second; };
  }

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
    ProjUiInstallMenuHooks();
    ProjEnsure();
    UiRememberProject( gProject.name );
    ControlEnsureLoaded();
    const std::string wn = ControlWorldName();
    const int ctrl = (int)ControlNear( gControl, wn, 0.0, 0.0, 0.0, 1e12 ).size();
    MenuView v; v.title = "GEODEZJA"; v.screenKey = "home"; v.helpContext = "home"; v.digits = true;
    v.crumbs = "Projekt: " + gProject.name + ( gProject.closed ? " (zamkniety)" : "" ) + "   swiat: " + wn + "   osnowa: " + std::to_string( ctrl ) + " pkt   pomiary: " + std::to_string( gProject.items.size() );
    for ( const Measurement& m : gProject.items ) {
      std::string label = std::string( ProjMark( m.state ) ) + " " + m.name;
      if ( !m.summary.empty() ) label += "   " + m.summary.substr( 0, m.summary.find( '\n' ) ).substr( 0, 36 );
      MenuItem it{ "m:" + m.id, label, std::string( ProjStateText( m.state ) ) + ( m.dir.empty() ? "" : ", folder " + m.dir ) + "; Enter - szczegoly, pliki, usuwanie", true }; it.section = "Pomiary";
      v.items.push_back( it );
    }
    std::string why; const bool canAdd = CanAddMeasurement( gProject, why );
    { MenuItem it{ "new", "+ Nowy pomiar...", "kreator albo reczny wybor rodzaju", canAdd }; it.section = "Pomiary"; it.why = why; v.items.push_back( it ); }
    if ( gProject.closed ) { MenuItem it{ "reopen", "Otworz projekt ponownie", "pozwoli dodawac pomiary", true }; it.section = "Projekt"; v.items.push_back( it ); }
    { bool anyDir = false; for ( const Measurement& m : gProject.items ) if ( !m.dir.empty() ) anyDir = true;
      MenuItem it{ "results", "Wyniki i raporty (pliki pomiarow)", "foldery wynikow pomiarow i otwarcie folderu", anyDir }; it.section = "Projekt"; it.why = "zaden pomiar nie zapisal jeszcze wynikow"; v.items.push_back( it ); }
    if ( !gProject.closed ) { MenuItem it{ "close", "Zamknij projekt (raport zbiorczy)", "raport CSV + DOCX z porownaniem wysokosci; projekt tylko do odczytu", !gProject.items.empty() }; it.section = "Projekt"; it.why = "najpierw wykonaj pomiar"; v.items.push_back( it ); }
    { MenuItem it{ "projects", "Projekty: zmien / nowy", "projekty tego swiata", true }; it.section = "Projekt"; v.items.push_back( it ); }
    { MenuItem it{ "tools", "Osnowa, mapa, teleport, poziomowanie", "narzedzia niezalezne od projektu", true }; it.section = "Narzedzia"; v.items.push_back( it ); }
    const Next nx = ProjectNext( gProject, Next{} );
    for ( const MenuItem& it : v.items ) if ( it.id == nx.id ) { v.nextId = nx.id; v.nextText = nx.text; }
    if ( v.nextText.empty() ) v.nextText = nx.text;                                    // np. "create": bez pozycji, sam tekst
    MenuOpen( v, []( const std::string& id ) {
      if ( id == "new" ) ProjUiOpenNew();
      else if ( id == "reopen" ) { ReopenProject( gProject ); ProjSave(); HudMessage( "Projekt otwarty ponownie" ); ProjUiOpenHome(); }
      else if ( id == "results" ) ProjUiOpenResults();
      else if ( id == "close" ) ProjUiConfirmClose();
      else if ( id == "projects" ) ProjUiOpenList();
      else if ( id == "tools" ) CtrlOpenToolsMenu();
      else if ( id.compare( 0, 2, "m:" ) == 0 ) ProjUiOpenMeasurement( id.substr( 2 ) );
    } );
  }

  // Usuniecie biezacego projektu z wynikami wszystkich jego pomiarow: potwierdzenie (domyslnie "Nie"). Osnowa zostaje.
  inline void ProjUiConfirmDeleteProject() {
    std::string why; const Measurement* running = RunningMeasurement( gProject );
    if ( running ) { HudMessage( ( "Nie mozna usunac: trwa pomiar " + running->name ).c_str() ); ProjUiOpenList(); return; }
    const std::string name = gProject.name;
    MenuOpen( MenuView{ "Usunac projekt " + name + "?", { { "no", "Nie, zostaw", "", true },
        { "yes", "Tak, usun projekt i wyniki jego pomiarow (nieodwracalne)", "pomiarow: " + std::to_string( gProject.items.size() ) + "; foldery wynikow zostana skasowane; punktow osnowy nie usuwa", true } } },
      [name]( const std::string& id ) {
        if ( id != "yes" ) { ProjUiOpenList(); return; }
        std::string why2;
        if ( DeleteProjectWithFiles( gProject, ProjRoot(), why2 ) ) {
          SvTrace( ( "projekt usuniety: " + name ).c_str() );
          gProject = Project(); ProjEnsure(); UiRememberProject( gProject.name );     // zostaje inny projekt tego swiata albo nowy domyslny
          HudMessage( ( "Usunieto projekt: " + name + ". Teraz: " + gProject.name ).c_str() );
          ProjUiOpenHome();
        } else { HudMessage( ( "Nie usunieto: " + why2 ).c_str() ); ProjUiOpenList(); }
      }, []() { ProjUiOpenList(); } );
  }

  inline void ProjUiOpenList() {
    MenuView v; v.title = "Projekty swiata " + ControlWorldName();
    const std::vector<Project> all = ProjListForWorld();
    for ( const Project& p : all ) {
      MenuItem it{ "p:" + p.name, std::string( p.name == gProject.name ? "* " : "  " ) + p.name + "  (" + std::to_string( p.items.size() ) + " pom." + ( p.closed ? ", zamkniety" : "" ) + ")", "Enter - przelacz na ten projekt", true };
      v.items.push_back( it );
    }
    v.items.push_back( { "newproj", "+ Nowy projekt", "wpiszesz nazwe (domyslnie: " + ProjAutoName() + ")", true } );
    { MenuItem it{ "rename", "Zmien nazwe biezacego projektu", "zmienia tez folder projektu; zapisy gry z trwajaca robota wskaza wtedy projekt Luzem", !gProject.closed }; it.why = "projekt zamkniety (otworz go ponownie)"; v.items.push_back( it ); }
    { MenuItem it{ "deleteproj", "Usun biezacy projekt i jego wyniki", "kasuje projekt i foldery wynikow jego pomiarow (z potwierdzeniem)", !RunningMeasurement( gProject ) }; it.why = "trwa pomiar w tym projekcie"; v.items.push_back( it ); }
    MenuOpen( v, []( const std::string& id ) {
      if ( id == "deleteproj" ) { ProjUiConfirmDeleteProject(); return; }
      if ( id == "newproj" ) {
        TextInputStart( "Nazwa nowego projektu", ProjAutoName(), 32, []( const std::string& t ) { return ProjectNameProblem( t, ProjRoot() ); },
          []( const std::string& t ) { gProject = ProjMake( t ); ProjSave(); UiRememberProject( gProject.name ); HudMessage( ( "Nowy projekt: " + gProject.name ).c_str() ); ProjUiOpenHome(); },
          []() { ProjUiOpenList(); } );
        return;
      } else if ( id == "rename" ) {
        TextInputStart( "Nowa nazwa projektu " + gProject.name, gProject.name, 32, []( const std::string& t ) { return ProjectNameProblem( t, ProjRoot() ); },
          []( const std::string& t ) {
            std::string why;
            if ( RenameProject( gProject, t, ProjRoot(), why ) ) { UiRememberProject( gProject.name ); HudMessage( ( "Nazwa projektu: " + gProject.name ).c_str() ); }
            else HudMessage( ( "Nie zmieniono nazwy: " + why ).c_str() );
            ProjUiOpenHome(); },
          []() { ProjUiOpenList(); } );
        return;
      } else if ( id.compare( 0, 2, "p:" ) == 0 ) {
        Project p; if ( LoadProject( ProjRoot(), id.substr( 2 ), p ) ) { gProject = p; ProjReconcile(); UiRememberProject( gProject.name ); HudMessage( ( "Projekt: " + gProject.name ).c_str() ); }
      }
      ProjUiOpenHome();
    }, []() { ProjUiOpenHome(); } );
  }
}
