// Aktualny projekt (stan silnika): ladowanie/zapis manifestu, wybor projektu dla biezacego swiata. Bez strażnika include.
// Wymaga calc/Project.hpp, engine/ControlNet.hpp (ControlWorldName) i engine/Export.hpp (LV_OUT_DIR, NowStamp); dolaczane przed Controller.hpp.
#include <filesystem>
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  inline Project gProject;                                      // pusty (name == "") = jeszcze nie wczytany
  inline UiSettings gUi; inline bool gUiLoaded = false;         // GothicSurveying/ustawienia.txt: ostatni projekt, kursory

  inline std::string UiPath() { return std::string( LV_OUT_DIR ) + "/ustawienia.txt"; }
  inline void UiEnsure() {
    if ( gUiLoaded ) return;
    gUiLoaded = true;
    std::ifstream f( UiPath(), std::ios::binary );
    if ( f ) gUi = UiSettingsFromText( std::string( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() ) );
  }
  inline void UiSave() {
    std::error_code ec; std::filesystem::create_directories( LV_OUT_DIR, ec );
    const std::string t = UiSettingsToText( gUi );
    std::ofstream f( UiPath(), std::ios::binary | std::ios::trunc );
    if ( f ) f.write( t.data(), (std::streamsize)t.size() );
  }
  inline void UiRememberProject( const std::string& name ) { UiEnsure(); if ( gUi.lastProject != name ) { gUi.lastProject = name; UiSave(); } }

  inline std::string ProjRoot() { return LV_OUT_DIR; }
  inline std::string ProjWorldKey() { return ControlWorldKey( ControlWorldName() ); }

  inline std::string ProjSanitize( const std::string& s, size_t maxLen ) {   // tylko [A-Za-z0-9_-], reszta -> '_'
    std::string o;
    for ( unsigned char c : s ) { if ( o.size() >= maxLen ) break; o += ( c < 128 && ( std::isalnum( c ) || c == '-' || c == '_' ) ) ? (char)c : '_'; }
    return o;
  }

  inline bool ProjSave() {
    if ( gProject.name.empty() ) return false;
    const bool ok = SaveProject( gProject, ProjRoot() );
    if ( !ok ) SvTrace( ( "projekt: BLAD zapisu " + gProject.name ).c_str() );
    return ok;
  }

  inline Project ProjMake( const std::string& name ) {
    Project p; p.name = name; p.world = ProjWorldKey(); p.created = NowStamp();
    return p;
  }

  inline std::string ProjAutoName() {                           // <swiat>_<n>: bez wpisywania tekstu w grze
    const std::string base = ProjSanitize( ControlWorldName(), 24 );
    for ( int n = 1; n < 1000; n++ ) {
      const std::string name = base + "_" + std::to_string( n );
      if ( !std::filesystem::exists( ProjectFile( ProjRoot(), name ) ) ) return name;
    }
    return base + "_x";
  }

  inline std::vector<Project> ProjListForWorld() {              // projekty biezacego swiata (wczytane), od najstarszego
    std::vector<Project> out; const std::string key = ProjWorldKey();
    for ( const std::string& n : ListProjects( ProjRoot() ) ) { Project p; if ( LoadProject( ProjRoot(), n, p ) && p.world == key ) out.push_back( p ); }
    std::stable_sort( out.begin(), out.end(), []( const Project& a, const Project& b ) { return a.created < b.created; } );
    return out;
  }

  // Zapewnia aktualny projekt dla biezacego swiata: zostaje obecny, inaczej najnowszy projekt swiata, a gdy brak - domyslny "Luzem_<swiat>".
  inline void ProjEnsure() {
    const std::string key = ProjWorldKey();
    if ( !gProject.name.empty() && gProject.world == key ) return;
    const std::vector<Project> all = ProjListForWorld();
    if ( !all.empty() ) {
      UiEnsure(); gProject = all.back();                         // ostatnio uzywany projekt tego swiata, a bez niego najnowszy
      for ( const Project& p : all ) if ( p.name == gUi.lastProject ) gProject = p;
      SvTrace( ( "projekt: wczytany " + gProject.name ).c_str() ); return;
    }
    gProject = ProjMake( "Luzem_" + ProjSanitize( key, 24 ) );
    ProjSave(); UiRememberProject( gProject.name );
    SvTrace( ( "projekt: utworzony domyslny " + gProject.name ).c_str() );
  }
}
