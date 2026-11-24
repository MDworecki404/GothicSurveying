#include "Keys.hpp"
#include "job/Jobs.hpp"
#include "calc/Docx.hpp"
#include "calc/TexDecode.hpp"

// Kolejność ma znaczenie: moduły silnika nie includują siebie nawzajem (bez strażników include).
#include "engine/Panels.hpp"
#include "engine/Placement.hpp"
#include "engine/ControlNet.hpp"
#include "engine/Scope.hpp"
#include "engine/Menu.hpp"
#include "engine/World.hpp"
#include "engine/PoleFine.hpp"
#include "engine/TachyView.hpp"
#include "engine/ControlLabel.hpp"
#include "engine/MapView.hpp"
#include "engine/LevelView.hpp"
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
    // Zapis gry w trakcie roboty: stan roboty trafia do GothicSurveying/stan_slot<N>.txt; wczytanie zapisu wznawia robote (patrz ControllerTick).
    auto oCGame_WriteSavegame_Ivk = Union::CreateHook(ADDRESS_OF(&oCGame::WriteSavegame), &oCGame::WriteSavegame_Hooked);
    void oCGame::WriteSavegame_Hooked( int slot, int flag )
    {
        (this->*oCGame_WriteSavegame_Ivk)( slot, flag );
        CtrlSaveJobState( slot );
    }
    auto oCGame_LoadSavegame_Ivk = Union::CreateHook(ADDRESS_OF(&oCGame::LoadSavegame), &oCGame::LoadSavegame_Hooked);
    void oCGame::LoadSavegame_Hooked( int slot, int flag )
    {
        gPendingLoadSlot = slot;
        (this->*oCGame_LoadSavegame_Ivk)( slot, flag );
    }
    auto zCWorld_Render_Ivk = Union::CreateHook(ADDRESS_OF(&zCWorld::Render), &zCWorld::Render_Hooked);
    void zCWorld::Render_Hooked( zCCamera& cam )
    {
        if ( this == ogame->GetWorld() ) ControllerTick();
        (this->*zCWorld_Render_Ivk)( cam );
    }
}
