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
    // Po zmianie świata: oddaje własne referencje bez dotykania świata (voby zwolnił już świat).
    void ForgetAll() {
      for ( auto& kv : pts_ ) DropVobRef( kv.second.marker );
      DropVobRef( rod_[0] ); DropVobRef( rod_[1] ); DropVobRef( instr_ );
      pts_.clear(); rodPoint_[0] = rodPoint_[1] = -1; hasInstr_ = false;
    }
    bool Stale() const {                                 // któryś z naszych vobów nie należy już do bieżącego świata
      for ( const auto& kv : pts_ ) if ( VobIsStale( kv.second.marker ) ) return true;
      return VobIsStale( rod_[0] ) || VobIsStale( rod_[1] ) || VobIsStale( instr_ );
    }
    void ClearAll() {
      for ( auto& kv : pts_ ) RemoveVobSafe( kv.second.marker );
      RemoveVobSafe( rod_[0] ); RemoveVobSafe( rod_[1] ); RemoveVobSafe( instr_ );
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
  };
}
