// Etykieta najblizszego punktu osnowy: gdy bohater jest blisko znaku (do 12 m) i patrzy w jego strone, nad znakiem unosi sie panel "G07 - Cmentarz (7 m)".
// Dolaczane raz z Plugin.hpp po ControlNet.hpp i Panels.hpp, bez straznika include.
#include <cmath>
#include <string>

namespace GOTHIC_NAMESPACE {
  inline zCView*     gCtlLabel = nullptr;
  inline std::string gCtlLabelText;
  inline int         gCtlLabelW = 0;
  inline const int   LV_CTL_LABEL_H = 380;                     // wysokosc etykiety [jednostki ekranu]
  inline const double LV_CTL_LABEL_RANGE = 1200.0;             // zasieg [cm]

  inline void ControlLabelHide() { PanelClose( gCtlLabel ); gCtlLabelText.clear(); }

  inline void ControlLabelTick( bool paused ) {
    zCCamera* cam = zCCamera::activeCam;
    if ( paused || !player || !cam || gControl.empty() ) { ControlLabelHide(); return; }
    const zVEC3 hero = player->GetPositionWorld();
    const std::string key = ControlWorldKey( ControlWorldName() );
    const ControlPoint* best = nullptr; double bestD = LV_CTL_LABEL_RANGE;
    for ( const ControlPoint& c : gControl ) {
      if ( ControlWorldKey( c.world ) != key ) continue;
      const double d = hypot( c.x - hero[VX], c.z - hero[VZ] );
      if ( d < bestD ) { bestD = d; best = &c; }
    }
    if ( !best ) { ControlLabelHide(); return; }
    const zVEC3 top( (float)best->x, (float)best->y + 150.0f, (float)best->z );        // nad znakiem
    const zVEC3 v = cam->camMatrix * top;
    if ( v[2] <= 0 ) { ControlLabelHide(); return; }                                      // za plecami
    float fx = 0, fy = 0; cam->Project( &v, fx, fy );
    const int x = screen->anx( fx ), y = screen->any( fy );
    if ( x < 0 || x > 8192 || y < 0 || y > 8192 ) { ControlLabelHide(); return; }
    const std::string place = ControlPlaceName( *best );
    const std::string text = best->id + ( place.empty() ? "" : " - " + place ) + " (" + std::to_string( (int)( bestD / 100.0 + 0.5 ) ) + " m)";
    if ( text != gCtlLabelText || !gCtlLabel ) {
      PanelClose( gCtlLabel );
      gCtlLabelW = 70 * (int)text.size() + 400;
      gCtlLabel = PanelOpen( 0, 0, gCtlLabelW, LV_CTL_LABEL_H );
      gCtlLabel->PrintCXY( zSTRING( text.c_str() ) );
      gCtlLabelText = text;
    }
    gCtlLabel->SetPos( x - gCtlLabelW / 2, y - LV_CTL_LABEL_H );
  }
}
