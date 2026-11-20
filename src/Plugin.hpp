#include "Level.hpp"

namespace GOTHIC_NAMESPACE
{
    // diagnostyka: konstruktor statyczny wykonuje się przy załadowaniu DLL przez Union
    struct LvLoadProbe { LvLoadProbe() { LvTrace( "1. DLL zaladowana" ); } } lvLoadProbe;

    // Pętla klatki: zCWorld::Render (sprawdzone w grze). Tylko dla świata gry, nie dla podglądów przedmiotów.
    // LevelLoop() przed oryginałem, żeby zbliżenie lunety (FOV) obowiązywało w tej samej klatce.
    auto zCWorld_Render_Ivk = Union::CreateHook(ADDRESS_OF(&zCWorld::Render), &zCWorld::Render_Hooked);
    void zCWorld::Render_Hooked( zCCamera& cam )
    {
        if ( this == ogame->GetWorld() ) LevelLoop();
        (this->*zCWorld_Render_Ivk)( cam );
    }
}
