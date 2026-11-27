// Adapter IWorld dla silnika: markery (reper/żabka), dwie łaty (role wstecz/wprzód), niwelator. Bez strażnika include.
// Wysokość punktu = wysokość stopy łaty (szczyt znaku/żabki), więc wszystkie odczyty liczą się względem tej samej płaszczyzny.
#include <ctime>
#include <map>
#include <random>

namespace GOTHIC_NAMESPACE {
  class EngineWorld : public IWorld {
   public:
    bool PlaceMarker( int pointId, MarkerKind kind ) override {
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      Pt pt; pt.pos = p; pt.reper = kind == MarkerKind::Reper;
      pt.marker = SpawnVob( pt.reper ? LV_VIS_REPER : LV_VIS_PLATE, p );
      pts_[pointId] = pt;
      return true;
    }
    void RemoveMarker( int pointId ) override {
      auto it = pts_.find( pointId );
      if ( it == pts_.end() ) return;
      RemoveVobSafe( it->second.marker ); pts_.erase( it );
    }
    bool FeetWithinCm( int pointId, double cm ) const override {
      auto it = pts_.find( pointId );
      if ( it == pts_.end() || !player ) return false;
      zVEC3 h = player->GetPositionWorld();
      double dx = h[VX] - it->second.pos[VX], dz = h[VZ] - it->second.pos[VZ];
      return sqrt( dx * dx + dz * dz ) <= cm;
    }
    void PlaceRod( RodRole role, int pointId ) override {
      auto it = pts_.find( pointId );
      if ( it == pts_.end() ) return;
      const int r = role == RodRole::Back ? 0 : 1;
      zVEC3 foot = Foot( pointId );
      if ( !rod_[r] ) rod_[r] = SpawnVob( LV_VIS_ROD[RodModelIdx()], foot ); else rod_[r]->SetPositionWorld( foot );
      rodPoint_[r] = pointId;
    }
    int RodModelIdx() const { return (std::max)( 0, (std::min)( 2, (int)( gRodLenCm / 100.0f + 0.5f ) - 3 ) ); }
    void RefreshRods() {                               // po zmianie wysuniecia: modele lat o nowej dlugosci na tych samych miejscach
      for ( int r = 0; r < 2; r++ ) if ( rod_[r] && rodPoint_[r] >= 0 ) { RemoveVobSafe( rod_[r] ); PlaceRod( r == 0 ? RodRole::Back : RodRole::Fore, rodPoint_[r] ); }
    }
    void RemoveRod( RodRole role ) override {
      const int r = role == RodRole::Back ? 0 : 1;
      RemoveVobSafe( rod_[r] ); rodPoint_[r] = -1;
    }
    bool PlaceInstrument() override {
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      SpawnInstrument( p, 0 );
      return true;
    }
    void RemoveInstrument() override { RemoveVobSafe( instr_ ); RemoveVobSafe( scanHead_ ); RemoveVobSafe( scope_ ); hasInstr_ = false; havePose_ = false; }
    double PointHeightM( int pointId ) const override { return pts_.count( pointId ) ? Foot( pointId )[VY] / 100.0 : 0.0; }
    double SightDistCm( int pointId ) const override {
      auto it = pts_.find( pointId );
      if ( it == pts_.end() || !hasInstr_ ) return 0;
      double dx = it->second.pos[VX] - instrGround_[VX], dz = it->second.pos[VZ] - instrGround_[VZ];
      return sqrt( dx * dx + dz * dz );
    }
    // --- GNSS: tyczka z odbiornikiem, promienie do nieba, zegar gry ---
    bool InstrumentLevelingEnabled() const override { return gLevelingOn; }
    bool TwoFaceByDefault() const override { return true; }
    bool PlaceBase( GridPos& g ) override {            // stacja bazowa RTK: model tripodu, jedna naraz
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      RemoveVobSafe( base_ ); base_ = SpawnVob( LV_VIS_GNSSTRI, p ); baseGround_ = p; hasBase_ = true;
      g = GnssToGrid( p[VX], p[VY], p[VZ] );
      return true;
    }
    bool PlacePole() override {
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      RemoveVobSafe( pole_ );
      pole_ = SpawnVob( LV_VIS_POLE, p ); poleGround_ = p; hasPole_ = true; poleIsPrism_ = false; poleTripod_ = false;
      return true;
    }
    bool PlaceGnssTripod() override {                  // statyka: odbiornik na statywie w slocie tyczki
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      RemoveVobSafe( pole_ );
      pole_ = SpawnVob( LV_VIS_GNSSTRI, p ); poleGround_ = p; hasPole_ = true; poleIsPrism_ = false; poleTripod_ = true;
      return true;
    }
    bool PlacePrism() override {                       // ten sam slot co tyczka GNSS (jedna tyczka naraz), inny model
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      RemoveVobSafe( pole_ );
      pole_ = SpawnVob( PrismModel( prismH_ ), p ); poleGround_ = p; hasPole_ = true; poleIsPrism_ = true;
      FaceInstrument( pole_ );
      return true;
    }
    bool PlaceTachymeter() override {                  // tachimetr na statywie (inny model niz niwelator)
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      PlaceTachymeterPos( p );
      return true;
    }
    bool PlaceScanner() override {
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      PlaceTachymeterPos( p, 2 );
      return true;
    }
    bool PlaceTachymeterAt( const ControlPoint& cp ) override {    // na znaku punktu osnowy: dokladnie na jego pozycji
      PlaceTachymeterPos( zVEC3( (float)cp.x, (float)cp.y, (float)cp.z ) );
      return true;
    }
    bool PlaceTachymeterOnPoint( int pointId ) override {          // nad znakiem punktu (reper A, punkt celu): staje dokladnie na jego pozycji
      if ( !pts_.count( pointId ) ) return false;
      PlaceTachymeterPos( Foot( pointId ) );
      return true;
    }
    bool PlacePrismOnPoint( int pointId ) override {
      if ( !pts_.count( pointId ) ) return false;
      const zVEC3 p = Foot( pointId );
      RemoveVobSafe( pole_ );
      pole_ = SpawnVob( PrismModel( prismH_ ), p ); poleGround_ = p; hasPole_ = true; poleIsPrism_ = true;
      FaceInstrument( pole_ );
      return true;
    }
    bool PlaceBacksight( int slot ) override {         // nawiazanie nr slot: znak + lustro na tyczce pod stopami
      zVEC3 p;
      if ( slot < 1 || slot > 5 || !FeetGround( p ) ) return false;
      PlaceBacksightPos( slot, p, true );
      return true;
    }
    bool PlaceBacksightAt( int slot, const ControlPoint& cp ) override {   // na znaku punktu osnowy (znak juz stoi)
      if ( slot < 1 || slot > 5 ) return false;
      PlaceBacksightPos( slot, zVEC3( (float)cp.x, (float)cp.y, (float)cp.z ), false );
      return true;
    }
    void RemoveBacksight( int slot ) override {
      if ( slot < 1 || slot > 5 ) return;
      Back& b = back_[slot - 1]; RemoveVobSafe( b.marker ); RemoveVobSafe( b.prism ); b.up = false;
    }
    std::string StoreControlPoint( const ControlPoint& cp ) override { return ControlAdd( cp ); }
    std::vector<ControlPoint> ControlNearHero( double maxM ) const override {
      ControlEnsureLoaded();
      if ( !player ) return {};
      const zVEC3 h = player->GetPositionWorld();
      return ControlNear( gControl, ControlWorldName(), h[VX], h[VZ], 0.0, maxM );
    }
    std::vector<ControlPoint> ControlNearStation( double minM, double maxM ) const override {
      ControlEnsureLoaded();
      if ( !hasInstr_ ) return {};
      return ControlNear( gControl, ControlWorldName(), instrGround_[VX], instrGround_[VZ], minM, maxM );
    }
    void SetPrismHeightM( double h ) override {        // fizyczna wysokosc lustra: modele tyczek tej wysokosci
      prismH_ = h;
      if ( poleIsPrism_ && pole_ ) { const zVEC3 p = poleGround_; RemoveVobSafe( pole_ ); pole_ = SpawnVob( PrismModel( prismH_ ), p ); FaceInstrument( pole_ ); }
      for ( int i = 0; i < 5; i++ ) {
        Back& b = back_[i];
        if ( !b.up || !b.prism ) continue;
        RemoveVobSafe( b.prism ); b.prism = SpawnVob( PrismModel( prismH_ ), b.pos ); FaceInstrument( b.prism );
      }
    }
    // Pomiar bezlustrowy: promien z oka lunety wzdluz celowej; pierwszy trafiony punkt (teren, budynki) i odleglosc skosna [m].
    bool TraceSight( double camAzDeg, double camPitchDeg, GridPos& hit, double& slopeM ) const override {
      zCWorld* w = ogame->GetWorld();
      if ( !w || !hasInstr_ ) return false;
      const float rad = 3.14159265f / 180.0f, az = (float)camAzDeg * rad, p = (float)camPitchDeg * rad;
      const zVEC3 from = instrGround_ + zVEC3( 0, LV_INSTR_H, 0 );
      const zVEC3 dir( sinf( az ) * cosf( p ), sinf( p ), cosf( az ) * cosf( p ) );
      if ( !w->TraceRayNearestHit( from, dir * 30000.0f, (zCVob*)nullptr,
           zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP ) ) return false;
      const zVEC3 h = w->traceRayReport.foundIntersection;
      hit = GnssToGrid( h[VX], h[VY], h[VZ] ); slopeM = ( h - from ).Length() / 100.0;
      return true;
    }
    // Skaner laserowy: promien z oka instrumentu; teren i voby ze swiata (bez postaci i voba skanera), zasieg 120 m.
    // --- kolor punktu skanu ---
    // Miniaturki kolorow tekstur z GothicSurveying/texcolors.bin (narzedzie assets/make_texture_colors.py): niezalezne od pamieci silnika.
    static std::map<std::string, TexThumb>& Thumbs() {
      static std::map<std::string, TexThumb> m; static bool tried = false;
      if ( !tried ) {
        tried = true;
        std::ifstream f( "GothicSurveying/texcolors.bin", std::ios::binary );
        const std::string d( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
        char t[120]; sprintf( t, "skan: miniaturki tekstur: %d wczytanych (GothicSurveying/texcolors.bin)", (int)TexColorsParse( d, m ) ); SvTrace( t );
      }
      return m;
    }
    static void TexName( zCTexture* tex, char* out, size_t n ) {   // nazwa obiektu tekstury (SEH, bez obiektow C++)
      __try { strncpy( out, tex->GetObjectName().ToChar(), n - 1 ); out[n - 1] = 0; } __except ( EXCEPTION_EXECUTE_HANDLER ) { out[0] = 0; }
    }
    static const TexThumb* ThumbFor( zCTexture* tex ) {            // po wskazniku tekstury (nazwa z obiektu, raz na teksture)
      static std::map<zCTexture*, const TexThumb*> cache;
      if ( !tex ) return nullptr;
      auto it = cache.find( tex );
      if ( it != cache.end() ) return it->second;
      char nb[256] = {}; TexName( tex, nb, sizeof nb );
      const std::string name = nb, key = TexColorKey( name );
      auto jt = Thumbs().find( key ); const TexThumb* th = jt == Thumbs().end() ? nullptr : &jt->second;
      static int logged = 0; if ( logged < 10 ) { logged++; char t[200]; sprintf( t, "skan: tekstura \"%s\" (klucz %s) -> %s", name.c_str(), key.c_str(), th ? "miniaturka jest" : "BRAK miniaturki" ); SvTrace( t ); }
      cache[tex] = th; return th;
    }
    // UV w miejscu trafienia wielokata swiata (interpolacja barycentryczna cech wierzcholkow); false gdy brak wielokata/tekstury.
    static bool PolyHitUv( zCPolygon* poly, const zVEC3& p, zCTexture*& tex, float& tu, float& tv ) {
      bool ok = false;
      __try {
        if ( poly && poly->material && poly->material->texture && poly->polyNumVert >= 3 && poly->vertex && poly->feature ) {
          float bestMin = -1e9f;
          for ( int i = 1; i + 1 < poly->polyNumVert; i++ ) {
            const zVEC3 a = poly->vertex[0]->position, b = poly->vertex[i]->position, c = poly->vertex[i + 1]->position;
            const float e0x = b[VX] - a[VX], e0y = b[VY] - a[VY], e0z = b[VZ] - a[VZ], e1x = c[VX] - a[VX], e1y = c[VY] - a[VY], e1z = c[VZ] - a[VZ];
            const float e2x = p[VX] - a[VX], e2y = p[VY] - a[VY], e2z = p[VZ] - a[VZ];
            const float d00 = e0x * e0x + e0y * e0y + e0z * e0z, d01 = e0x * e1x + e0y * e1y + e0z * e1z, d11 = e1x * e1x + e1y * e1y + e1z * e1z;
            const float d20 = e2x * e0x + e2y * e0y + e2z * e0z, d21 = e2x * e1x + e2y * e1y + e2z * e1z, den = d00 * d11 - d01 * d01;
            if ( fabsf( den ) < 1e-9f ) continue;
            const float wv = ( d11 * d20 - d01 * d21 ) / den, ww = ( d00 * d21 - d01 * d20 ) / den, wu = 1.0f - wv - ww;
            const float mn = ( std::min )( wu, ( std::min )( wv, ww ) );
            if ( mn > bestMin ) {
              bestMin = mn; ok = true;
              tu = wu * poly->feature[0]->texu + wv * poly->feature[i]->texu + ww * poly->feature[i + 1]->texu;
              tv = wu * poly->feature[0]->texv + wv * poly->feature[i]->texv + ww * poly->feature[i + 1]->texv;
            }
          }
          tex = poly->material->texture;
        }
      } __except ( EXCEPTION_EXECUTE_HANDLER ) { ok = false; }
      return ok && tex;
    }
    static unsigned MaterialRgb( zCMaterial* m ) {                // srednia barwa tekstury (miniaturka, potem info silnika albo kolor materialu); 0 gdy brak danych
      { zCTexture* t = nullptr; __try { t = m ? m->texture : nullptr; } __except ( EXCEPTION_EXECUTE_HANDLER ) {}
        if ( const TexThumb* th = ThumbFor( t ) ) return TexThumbAverage( *th ); }
      unsigned rgb = 0;
      __try {
        if ( m ) {
          zCOLOR c; c.dword = 0;
          if ( m->texture ) c = m->texture->GetTextureInfo().averageColor;
          if ( c.dword == 0 ) c = m->color;
          if ( c.dword != 0 ) rgb = ( (unsigned)c.r << 16 ) | ( (unsigned)c.g << 8 ) | (unsigned)c.b;
        }
      } __except ( EXCEPTION_EXECUTE_HANDLER ) {}
      return rgb;
    }
    static unsigned VobAverageRgb( zCVob* v ) {                    // voby: srednia z materialow siatki (nie wiemy, ktora czesc trafiono)
      unsigned sr = 0, sg = 0, sb = 0; int n = 0;
      __try {
        zCProgMeshProto* pm = v ? zDYNAMIC_CAST<zCProgMeshProto>( v->visual ) : nullptr;
        if ( pm && pm->subMeshList )
          for ( int i = 0; i < pm->numSubMeshes && i < 64; i++ ) {
            const unsigned c = MaterialRgb( pm->subMeshList[i].material );
            if ( c ) { sr += ( c >> 16 ) & 255; sg += ( c >> 8 ) & 255; sb += c & 255; n++; }
          }
      } __except ( EXCEPTION_EXECUTE_HANDLER ) { n = 0; }
      return n ? ( ( sr / n ) << 16 ) | ( ( sg / n ) << 8 ) | ( sb / n ) : 0u;
    }
    // Piksel tekstury w miejscu trafienia: wspolrzedne UV z interpolacji (barycentrycznie) cech wierzcholkow wielokata swiata, potem odczyt z bufora tekstury
    // (Lock + GetTextureBuffer) i wlasne dekodowanie DXT/16/32 bit (zCTexture::GetRGBAAtTexel zwraca zera dla DXT). Pierwsze probki trafiaja do logu (diagnostyka).
    // Srednia barwa tekstury z poziomu 1 (64x64 dla 128 px; poziom 0 i 1 maja spojne dane, nizsze nie): zamiennik, gdy tekstel jest przezroczysty albo brak danych.
    static bool LevelAverageRgb( zCTexture* tex, unsigned& rgb ) {
      bool ok = false;
      __try {
        if ( tex ) {
          const zCTextureInfo ti = tex->GetTextureInfo(); const int fmt = (int)ti.texFormat;
          if ( ti.sizeX > 1 && ti.numMipMap > 1 && tex->Lock( 1 ) ) {
            void* buf = nullptr; int pitch = 0;
            if ( tex->GetTextureBuffer( 1, buf, pitch ) && buf ) {
              const int lw = ( std::max )( 1, ti.sizeX >> 1 ), lh = ( std::max )( 1, ti.sizeY >> 1 ); long sr = 0, sg = 0, sb = 0, n = 0;
              for ( int yy = 0; yy < lh; yy++ ) for ( int xx = 0; xx < lw; xx++ ) { unsigned char q[3]; if ( TexDecodeTexel( fmt, (const unsigned char*)buf, pitch, lw, lh, xx, yy, q ) ) { sr += q[0]; sg += q[1]; sb += q[2]; n++; } }
              if ( n ) { rgb = ( (unsigned)( sr / n ) << 16 ) | ( (unsigned)( sg / n ) << 8 ) | (unsigned)( sb / n ); ok = true; }
            }
            tex->Unlock();
          }
        }
      } __except ( EXCEPTION_EXECUTE_HANDLER ) { ok = false; }
      return ok;
    }
    static unsigned TextureAverageCached( zCTexture* tex ) {       // 0 gdy brak; cache po wskazniku tekstury (liczone raz)
      static std::map<zCTexture*, unsigned> cache;
      auto it = cache.find( tex );
      if ( it != cache.end() ) return it->second;
      unsigned rgb = 0; if ( !LevelAverageRgb( tex, rgb ) ) rgb = 0;
      cache[tex] = rgb; return rgb;
    }
    static bool PolyTextureRgb( zCPolygon* poly, const zVEC3& p, unsigned& rgb ) {
      static int fails = 0, logged = 0;
      if ( fails > 20 ) return false;
      bool ok = false; float tu = 0, tv = 0, rr = 0, gg = 0, bb = 0; int sw = 0, sh = 0, fmt = -1, lk = 0, gb = 0, pitch = 0; unsigned char head[8] = {};
      __try {
        if ( poly && poly->material && poly->material->texture && poly->polyNumVert >= 3 && poly->vertex && poly->feature ) {
          float bestMin = -1e9f;
          for ( int i = 1; i + 1 < poly->polyNumVert; i++ ) {
            const zVEC3 a = poly->vertex[0]->position, b = poly->vertex[i]->position, c = poly->vertex[i + 1]->position;
            const float e0x = b[VX] - a[VX], e0y = b[VY] - a[VY], e0z = b[VZ] - a[VZ], e1x = c[VX] - a[VX], e1y = c[VY] - a[VY], e1z = c[VZ] - a[VZ];
            const float e2x = p[VX] - a[VX], e2y = p[VY] - a[VY], e2z = p[VZ] - a[VZ];
            const float d00 = e0x * e0x + e0y * e0y + e0z * e0z, d01 = e0x * e1x + e0y * e1y + e0z * e1z, d11 = e1x * e1x + e1y * e1y + e1z * e1z;
            const float d20 = e2x * e0x + e2y * e0y + e2z * e0z, d21 = e2x * e1x + e2y * e1y + e2z * e1z, den = d00 * d11 - d01 * d01;
            if ( fabsf( den ) < 1e-9f ) continue;
            const float wv = ( d11 * d20 - d01 * d21 ) / den, ww = ( d00 * d21 - d01 * d20 ) / den, wu = 1.0f - wv - ww;
            const float mn = ( std::min )( wu, ( std::min )( wv, ww ) );
            if ( mn > bestMin ) {
              bestMin = mn;
              tu = wu * poly->feature[0]->texu + wv * poly->feature[i]->texu + ww * poly->feature[i + 1]->texu;
              tv = wu * poly->feature[0]->texv + wv * poly->feature[i]->texv + ww * poly->feature[i + 1]->texv;
            }
          }
          zCTexture* tex = poly->material->texture;
          tu -= floorf( tu ); tv -= floorf( tv );
          const zCTextureInfo ti = tex->GetTextureInfo(); sw = ti.sizeX; sh = ti.sizeY; fmt = (int)ti.texFormat;
          tex->CacheIn( -1.0f );                                                       // dane tekstury w pamieci
          lk = tex->Lock( 1 );
          if ( lk ) {
            void* buf = nullptr;
            gb = tex->GetTextureBuffer( 0, buf, pitch );
            static int diagTex = 0;                                                  // diagnostyka ukladu poziomow: pitch i wskaznik dla kazdego poziomu pierwszych tekstur
            if ( diagTex < 6 && ti.numMipMap > 0 ) {
              diagTex++; char d[700]; int n = sprintf( d, "skan: poziomy tekstury %dx%d (ref %dx%d) mip=%d fmt=%d:", ti.sizeX, ti.sizeY, ti.refSizeX, ti.refSizeY, ti.numMipMap, fmt );
              for ( int lv = 0; lv < ti.numMipMap && lv < 6 && n < 600; lv++ ) {
                void* b2 = nullptr; int p2 = 0; const int g2 = tex->GetTextureBuffer( lv, b2, p2 );
                int ar = -1, ag = -1, ab = -1, okc = 0;                                // srednia barwa poziomu (wszystkie piksele) - czy poziomy sa spojne
                if ( g2 && b2 && ti.sizeX > 0 ) {
                  const int lw = ( std::max )( 1, ti.sizeX >> lv ), lh = ( std::max )( 1, ti.sizeY >> lv ); long sr = 0, sg = 0, sb = 0;
                  for ( int yy = 0; yy < lh; yy++ ) for ( int xx = 0; xx < lw; xx++ ) { unsigned char q[3]; if ( TexDecodeTexel( fmt, (const unsigned char*)b2, p2, lw, lh, xx, yy, q ) ) { sr += q[0]; sg += q[1]; sb += q[2]; okc++; } }
                  if ( okc ) { ar = (int)( sr / okc ); ag = (int)( sg / okc ); ab = (int)( sb / okc ); }
                }
                n += sprintf( d + n, " [%d: g=%d pitch=%d avg=(%d,%d,%d) ok=%d]", lv, g2, p2, ar, ag, ab, okc );
              }
              SvTrace( d );
              gb = tex->GetTextureBuffer( 0, buf, pitch );
            }
            if ( gb && buf ) {
              memcpy( head, buf, 8 );
              unsigned char c3[3];
              int ew = TexEffectiveWidth( fmt, pitch ); if ( ew <= 0 || sw <= 0 ) ew = sw;      // szerokosc poziomu z pitch (rozmiar z info bywa rozmiarem odniesienia)
              const int eh = ( std::max )( 1, sw > 0 ? (int)( (long long)sh * ew / sw ) : sh );
              if ( TexDecodeTexel( fmt, (const unsigned char*)buf, pitch, ew, eh, (int)( tu * ew ), (int)( tv * eh ), c3 ) ) { rr = c3[0]; gg = c3[1]; bb = c3[2]; ok = true; }
            }
            tex->Unlock();
          }
        }
      } __except ( EXCEPTION_EXECUTE_HANDLER ) { fails++; return false; }
      if ( logged < 16 ) {
        logged++; char t[240];
        sprintf( t, "skan: tekstura uv=(%.3f,%.3f) %dx%d format %d lock=%d bufor=%d pitch=%d bajty=%02X%02X%02X%02X%02X%02X%02X%02X -> %s rgb=(%.0f,%.0f,%.0f)", tu, tv, sw, sh, fmt, lk, gb, pitch,
                 head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7], ok ? "ok" : "BRAK", rr, gg, bb );
        SvTrace( t );
      }
      if ( !ok ) return false;
      rgb = ( (unsigned)rr << 16 ) | ( (unsigned)gg << 8 ) | (unsigned)bb;
      return true;
    }
    bool ScanRay( double azDeg, double elevDeg, double& distM, double& cosInc, bool& isVob, bool& isTarget, unsigned* rgb ) const override {
      zCWorld* w = ogame->GetWorld();
      if ( !w || !hasInstr_ ) return false;
      const float rad = 3.14159265f / 180.0f, az = (float)azDeg * rad, p = (float)elevDeg * rad;
      const zVEC3 from = instrGround_ + zVEC3( 0, LV_INSTR_H, 0 );
      const zVEC3 dir( sinf( az ) * cosf( p ), sinf( p ), cosf( az ) * cosf( p ) );
      double best = 1e18; isVob = false; isTarget = false; cosInc = 0.5;
      if ( w->TraceRayNearestHit( from, dir * 12000.0f, instr_, zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP | zTRACERAY_STAT_POLY | zTRACERAY_POLY_NORMAL ) ) {   // STAT_POLY + POLY_NORMAL: raport zawiera wielokat i normalna
        const zTTraceRayReport& r = w->traceRayReport;
        best = ( r.foundIntersection - from ).Length();
        zVEC3 n = r.foundPolyNormal; const float nl = n.Length();
        cosInc = nl > 0.5f ? fabs( ( n[VX] * dir[VX] + n[VY] * dir[VY] + n[VZ] * dir[VZ] ) / nl ) : 0.5;
        isVob = r.foundVob != nullptr;
        if ( rgb ) *rgb = HitColor( r, cosInc );
      }
      const double sphR = (double)kScanTargetRadiusM * 100.0;                       // kule referencyjne: przeciecie promienia z kula (analitycznie, bez kolizji vobow)
      for ( const ScanTgt& t : scanTgt_ ) {
        if ( !t.up ) continue;
        const zVEC3 c = t.pos + zVEC3( 0, (float)( kScanTargetUpM * 100.0 ), 0 ), oc = from - c;
        const double b = oc[VX] * dir[VX] + oc[VY] * dir[VY] + oc[VZ] * dir[VZ], k = (double)oc.Length() * oc.Length() - sphR * sphR, disc = b * b - k;
        if ( disc < 0 ) continue;
        const double tt = -b - sqrt( disc );
        if ( tt > 0 && tt < best && tt < 12000.0 ) {
          best = tt; isTarget = true; isVob = false;
          const zVEC3 hit = from + dir * (float)tt, nrm = ( hit - c ) * (float)( 1.0 / sphR );
          cosInc = fabs( nrm[VX] * dir[VX] + nrm[VY] * dir[VY] + nrm[VZ] * dir[VZ] );
        }
      }
      if ( best > 12000.0 ) return false;
      distM = best / 100.0;
      return true;
    }
    // Kolor trafienia: srednia barwa tekstury (miniaturka), przyciemniona katem padania; wspolny dla skanera naziemnego i lotniczego.
    unsigned HitColor( const zTTraceRayReport& r, double cosInc ) const {
      unsigned rgbOut = 0;
                                                                // kolor: srednia barwa tekstury, przyciemniona katem padania (powierzchnie skosne ciemniejsze)
          unsigned c = 0; bool have = false;
          if ( r.foundPoly && !r.foundVob ) {                                        // teren i budynki ze swiata: piksel tekstury, w razie braku srednia barwa
            zCTexture* tex = nullptr; float tu = 0, tv = 0;
            if ( PolyHitUv( r.foundPoly, r.foundIntersection, tex, tu, tv ) ) {
              if ( const TexThumb* th = ThumbFor( tex ) ) { c = TexThumbSample( *th, tu, tv ); have = true; }       // miniaturka tekstury w miejscu trafienia
            }
            if ( !have ) have = PolyTextureRgb( r.foundPoly, r.foundIntersection, c );                              // zapas: bufor tekstury silnika
            if ( !have && r.foundPoly->material && r.foundPoly->material->texture ) { c = TextureAverageCached( r.foundPoly->material->texture ); have = c != 0; }   // przezroczysty tekstel: srednia tekstury
            if ( !have ) { c = MaterialRgb( r.foundPoly->material ); have = c != 0; }
          } else if ( r.foundVob ) {                                                  // voby: material wielokata albo srednia z siatki
            if ( r.foundPoly ) c = MaterialRgb( r.foundPoly->material );
            if ( !c ) c = VobAverageRgb( r.foundVob );
            have = c != 0;
          }
          if ( !have ) c = 0x808080;
          const double k = 0.65 + 0.35 * cosInc;
      rgbOut = ( (unsigned)( ( ( c >> 16 ) & 255 ) * k ) << 16 ) | ( (unsigned)( ( ( c >> 8 ) & 255 ) * k ) << 8 ) | (unsigned)( ( c & 255 ) * k );
      return rgbOut;
    }
    void ScanHead( double azTrueDeg ) override {                                   // glowica obraca sie za azymutem skanowanej kolumny
      if ( !scanHead_ || VobIsStale( scanHead_ ) ) return;
      const float a = (float)azTrueDeg * 3.14159265f / 180.0f;
      scanHead_->SetHeadingAtWorld( zVEC3( sinf( a ), 0, cosf( a ) ) );
    }
    void ScanBeam( double azTrueDeg, double elevDeg, double distM ) override {
      if ( distM < 0 ) { beams_.clear(); return; }
      beams_.push_back( { azTrueDeg, elevDeg, distM } );
      if ( beams_.size() > 40 ) beams_.erase( beams_.begin() );
    }
    void ScanVisualTick() const {                                                 // co klatke: wiazki lasera od glowicy do ostatnich trafien
      if ( !abeams_.empty() && zlineCache ) {                                     // wiazki drona (punkt poczatkowy zapamietany w momencie impulsu)
        const float rad = 3.14159265f / 180.0f;
        for ( const ABeam& b : abeams_ ) {
          const float az = (float)b.az * rad, el = (float)b.el * rad; const zVEC3 dir( sinf( az ) * cosf( el ), sinf( el ), cosf( az ) * cosf( el ) );
          zlineCache->Line3D( b.from, b.from + dir * (float)( b.dist * 100.0 ), GFX_RED, 0 );
        }
      }
      if ( beams_.empty() || !hasInstr_ || !zlineCache ) return;
      const zVEC3 eye = instrGround_ + zVEC3( 0, LV_INSTR_H, 0 );
      const float rad = 3.14159265f / 180.0f;
      for ( const Beam& b : beams_ ) {
        const float az = (float)b.az * rad, el = (float)b.el * rad;
        const zVEC3 dir( sinf( az ) * cosf( el ), sinf( el ), cosf( az ) * cosf( el ) );
        zlineCache->Line3D( eye, eye + dir * (float)( b.dist * 100.0 ), GFX_RED, 0 );
      }
    }
    bool PlaceScanTarget( int slot, const ControlPoint& cp ) override {          // kula na znaku punktu osnowy (slot 1-6)
      if ( slot < 1 || slot > 6 ) return false;
      ScanTgt& t = scanTgt_[slot - 1];
      RemoveVobSafe( t.vob );
      t.pos = zVEC3( (float)cp.x, (float)cp.y, (float)cp.z ); t.up = true; t.vob = SpawnVob( LV_VIS_SPHERE, t.pos );
      return true;
    }
    void RemoveScanTarget( int slot ) override {
      if ( slot < 1 || slot > 6 ) return;
      RemoveVobSafe( scanTgt_[slot - 1].vob ); scanTgt_[slot - 1].up = false;
    }
    bool ScanRegistrationByDefault() const override { return true; }
    std::string ScanOutDir() override { return OutDirWithSuffix( "_skan" ); }
    std::string AerialOutDir() override { return OutDirWithSuffix( "_lot" ); }
    // --- nalot ALS: dron (korpus + 4 smigla), promienie z dowolnego punktu, wysokosc terenu, kamera za dronem ---
    bool AerialRay( const GridPos& from, double azDeg, double elevDeg, double maxM, double& distM, double& cosInc, bool& isVob, unsigned* rgb ) const override {
      zCWorld* w = ogame->GetWorld();
      if ( !w ) return false;
      double gx, gy, gz; GnssFromGrid( from, gx, gy, gz );
      const zVEC3 org( (float)gx, (float)gy, (float)gz );
      const float rad = 3.14159265f / 180.0f, az = (float)azDeg * rad, p = (float)elevDeg * rad;
      const zVEC3 dir( sinf( az ) * cosf( p ), sinf( p ), cosf( az ) * cosf( p ) );
      if ( !w->TraceRayNearestHit( org, dir * (float)( maxM * 100.0 ), (zCVob*)nullptr, zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP | zTRACERAY_STAT_POLY | zTRACERAY_POLY_NORMAL ) ) return false;
      const zTTraceRayReport& r = w->traceRayReport;
      distM = ( r.foundIntersection - org ).Length() / 100.0;
      zVEC3 n = r.foundPolyNormal; const float nl = n.Length();
      cosInc = nl > 0.5f ? fabs( ( n[VX] * dir[VX] + n[VY] * dir[VY] + n[VZ] * dir[VZ] ) / nl ) : 0.5;
      isVob = r.foundVob != nullptr;
      if ( rgb ) *rgb = HitColor( r, cosInc );
      return true;
    }
    bool GroundHeightM( double n, double e, double& h ) const override {            // najwyzszy wielokat statyczny w (N, E): teren (dachy tez, bo dron leci nad nimi)
      zCWorld* w = ogame->GetWorld();
      if ( !w ) return false;
      double gx, gy, gz; GnssFromGrid( GridPos{ n, e, 0 }, gx, gy, gz );
      if ( !w->TraceRayNearestHit( zVEC3( (float)gx, 100000.0f, (float)gz ), zVEC3( 0, -300000.0f, 0 ), (zCVob*)nullptr, zTRACERAY_VOB_IGNORE | zTRACERAY_POLY_IGNORE_TRANSP ) ) return false;
      h = w->traceRayReport.foundIntersection[VY] / 100.0 + GNSS_H0;
      return true;
    }
    bool HeroGround( GridPos& g ) const override {
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      g = GnssToGrid( p[VX], p[VY], p[VZ] );
      return true;
    }
    bool RouteBlocked( const GridPos& a, const GridPos& b ) const override {
      zCWorld* w = ogame->GetWorld();
      if ( !w ) return false;
      double ax, ay, az, bx, by, bz; GnssFromGrid( a, ax, ay, az ); GnssFromGrid( b, bx, by, bz );
      const zVEC3 from( (float)ax, (float)ay, (float)az ), to( (float)bx, (float)by, (float)bz );
      return w->TraceRayNearestHit( from, to - from, (zCVob*)nullptr, zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP ) != 0;
    }
    bool PlaceDrone( const GridPos& pos ) override {
      RemoveDrone();
      double gx, gy, gz; GnssFromGrid( pos, gx, gy, gz );
      const zVEC3 p( (float)gx, (float)gy, (float)gz );
      drone_ = SpawnVob( LV_VIS_DRONE, p );
      for ( int i = 0; i < 4; i++ ) prop_[i] = SpawnVob( LV_VIS_PROP, p );
      droneUp_ = drone_ != nullptr;
      char b[120]; sprintf( b, "dron: postawiony (%.0f, %.0f, %.0f)", p[VX], p[VY], p[VZ] ); SvTrace( b );
      return droneUp_;
    }
    void RemoveDrone() override { RemoveVobSafe( drone_ ); for ( zCVob*& v : prop_ ) RemoveVobSafe( v ); droneUp_ = false; }
    void SetDronePose( const GridPos& pos, double yawDeg, double pitchDeg, double rollDeg, double propPhaseDeg ) override {
      if ( !droneUp_ || !drone_ || VobIsStale( drone_ ) ) return;
      double gx, gy, gz; GnssFromGrid( pos, gx, gy, gz );
      const zVEC3 p( (float)gx, (float)gy, (float)gz );
      const Rot3 R = RotFromRpy( rollDeg, pitchDeg, yawDeg );
      drone_->SetTrafoObjToWorld( DroneTrafo( drone_->trafoObjToWorld, R, p ) );
      static const double ox[4] = { 45, -45, 45, -45 }, oy[4] = { 45, 45, -45, -45 };      // silniki w ukladzie ciala drona [cm]: x prawo, y przod, z gora
      for ( int i = 0; i < 4; i++ ) {
        if ( !prop_[i] || VobIsStale( prop_[i] ) ) continue;
        const Vec3 o = Mul( R, Vec3{ ox[i], oy[i], 8.0 } );                                // ENU (E, N, U) -> gra (x = E, y = U, z = N)
        const Rot3 Rs = MulR( R, RotFromRpy( 0, 0, ( i % 2 ? -1.0 : 1.0 ) * propPhaseDeg ) );
        prop_[i]->SetTrafoObjToWorld( DroneTrafo( prop_[i]->trafoObjToWorld, Rs, p + zVEC3( (float)o.x, (float)o.z, (float)o.y ) ) );
      }
    }
    void AerialBeam( const GridPos& from, double azDeg, double elevDeg, double distM ) override {     // wiazki lasera drona: ostatnie ok. 40, rysowane w ScanVisualTick
      if ( distM < 0 ) { abeams_.clear(); return; }
      double gx, gy, gz; GnssFromGrid( from, gx, gy, gz ); abeams_.push_back( { zVEC3( (float)gx, (float)gy, (float)gz ), azDeg, elevDeg, distM } );
      if ( abeams_.size() > 40 ) abeams_.erase( abeams_.begin() );
    }
    void SetFollowCamera( bool on ) override { followCam_ = on; camLog_ = 0; SvTrace( on ? "kamera za dronem: wlaczona" : "kamera za dronem: wylaczona" ); }
    void FollowCamTick() {                                                              // co klatke po ruchu drona; kamera 12 m za dronem i 5 m nad nim
      if ( !followCam_ || !droneUp_ || !drone_ || VobIsStale( drone_ ) ) return;
      zCVob* cam = ogame->GetCameraVob();
      if ( !cam ) return;
      const zVEC3 dp = drone_->GetPositionWorld(); zVEC3 at = drone_->GetAtVectorWorld(); at[VY] = 0;
      if ( at.Length() > 0.01f ) at.Normalize();
      const zVEC3 cp = dp - at * 1200.0f + zVEC3( 0, 500.0f, 0 ); zVEC3 look = dp - cp; look.Normalize();
      cam->SetPositionWorld( cp ); cam->SetHeadingAtWorld( look );
      if ( camLog_ < 3 ) { camLog_++; const zVEC3 rb = cam->GetPositionWorld(); char b[160]; sprintf( b, "kamera za dronem: ustawiono (%.0f,%.0f,%.0f), odczyt (%.0f,%.0f,%.0f)", cp[VX], cp[VY], cp[VZ], rb[VX], rb[VY], rb[VZ] ); SvTrace( b ); }
    }
    GridPos StationGrid() const override { return GnssToGrid( instrGround_[VX], instrGround_[VY], instrGround_[VZ] ); }
    GridPos PointGrid( int slot ) const override {
      if ( slot < 1 || slot > 5 || !back_[slot - 1].up ) return GridPos{ 0, 0, 0 };
      const zVEC3& p = back_[slot - 1].pos;
      return GnssToGrid( p[VX], p[VY], p[VZ] );
    }
    // Linia celowa od oka tachimetru do lustra / znaku nawiazania: bez trafienia w teren lub voby (ostatnie 30 cm pomijamy).
    bool LineClear( bool toPrism, int slot = 1 ) const override {
      zCWorld* w = ogame->GetWorld();
      if ( !w || !hasInstr_ ) return true;
      zVEC3 target;
      if ( toPrism ) { if ( !hasPole_ ) return true; target = poleGround_ + zVEC3( 0, (float)( prismH_ * 100.0 ), 0 ); }
      else { if ( slot < 1 || slot > 5 || !back_[slot - 1].up ) return true; target = back_[slot - 1].pos + zVEC3( 0, (float)( prismH_ * 100.0 ), 0 ); }
      const zVEC3 from = instrGround_ + zVEC3( 0, LV_INSTR_H, 0 );
      zVEC3 to = target - from; const float len = to.Length();
      if ( len < 40.0f ) return true;
      to = to * ( ( len - 30.0f ) / len );
      return !w->TraceRayNearestHit( from, to, (zCVob*)nullptr, zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP );
    }
    void RemovePole() override { RemoveVobSafe( pole_ ); hasPole_ = false; poleIsPrism_ = false; }
    GridPos PoleGroundGrid() const override { return GnssToGrid( poleGround_[VX], poleGround_[VY], poleGround_[VZ] ); }
    double NowSec() const override { return clock_; }
    std::string TimeText() const override {
      time_t t = time( nullptr ); tm lt; localtime_s( &lt, &t );
      char b[16]; strftime( b, sizeof b, "%H:%M:%S", &lt );
      return b;
    }
    void AdvanceClock( double dtSec ) { clock_ += dtSec; }
    bool HasPole() const { return hasPole_; }
    zVEC3 PoleGround() const { return poleGround_; }
    // Precyzyjne ustawienie: przesuwa tyczkę o (dxCm, dzCm) od podanego punktu i osadza ją na gruncie pod nowym miejscem.
    bool MovePoleFrom( const zVEC3& from, double dxCm, double dzCm ) {
      zCWorld* w = ogame->GetWorld();
      if ( !hasPole_ || !pole_ || !w ) return false;
      const zVEC3 top( from[VX] + (float)dxCm, from[VY] + 100.0f, from[VZ] + (float)dzCm );
      if ( !w->TraceRayNearestHit( top, zVEC3( 0, -3000, 0 ), (zCVob*)player, zTRACERAY_VOB_IGNORE | zTRACERAY_POLY_IGNORE_TRANSP ) ) return false;
      poleGround_ = w->traceRayReport.foundIntersection;
      pole_->SetPositionWorld( poleGround_ );
      return true;
    }
    void SetDressVisible( bool v ) { if ( helmet_ ) helmet_->showVisual = v ? 1 : 0; if ( vest_ ) vest_->showVisual = v ? 1 : 0; }
    // Maska horyzontu: 36 sektorów po 10 stopni, promienie co 15 stopni elewacji (5-80) od szczytu tyczki; horyzont sektora =
    // najwyższy zasłonięty kierunek + pół kroku. Najbliższa przeszkoda = najmniejsza pozioma odległość trafienia.
    SkyMask SkyHorizon() const override {
      SkyMask m; zCWorld* w = ogame->GetWorld();
      if ( !hasPole_ || !w ) return m;
      const zVEC3 from( poleGround_[VX], poleGround_[VY] + LV_POLE_H, poleGround_[VZ] );
      const float rad = 3.14159265f / 180.0f;
      static const int els[6] = { 5, 20, 35, 50, 65, 80 };
      double nearestCm = 1e9;
      for ( int s = 0; s < 36; s++ ) {
        const float az = ( s * 10 + 5 ) * rad;
        double blocked = 0;
        for ( int el : els ) {
          const float e = el * rad;
          const zVEC3 dir( sinf( az ) * cosf( e ), sinf( e ), cosf( az ) * cosf( e ) );
          if ( !w->TraceRayNearestHit( from, dir * 8000.0f, (zCVob*)nullptr,
               zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP ) ) continue;
          blocked = el + 7.5;                           // elewacje rosną, więc zostaje najwyższa zasłonięta
          const double d = ( w->traceRayReport.foundIntersection - from ).Length() * cosf( e );
          if ( d < nearestCm ) nearestCm = d;
        }
        m.horizonDeg[s] = blocked;
      }
      m.nearestObstacleM = nearestCm > 1e8 ? 100.0 : nearestCm / 100.0;
      return m;
    }
    double Random01() override { static std::mt19937 g{ std::random_device{}() }; return std::uniform_real_distribution<double>( 0.0, 1.0 )( g ); }
    void ShiftInstrumentMm( double dyMm ) override {   // oś lunety (kamera) i model niwelatora idą razem w górę/w dół
      if ( !hasInstr_ ) return;
      instrGround_[VY] += (float)( dyMm / 10.0 );
      if ( instr_ ) instr_->SetPositionWorld( instrGround_ );
      if ( instrKind_ != 2 && scanHead_ ) scanHead_->SetPositionWorld( instrGround_ );
      if ( scope_ ) scope_->SetPositionWorld( instrGround_ + zVEC3( 0, LV_INSTR_H, 0 ) );
    }
    void Say( const std::string& text ) override { HudMessage( text.c_str() ); }

    // --- tylko silnik ---
    bool HasInstrument() const { return hasInstr_; }
    zVEC3 InstrumentGround() const { return instrGround_; }
    std::vector<zVEC3> PrismTops() const {             // srodki lusterek (nawiazania i tyczka z lustrem) - cele szybkiego najechania w lunecie
      std::vector<zVEC3> v; const zVEC3 up( 0, (float)( prismH_ * 100.0 ), 0 );
      for ( const Back& b : back_ ) if ( b.up ) v.push_back( b.pos + up );
      if ( hasPole_ && poleIsPrism_ ) v.push_back( poleGround_ + up );
      return v;
    }
    zVEC3 PointFoot( int pointId ) const { return Foot( pointId ); }
    void SetInstrumentVisible( bool v ) {
      if ( instr_ ) instr_->showVisual = v ? 1 : 0;
      if ( instrKind_ != 2 && scanHead_ ) scanHead_->showVisual = v ? 1 : 0;   // skaner: glowica zawsze widoczna (steruje nia skan)
      if ( scope_ ) scope_->showVisual = v ? 1 : 0;
    }
    // Poza instrumentu = kierunek lunety (azymut, pochylenie [rad]): obraca alidade/gorna czesc i lunete modelu; pamiec kierunku miedzy wejsciami do lunety.
    void SetInstrumentPose( float az, float pitch ) {
      poseAz_ = az; posePitch_ = pitch; havePose_ = true;
      if ( !hasInstr_ || instrKind_ == 2 ) return;
      if ( scanHead_ && !VobIsStale( scanHead_ ) ) scanHead_->SetHeadingAtWorld( zVEC3( sinf( az ), 0, cosf( az ) ) );
      if ( scope_ && !VobIsStale( scope_ ) ) scope_->SetHeadingAtWorld( zVEC3( sinf( az ) * cosf( pitch ), sinf( pitch ), cosf( az ) * cosf( pitch ) ) );
    }
    bool InstrumentPose( float& az, float& pitch ) const { if ( !havePose_ || !hasInstr_ ) return false; az = poseAz_; pitch = posePitch_; return true; }
    void TurnRodsToInstrument() {                      // łaty zwrócone przodem do instrumentu (do odczytu)
      for ( int r = 0; r < 2; r++ ) {
        if ( !rod_[r] || !hasInstr_ ) continue;
        zVEC3 f = Foot( rodPoint_[r] );
        zVEC3 d( instrGround_[VX] - f[VX], 0, instrGround_[VZ] - f[VZ] );
        if ( d.Length() > 0.1f ) { d.Normalize(); rod_[r]->SetHeadingAtWorld( d ); }
      }
    }
    // Kask i kamizelka: zwykłe voby, które co klatkę (DressTick) stawiamy w węzłach szkieletu bohatera (ZS_HELMET, ZS_TORSO),
    // obrócone jak bohater. Nie używamy AttachChildVobToNode - w grze vob zostawał w punkcie (0,0,0). Pancerza bohatera nie ruszamy.
    void DressOn() {
      if ( !player || helmet_ || vest_ ) return;
      zCModel* model = player->GetModel();
      if ( !model ) return;
      helmetNode_ = model->SearchNode( zSTRING( "ZS_HELMET" ) );
      for ( const char* name : { "ZS_TORSO", "BIP01 SPINE2", "BIP01 SPINE1", "BIP01 SPINE" } ) {   // ZS_TORSO nie istnieje w modelu bohatera
        vestNode_ = model->SearchNode( zSTRING( name ) );
        SvTrace( ( std::string( "wezel " ) + name + ( vestNode_ ? ": jest" : ": brak" ) ).c_str() );
        if ( vestNode_ ) break;
      }
      if ( !helmetNode_ || !vestNode_ ) SvTrace( "brak wezla szkieletu dla ubioru" );
      dressPos_[0] = dressPos_[1] = zVEC3( 1e9f, 0, 0 ); dressAt_[0] = dressAt_[1] = zVEC3( 0, 0, 0 );
      if ( helmetNode_ ) helmet_ = SpawnVob( LV_VIS_HELMET, player->GetPositionWorld() );
      if ( vestNode_ )   vest_   = SpawnVob( LV_VIS_VEST, player->GetPositionWorld() );
      DressTick();
      for ( int i = 0; i < 2; i++ ) {                   // diagnostyka: gdzie leża węzły względem stóp bohatera
        zCModelNodeInst* n = i ? vestNode_ : helmetNode_;
        if ( !n ) continue;
        zVEC3 d = model->GetNodePositionWorld( n ) - player->GetPositionWorld();
        char b[96]; sprintf( b, "wezel %s wzgl. bohatera: dx=%.1f dy=%.1f dz=%.1f", i ? "kamizelki" : "ZS_HELMET", d[VX], d[VY], d[VZ] );
        SvTrace( b );
      }
    }
    void DressTick() {                                 // tanio: ruszamy voba tylko, gdy węzeł lub kierunek bohatera naprawdę się zmieniły
      zCModel* model = player ? player->GetModel() : nullptr;
      if ( !model || ( !helmet_ && !vest_ ) ) return;
      zVEC3 at = player->GetAtVectorWorld(); at[VY] = 0;
      if ( at.Length() > 0.01f ) at.Normalize();
      for ( int i = 0; i < 2; i++ ) {
        zCVob* v = i ? vest_ : helmet_;
        if ( !v || VobIsStale( v ) ) continue;
        const zVEC3 pos = model->GetNodePositionWorld( i ? vestNode_ : helmetNode_ );
        const zVEC3 d = pos - dressPos_[i];
        const bool moved = d[VX] * d[VX] + d[VY] * d[VY] + d[VZ] * d[VZ] > 0.04f;
        const bool turned = ( at[VX] * dressAt_[i][VX] + at[VZ] * dressAt_[i][VZ] ) < 0.99995f;
        if ( moved || turned ) {                        // pełny obrót bohatera (też pochylenie) + pozycja węzła
          zMAT4 m = player->trafoObjToWorld; m.SetTranslation( pos );
          v->SetTrafoObjToWorld( m ); dressPos_[i] = pos; dressAt_[i] = at;
        }
      }
    }
    void DressOff() { RemoveVobSafe( helmet_ ); RemoveVobSafe( vest_ ); helmetNode_ = vestNode_ = nullptr; }
    // Po zmianie świata: oddaje własne referencje bez dotykania świata (voby zwolnił już świat).
    void ForgetAll() {
      for ( auto& kv : pts_ ) DropVobRef( kv.second.marker );
      DropVobRef( rod_[0] ); DropVobRef( rod_[1] ); DropVobRef( instr_ ); DropVobRef( scanHead_ ); DropVobRef( scope_ ); DropVobRef( pole_ ); DropVobRef( base_ ); for ( ScanTgt& t : scanTgt_ ) { DropVobRef( t.vob ); t.up = false; } hasPole_ = false; poleIsPrism_ = false;
      for ( Back& b : back_ ) { DropVobRef( b.marker ); DropVobRef( b.prism ); b.up = false; }
      DropVobRef( helmet_ ); DropVobRef( vest_ ); helmetNode_ = vestNode_ = nullptr;
      DropVobRef( drone_ ); for ( zCVob*& v : prop_ ) DropVobRef( v ); droneUp_ = false; followCam_ = false;
      pts_.clear(); rodPoint_[0] = rodPoint_[1] = -1; hasInstr_ = false; hasBase_ = false;
    }
    bool Stale() const {                                 // któryś z naszych vobów nie należy już do bieżącego świata
      for ( const auto& kv : pts_ ) if ( VobIsStale( kv.second.marker ) ) return true;
      return VobIsStale( rod_[0] ) || VobIsStale( rod_[1] ) || VobIsStale( instr_ ) || VobIsStale( scanHead_ ) || VobIsStale( scope_ ) || VobIsStale( helmet_ ) || VobIsStale( vest_ ) || VobIsStale( pole_ ) || VobIsStale( base_ ) || VobIsStale( drone_ ) || ScanTargetsStale() || BacksightsStale();
    }
    void ClearAll() {
      for ( auto& kv : pts_ ) RemoveVobSafe( kv.second.marker );
      RemoveVobSafe( rod_[0] ); RemoveVobSafe( rod_[1] ); RemoveVobSafe( instr_ ); RemoveVobSafe( scanHead_ ); RemoveVobSafe( scope_ ); RemoveVobSafe( pole_ ); RemoveVobSafe( base_ ); for ( ScanTgt& t : scanTgt_ ) { RemoveVobSafe( t.vob ); t.up = false; } hasPole_ = false; poleIsPrism_ = false;
      for ( int i = 1; i <= 5; i++ ) RemoveBacksight( i );
      DressOff(); RemoveDrone(); followCam_ = false;
      pts_.clear(); rodPoint_[0] = rodPoint_[1] = -1; hasInstr_ = false; hasBase_ = false;
    }

    // Zapis i odtworzenie sprzetu roboty (znaki, lacie, instrument, tyczka, nawiazania, baza) - zapis gry w trakcie roboty.
    static void PutPos( StateKV& s, const std::string& k, const zVEC3& p ) { s.Num( k + ".x", p[VX] ); s.Num( k + ".y", p[VY] ); s.Num( k + ".z", p[VZ] ); }
    static zVEC3 GetPos( const StateKV& s, const std::string& k ) { return zVEC3( (float)s.GetNum( k + ".x" ), (float)s.GetNum( k + ".y" ), (float)s.GetNum( k + ".z" ) ); }
    void SaveState( StateKV& s ) const {
      s.Num( "prismH", prismH_ ); s.Int( "pts.n", (long long)pts_.size() ); int i = 0;
      for ( const auto& kv : pts_ ) { const std::string p = "pts." + std::to_string( i++ ); s.Int( p + ".id", kv.first ); PutPos( s, p, kv.second.pos ); s.Bool( p + ".reper", kv.second.reper ); }
      s.Int( "rod0", rodPoint_[0] ); s.Int( "rod1", rodPoint_[1] );
      s.Bool( "instr", hasInstr_ ); s.Int( "instrKind", instrKind_ ); PutPos( s, "instrPos", instrGround_ );
      s.Bool( "havePose", havePose_ ); s.Num( "poseAz", poseAz_ ); s.Num( "posePitch", posePitch_ );
      s.Bool( "pole", hasPole_ ); s.Bool( "poleIsPrism", poleIsPrism_ ); s.Bool( "poleTripod", poleTripod_ ); PutPos( s, "polePos", poleGround_ );
      for ( int k = 0; k < 5; k++ ) { const std::string p = "back" + std::to_string( k ); s.Bool( p + ".up", back_[k].up ); s.Bool( p + ".marker", back_[k].marker != nullptr ); PutPos( s, p, back_[k].pos ); }
      s.Bool( "base", hasBase_ ); PutPos( s, "basePos", baseGround_ );
      for ( int k = 0; k < 6; k++ ) { const std::string p = "sct" + std::to_string( k ); s.Bool( p + ".up", scanTgt_[k].up ); PutPos( s, p, scanTgt_[k].pos ); }
    }
    void LoadState( const StateKV& s ) {                // po CtrlForgetJob/ClearAll: swiat bez naszych obiektow
      prismH_ = s.GetNum( "prismH", 1.5 );
      const int n = s.GetInt( "pts.n" );
      for ( int i = 0; i < n; i++ ) {
        const std::string p = "pts." + std::to_string( i ); Pt pt; pt.pos = GetPos( s, p ); pt.reper = s.GetBool( p + ".reper" );
        pt.marker = SpawnVob( pt.reper ? LV_VIS_REPER : LV_VIS_PLATE, pt.pos ); pts_[s.GetInt( p + ".id" )] = pt;
      }
      if ( s.GetBool( "instr" ) ) {
        const zVEC3 ip = GetPos( s, "instrPos" );
        const int kind = s.Has( "instrKind" ) ? s.GetInt( "instrKind" ) : ( s.GetBool( "instrTachy" ) ? 1 : 0 );
        if ( kind >= 1 ) PlaceTachymeterPos( ip, kind );
        else SpawnInstrument( ip, 0 );
        if ( s.GetBool( "havePose" ) ) SetInstrumentPose( (float)s.GetNum( "poseAz" ), (float)s.GetNum( "posePitch" ) );
      }
      for ( int r = 0; r < 2; r++ ) { const int id = s.GetInt( r ? "rod1" : "rod0", -1 ); if ( id >= 0 ) PlaceRod( r ? RodRole::Fore : RodRole::Back, id ); }
      if ( s.GetBool( "pole" ) ) {
        poleIsPrism_ = s.GetBool( "poleIsPrism" ); poleGround_ = GetPos( s, "polePos" ); hasPole_ = true;
        poleTripod_ = s.GetBool( "poleTripod" );
        pole_ = SpawnVob( poleIsPrism_ ? PrismModel( prismH_ ) : poleTripod_ ? LV_VIS_GNSSTRI : LV_VIS_POLE, poleGround_ );
        if ( poleIsPrism_ ) FaceInstrument( pole_ );
      }
      for ( int k = 0; k < 5; k++ ) {
        const std::string p = "back" + std::to_string( k );
        if ( s.GetBool( p + ".up" ) ) PlaceBacksightPos( k + 1, GetPos( s, p ), s.GetBool( p + ".marker" ) );
      }
      for ( int k = 0; k < 6; k++ ) { const std::string p = "sct" + std::to_string( k ); if ( s.GetBool( p + ".up" ) ) { scanTgt_[k].pos = GetPos( s, p ); scanTgt_[k].up = true; scanTgt_[k].vob = SpawnVob( LV_VIS_SPHERE, scanTgt_[k].pos ); } }
      if ( s.GetBool( "base" ) ) { baseGround_ = GetPos( s, "basePos" ); hasBase_ = true; base_ = SpawnVob( LV_VIS_GNSSTRI, baseGround_ ); }
    }

   private:
    static std::string OutDirWithSuffix( const char* suffix ) {
      time_t t = time( nullptr ); tm lt; localtime_s( &lt, &t ); char st[32]; strftime( st, sizeof st, "%Y-%m-%d_%H-%M-%S", &lt );
      std::error_code ec; const std::string dir = std::string( "GothicSurveying/" ) + st + suffix;
      std::filesystem::create_directories( dir, ec );
      return ec ? std::string() : dir;
    }
    // Macierz obiektu z obrotu ENU (x prawo, y przod, z gora) i pozycji w swiecie gry (x = E, y = H, z = N).
    static zMAT4 DroneTrafo( zMAT4 m, const Rot3& R, const zVEC3& pos ) {
      m.SetRightVector( zVEC3( (float)R.m[0], (float)R.m[6], (float)R.m[3] ) );
      m.SetUpVector( zVEC3( (float)R.m[2], (float)R.m[8], (float)R.m[5] ) );
      m.SetAtVector( zVEC3( (float)R.m[1], (float)R.m[7], (float)R.m[4] ) );
      m.SetTranslation( pos );
      return m;
    }
    struct ABeam { zVEC3 from; double az, el, dist; }; std::vector<ABeam> abeams_;   // wiazki drona
    zCVob* drone_ = nullptr; zCVob* prop_[4] = { nullptr, nullptr, nullptr, nullptr }; bool droneUp_ = false, followCam_ = false; int camLog_ = 0;   // dron ALS
    struct Pt { zVEC3 pos; zCVob* marker = nullptr; bool reper = false; };
    zVEC3 Foot( int pointId ) const {
      const Pt& pt = pts_.at( pointId );
      return pt.pos + zVEC3( 0, pt.reper ? LV_REPER_H : LV_PLATE_H, 0 );
    }
    std::map<int, Pt> pts_;
    zCVob* rod_[2] = { nullptr, nullptr };
    int rodPoint_[2] = { -1, -1 };
    zCVob* instr_ = nullptr; zVEC3 instrGround_; bool hasInstr_ = false;
    double prismH_ = 1.5; bool poleIsPrism_ = false, poleTripod_ = false;   // tachimetria: wysokosc lustra i rodzaj tyczki w slocie pole_
    struct Back { zVEC3 pos; zCVob* marker = nullptr; zCVob* prism = nullptr; bool up = false; };   // nawiazanie: znak + lustro
    Back back_[5];
    bool ScanTargetsStale() const { for ( const ScanTgt& t : scanTgt_ ) if ( VobIsStale( t.vob ) ) return true; return false; }
    bool BacksightsStale() const { for ( const Back& b : back_ ) if ( VobIsStale( b.marker ) || VobIsStale( b.prism ) ) return true; return false; }
    void PlaceTachymeterPos( const zVEC3& p, int kind = 1 ) {       // kind: 1 tachimetr, 2 skaner laserowy
      SpawnInstrument( p, kind );
      if ( poleIsPrism_ ) FaceInstrument( pole_ );
      for ( Back& b : back_ ) if ( b.up ) FaceInstrument( b.prism );
    }
    // Instrument i jego obrotowe czesci: niwelator (podstawa + gorna czesc), tachimetr (statyw + alidada + luneta), skaner (korpus + glowica). Nowe stawianie kasuje zapamietany kierunek.
    void SpawnInstrument( const zVEC3& p, int kind ) {
      RemoveVobSafe( instr_ ); RemoveVobSafe( scanHead_ ); RemoveVobSafe( scope_ );
      instr_ = SpawnVob( kind == 2 ? LV_VIS_SCANNER : kind == 1 ? LV_VIS_TACHY : LV_VIS_INSTR, p ); instrGround_ = p; hasInstr_ = true; instrKind_ = kind;
      scanHead_ = SpawnVob( kind == 2 ? LV_VIS_SCANHEAD : kind == 1 ? LV_VIS_TACHYHEAD : LV_VIS_INSTRHEAD, p );
      if ( kind == 1 ) scope_ = SpawnVob( LV_VIS_TACHYSCOPE, p + zVEC3( 0, LV_INSTR_H, 0 ) );
      havePose_ = false;
    }
    void PlaceBacksightPos( int slot, const zVEC3& p, bool withMarker ) {
      Back& b = back_[slot - 1];
      RemoveVobSafe( b.marker ); RemoveVobSafe( b.prism );
      if ( withMarker ) b.marker = SpawnVob( LV_VIS_REPER, p );
      b.prism = SpawnVob( PrismModel( prismH_ ), p ); b.pos = p; b.up = true;
      FaceInstrument( b.prism );
      for ( int i = 0; i < 5; i++ ) if ( i != slot - 1 ) RemoveVobSafe( back_[i].prism );   // widoczne jest tylko ostatnio postawione lustro (znak i pozycja zostaja)
    }
    void FaceInstrument( zCVob* v ) {                  // tyczka z lustrem zwrocona tarcza ku tachimetrowi
      if ( !v || !hasInstr_ ) return;
      const zVEC3 p = v->GetPositionWorld();
      zVEC3 d( instrGround_[VX] - p[VX], 0, instrGround_[VZ] - p[VZ] );
      if ( d.Length() > 0.1f ) { d.Normalize(); v->SetHeadingAtWorld( d ); }
    }
    struct ScanTgt { zVEC3 pos; zCVob* vob = nullptr; bool up = false; };
    ScanTgt scanTgt_[6];                               // kule referencyjne skanera (srodek = pos + kScanTargetUpM nad znakiem)
    zCVob* scanHead_ = nullptr;                        // obrotowa czesc instrumentu: glowica skanera / alidada tachimetru / gorna czesc niwelatora
    zCVob* scope_ = nullptr;                           // luneta tachimetru (obrot w poziomie i w pionie)
    float poseAz_ = 0, posePitch_ = 0; bool havePose_ = false;   // zapamietany kierunek lunety [rad]
    struct Beam { double az, el, dist; };
    std::vector<Beam> beams_;                          // ostatnie wiazki skanu (rysowane liniami)
    zCVob* base_ = nullptr; zVEC3 baseGround_; bool hasBase_ = false;   // stacja bazowa RTK
    int instrKind_ = 0;                                // rodzaj instr_: 0 niwelator, 1 tachimetr, 2 skaner laserowy
    zCVob* pole_ = nullptr; zVEC3 poleGround_; bool hasPole_ = false; double clock_ = 0;   // GNSS: tyczka i zegar gry [s]
    zCVob *helmet_ = nullptr, *vest_ = nullptr; zCModelNodeInst *helmetNode_ = nullptr, *vestNode_ = nullptr;
    zVEC3 dressPos_[2] = { zVEC3( 1e9f, 0, 0 ), zVEC3( 1e9f, 0, 0 ) }, dressAt_[2] = { zVEC3( 0, 0, 0 ), zVEC3( 0, 0, 0 ) };
  };

  inline EngineWorld gWorld;                           // jedyny świat robót (używany przez kontroler i tryby widoku)
}
