#include "Keys.hpp"
#include "job/Jobs.hpp"
#include "calc/Docx.hpp"

// Kolejność ma znaczenie: moduły silnika nie includują siebie nawzajem (bez strażników include).
#include "engine/Panels.hpp"
#include "engine/Placement.hpp"
#include "engine/Scope.hpp"
#include "engine/Menu.hpp"
#include "engine/World.hpp"
#include "engine/PoleFine.hpp"
#include "engine/Export.hpp"
#include "engine/FieldBookView.hpp"
#include "engine/FieldBookEdit.hpp"
#include "engine/Controller.hpp"

namespace GOTHIC_NAMESPACE
{
    // diagnostyka: konstruktor statyczny wykonuje się przy załadowaniu DLL przez Union
    struct LvLoadProbe { LvLoadProbe() { SvTrace( "1. DLL zaladowana" ); } } lvLoadProbe;

    // Pętla klatki: zCWorld::Render (sprawdzone w grze). Tylko dla świata gry, nie dla podglądów przedmiotów.
    // ControllerTick() przed oryginałem, żeby zbliżenie lunety (FOV) obowiązywało w tej samej klatce.
    auto zCWorld_Render_Ivk = Union::CreateHook(ADDRESS_OF(&zCWorld::Render), &zCWorld::Render_Hooked);
    void zCWorld::Render_Hooked( zCCamera& cam )
    {
        if ( this == ogame->GetWorld() ) ControllerTick();
        (this->*zCWorld_Render_Ivk)( cam );
    }
}
