// Zapis dziennika do plików w katalogu roboczym gry (katalog główny gry): dziennik_N.docx + dziennik_N.csv, przerwany: tylko CSV.
// Wymaga calc/Docx.hpp dołączonego wcześniej. Bez strażnika include.
#include <ctime>
#include <fstream>
#include <string>

namespace GOTHIC_NAMESPACE {
  inline std::string DateTodayText() {                // rzeczywista data systemowa, RRRR-MM-DD
    time_t t = time( nullptr ); tm lt; localtime_s( &lt, &t );
    char b[16]; strftime( b, sizeof b, "%Y-%m-%d", &lt );
    return b;
  }

  inline bool FileExistsRel( const std::string& path ) { std::ifstream f( path, std::ios::binary ); return f.good(); }

  inline int NextExportSeq() {                        // pierwszy wolny numer N
    for ( int n = 1; n < 10000; n++ ) {
      std::string base = "dziennik_" + std::to_string( n );
      if ( !FileExistsRel( base + ".csv" ) && !FileExistsRel( base + ".docx" ) && !FileExistsRel( base + "_przerwany.csv" ) ) return n;
    }
    return 9999;
  }

  inline bool WriteBytes( const std::string& path, const std::string& data ) {
    std::ofstream f( path, std::ios::binary );
    f.write( data.data(), (std::streamsize)data.size() );
    return f.good();
  }

  // aborted: tylko dziennik_N_przerwany.csv (bez DOCX); zwraca true, gdy wszystkie pliki zapisano.
  inline bool ExportFieldBook( const FieldBook& fb, int seq, bool aborted ) {
    const std::string base = "dziennik_" + std::to_string( seq );
    if ( aborted ) return WriteBytes( base + "_przerwany.csv", ToCsv( fb ) );
    bool ok = WriteBytes( base + ".csv", ToCsv( fb ) );
    ok = WriteBytes( base + ".docx", BuildDocx( fb, DateTodayText() ) ) && ok;
    return ok;
  }
}
