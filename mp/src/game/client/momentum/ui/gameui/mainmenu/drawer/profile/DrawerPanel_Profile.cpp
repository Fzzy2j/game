#include "cbase.h"

#include "DrawerPanel_Profile.h"

#include <steam/isteamuser.h>

#include "mom_shareddefs.h"

#include "vgui_controls/PropertySheet.h"
#include "controls/UserComponent.h"

#include "tier0/memdbgon.h"

using namespace vgui;

DrawerPanel_Profile::DrawerPanel_Profile(Panel *pParent) : BaseClass(pParent, "DrawerPanel_Profile")
{
    SetProportional(true);

    m_pUserComponent = new UserComponent(this);

    if (SteamUser())
    {
        m_pUserComponent->SetUser(SteamUser()->GetSteamID().ConvertToUint64());
    }

    m_pStatsAndActivitySheet = new PropertySheet(this, "StatsAndActivity");
    m_pStatsAndActivitySheet->AddPage(new Panel(m_pStatsAndActivitySheet), "#MOM_Drawer_Profile_Stats");
    m_pStatsAndActivitySheet->AddPage(new Panel(m_pStatsAndActivitySheet), "#MOM_Drawer_Profile_Activity");
}

void DrawerPanel_Profile::OnResetData()
{
    LoadControlSettings("resource/ui/mainmenu/DrawerPanel_Profile.res");
}