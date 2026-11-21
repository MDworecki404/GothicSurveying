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
      if ( !rod_[r] ) rod_[r] = SpawnVob( LV_VIS_ROD, foot ); else rod_[r]->SetPositionWorld( foot );
      rodPoint_[r] = pointId;
    }
    void RemoveRod( RodRole role ) override {
      const int r = role == RodRole::Back ? 0 : 1;
      RemoveVobSafe( rod_[r] ); rodPoint_[r] = -1;
    }
    bool PlaceInstrument() override {
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      RemoveVobSafe( instr_ );
      instr_ = SpawnVob( LV_VIS_INSTR, p ); instrGround_ = p; hasInstr_ = true;
      return true;
    }
    void RemoveInstrument() override { RemoveVobSafe( instr_ ); hasInstr_ = false; }
    double PointHeightM( int pointId ) const override { return pts_.count( pointId ) ? Foot( pointId )[VY] / 100.0 : 0.0; }
    double SightDistCm( int pointId ) const override {
      auto it = pts_.find( pointId );
      if ( it == pts_.end() || !hasInstr_ ) return 0;
      double dx = it->second.pos[VX] - instrGround_[VX], dz = it->second.pos[VZ] - instrGround_[VZ];
      return sqrt( dx * dx + dz * dz );
    }
    // --- GNSS: tyczka z odbiornikiem, promienie do nieba, zegar gry ---
    bool PlacePole() override {
      zVEC3 p;
      if ( !FeetGround( p ) ) return false;
      RemoveVobSafe( pole_ );
      pole_ = SpawnVob( LV_VIS_POLE, p ); poleGround_ = p; hasPole_ = true; poleIsPrism_ = false;
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
    bool PlaceTachymeterAt( const ControlPoint& cp ) override {    // na znaku punktu osnowy: dokladnie na jego pozycji
      PlaceTachymeterPos( zVEC3( (float)cp.x, (float)cp.y, (float)cp.z ) );
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
    }
    void Say( const std::string& text ) override { HudMessage( text.c_str() ); }

    // --- tylko silnik ---
    bool HasInstrument() const { return hasInstr_; }
    zVEC3 InstrumentGround() const { return instrGround_; }
    zVEC3 PointFoot( int pointId ) const { return Foot( pointId ); }
    void SetInstrumentVisible( bool v ) { if ( instr_ ) instr_->showVisual = v ? 1 : 0; }
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
      DropVobRef( rod_[0] ); DropVobRef( rod_[1] ); DropVobRef( instr_ ); DropVobRef( pole_ ); hasPole_ = false; poleIsPrism_ = false;
      for ( Back& b : back_ ) { DropVobRef( b.marker ); DropVobRef( b.prism ); b.up = false; }
      DropVobRef( helmet_ ); DropVobRef( vest_ ); helmetNode_ = vestNode_ = nullptr;
      pts_.clear(); rodPoint_[0] = rodPoint_[1] = -1; hasInstr_ = false;
    }
    bool Stale() const {                                 // któryś z naszych vobów nie należy już do bieżącego świata
      for ( const auto& kv : pts_ ) if ( VobIsStale( kv.second.marker ) ) return true;
      return VobIsStale( rod_[0] ) || VobIsStale( rod_[1] ) || VobIsStale( instr_ ) || VobIsStale( helmet_ ) || VobIsStale( vest_ ) || VobIsStale( pole_ ) || BacksightsStale();
    }
    void ClearAll() {
      for ( auto& kv : pts_ ) RemoveVobSafe( kv.second.marker );
      RemoveVobSafe( rod_[0] ); RemoveVobSafe( rod_[1] ); RemoveVobSafe( instr_ ); RemoveVobSafe( pole_ ); hasPole_ = false; poleIsPrism_ = false;
      for ( int i = 1; i <= 5; i++ ) RemoveBacksight( i );
      DressOff();
      pts_.clear(); rodPoint_[0] = rodPoint_[1] = -1; hasInstr_ = false;
    }

   private:
    struct Pt { zVEC3 pos; zCVob* marker = nullptr; bool reper = false; };
    zVEC3 Foot( int pointId ) const {
      const Pt& pt = pts_.at( pointId );
      return pt.pos + zVEC3( 0, pt.reper ? LV_REPER_H : LV_PLATE_H, 0 );
    }
    std::map<int, Pt> pts_;
    zCVob* rod_[2] = { nullptr, nullptr };
    int rodPoint_[2] = { -1, -1 };
    zCVob* instr_ = nullptr; zVEC3 instrGround_; bool hasInstr_ = false;
    double prismH_ = 1.5; bool poleIsPrism_ = false;   // tachimetria: wysokosc lustra i rodzaj tyczki w slocie pole_
    struct Back { zVEC3 pos; zCVob* marker = nullptr; zCVob* prism = nullptr; bool up = false; };   // nawiazanie: znak + lustro
    Back back_[5];
    bool BacksightsStale() const { for ( const Back& b : back_ ) if ( VobIsStale( b.marker ) || VobIsStale( b.prism ) ) return true; return false; }
    void PlaceTachymeterPos( const zVEC3& p ) {
      RemoveVobSafe( instr_ );
      instr_ = SpawnVob( LV_VIS_TACHY, p ); instrGround_ = p; hasInstr_ = true;
      if ( poleIsPrism_ ) FaceInstrument( pole_ );
      for ( Back& b : back_ ) if ( b.up ) FaceInstrument( b.prism );
    }
    void PlaceBacksightPos( int slot, const zVEC3& p, bool withMarker ) {
      Back& b = back_[slot - 1];
      RemoveVobSafe( b.marker ); RemoveVobSafe( b.prism );
      if ( withMarker ) b.marker = SpawnVob( LV_VIS_REPER, p );
      b.prism = SpawnVob( PrismModel( prismH_ ), p ); b.pos = p; b.up = true;
      FaceInstrument( b.prism );
    }
    void FaceInstrument( zCVob* v ) {                  // tyczka z lustrem zwrocona tarcza ku tachimetrowi
      if ( !v || !hasInstr_ ) return;
      const zVEC3 p = v->GetPositionWorld();
      zVEC3 d( instrGround_[VX] - p[VX], 0, instrGround_[VZ] - p[VZ] );
      if ( d.Length() > 0.1f ) { d.Normalize(); v->SetHeadingAtWorld( d ); }
    }
    zCVob* pole_ = nullptr; zVEC3 poleGround_; bool hasPole_ = false; double clock_ = 0;   // GNSS: tyczka i zegar gry [s]
    zCVob *helmet_ = nullptr, *vest_ = nullptr; zCModelNodeInst *helmetNode_ = nullptr, *vestNode_ = nullptr;
    zVEC3 dressPos_[2] = { zVEC3( 1e9f, 0, 0 ), zVEC3( 1e9f, 0, 0 ) }, dressAt_[2] = { zVEC3( 0, 0, 0 ), zVEC3( 0, 0, 0 ) };
  };

  inline EngineWorld gWorld;                           // jedyny świat robót (używany przez kontroler i tryby widoku)
}
