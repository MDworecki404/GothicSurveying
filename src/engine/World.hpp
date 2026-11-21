// Adapter IWorld dla silnika: markery (reper/żabka), dwie łaty (role wstecz/wprzód), niwelator. Bez strażnika include.
// Wysokość punktu = wysokość stopy łaty (szczyt znaku/żabki), więc wszystkie odczyty liczą się względem tej samej płaszczyzny.
#include <map>

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
      DropVobRef( rod_[0] ); DropVobRef( rod_[1] ); DropVobRef( instr_ );
      DropVobRef( helmet_ ); DropVobRef( vest_ ); helmetNode_ = vestNode_ = nullptr;
      pts_.clear(); rodPoint_[0] = rodPoint_[1] = -1; hasInstr_ = false;
    }
    bool Stale() const {                                 // któryś z naszych vobów nie należy już do bieżącego świata
      for ( const auto& kv : pts_ ) if ( VobIsStale( kv.second.marker ) ) return true;
      return VobIsStale( rod_[0] ) || VobIsStale( rod_[1] ) || VobIsStale( instr_ ) || VobIsStale( helmet_ ) || VobIsStale( vest_ );
    }
    void ClearAll() {
      for ( auto& kv : pts_ ) RemoveVobSafe( kv.second.marker );
      RemoveVobSafe( rod_[0] ); RemoveVobSafe( rod_[1] ); RemoveVobSafe( instr_ );
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
    zCVob *helmet_ = nullptr, *vest_ = nullptr; zCModelNodeInst *helmetNode_ = nullptr, *vestNode_ = nullptr;
    zVEC3 dressPos_[2] = { zVEC3( 1e9f, 0, 0 ), zVEC3( 1e9f, 0, 0 ) }, dressAt_[2] = { zVEC3( 0, 0, 0 ), zVEC3( 0, 0, 0 ) };
  };
}
