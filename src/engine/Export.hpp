// Zapis dziennika do plików w katalogu roboczym gry (katalog główny gry): folder GothicSurveying/<data_godzina>/ z dziennik.docx, dziennik.csv i odczyty.csv (przerwany: dziennik_przerwany.csv).
// Wymaga calc/Docx.hpp dołączonego wcześniej. Bez strażnika include.
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

namespace GOTHIC_NAMESPACE {
  inline std::string DateTodayText() {                // rzeczywista data systemowa, RRRR-MM-DD
    time_t t = time( nullptr ); tm lt; localtime_s( &lt, &t );
    char b[16]; strftime( b, sizeof b, "%Y-%m-%d", &lt );
    return b;
  }

  inline bool FileExistsRel( const std::string& path ) { std::ifstream f( path, std::ios::binary ); return f.good(); }

  // Wszystkie pliki trafiają do GothicSurveying/<RRRR-MM-DD_GG-MM-SS>/ (czas zakończenia roboty) w katalogu roboczym gry.
  inline const char* LV_OUT_DIR = "GothicSurveying";

  inline std::string NowStamp() {
    time_t t = time( nullptr ); tm lt; localtime_s( &lt, &t );
    char b[32]; strftime( b, sizeof b, "%Y-%m-%d_%H-%M-%S", &lt );
    return b;
  }

  // Tworzy folder zakończonej roboty i zwraca jego ścieżkę (przy kolizji nazwy dopisuje _2, _3...).
  inline std::string MakeJobDir() {
    namespace fs = std::filesystem; std::error_code ec;
    const std::string base = std::string( LV_OUT_DIR ) + "/" + NowStamp();
    std::string dir = base;
    for ( int n = 2; fs::exists( dir, ec ) && n < 100; n++ ) dir = base + "_" + std::to_string( n );
    fs::create_directories( dir, ec );
    return dir;
  }

  inline bool WriteBytes( const std::string& path, const std::string& data ) {
    std::ofstream f( path, std::ios::binary );
    f.write( data.data(), (std::streamsize)data.size() );
    return f.good();
  }

  // Pomocniczy log pojedynczych odczytów: w trakcie roboty GothicSurveying/odczyty_biezace.csv (dopisywany na bieżąco, żeby przetrwał awarię),
  // po zakończeniu przenoszony do folderu roboty jako odczyty.csv.
  inline std::string PrepareReadingLog() {
    namespace fs = std::filesystem; std::error_code ec;
    fs::create_directories( LV_OUT_DIR, ec );
    const std::string cur = std::string( LV_OUT_DIR ) + "/odczyty_biezace.csv";
    if ( fs::exists( cur, ec ) ) fs::rename( cur, std::string( LV_OUT_DIR ) + "/odczyty_niezakonczone_" + NowStamp() + ".csv", ec );   // po awarii
    return cur;
  }
  inline void AppendReadingLog( const std::string& file, int pointId, int midMm ) {
    const bool fresh = !FileExistsRel( file );
    std::ofstream f( file, std::ios::app );
    if ( fresh ) f << "punkt;odczyt_srodkowa_mm\n";
    f << pointId << ";" << midMm << "\n";
  }
  inline void MoveReadingLog( const std::string& file, const std::string& dir ) {
    std::error_code ec; if ( std::filesystem::exists( file, ec ) ) std::filesystem::rename( file, dir + "/odczyty.csv", ec );
  }

  // Raport GNSS: pomiar_gnss.csv + pomiar_gnss.docx; aborted: tylko pomiar_gnss_przerwany.csv.
  inline bool ExportGnss( GnssReport r, const std::string& dir, bool aborted ) {
    r.dateText = DateTodayText();
    if ( aborted ) return WriteBytes( dir + "/pomiar_gnss_przerwany.csv", GnssToCsv( r ) );
    bool ok = WriteBytes( dir + "/pomiar_gnss.csv", GnssToCsv( r ) );
    ok = WriteBytes( dir + "/pomiar_gnss.docx", GnssBuildDocx( r ) ) && ok;
    return ok;
  }

  // aborted: tylko dziennik_przerwany.csv (bez DOCX); zwraca true, gdy wszystkie pliki zapisano.
  inline bool ExportFieldBook( const FieldBook& fb, const std::string& dir, bool aborted, const ManualVerdict* verdict = nullptr ) {
    if ( aborted ) return WriteBytes( dir + "/dziennik_przerwany.csv", ToCsv( fb ) );
    bool ok = WriteBytes( dir + "/dziennik.csv", verdict ? ToCsvWithVerdict( fb, *verdict ) : ToCsv( fb ) );
    ok = WriteBytes( dir + "/dziennik.docx", BuildDocx( fb, DateTodayText(), verdict ) ) && ok;
    return ok;
  }
}
