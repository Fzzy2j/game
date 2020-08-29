#include "cbase.h"

#include "MenuDrawer.h"

#include "lobby/DrawerPanel_Lobby.h"
#include "profile/DrawerPanel_Profile.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/PropertySheet.h"

#include "tier0/memdbgon.h"

using namespace vgui;

MenuDrawerPanel::MenuDrawerPanel(Panel *pParent) : BaseClass(pParent, "MenuDrawer")
{
    SetProportional(true);

    m_bDrawerOpen = false;

    const auto hScheme = scheme()->LoadSchemeFromFile("resource/MenuDrawerScheme.res", "MenuDrawerScheme");
    if (hScheme)
    {
        SetScheme(hScheme);
    }

    m_pDrawerHandleButton = new Button(this, "DrawerHandleButton", "<", this, "DrawerToggle");

    m_pDrawerContent = new PropertySheet(this, "DrawerContent");
    m_pDrawerContent->AddActionSignalTarget(this);

    LoadControlSettings("resource/ui/mainmenu/MenuDrawer.res");

    m_pDrawerContent->InvalidateLayout(true, true);

    m_pProfileDrawerPanel = new DrawerPanel_Profile(m_pDrawerContent);
    m_pLobbyDrawerPanel = new DrawerPanel_Lobby(m_pDrawerContent);

    m_pDrawerContent->AddPage(m_pProfileDrawerPanel, "#MOM_Drawer_Profile");
    m_pDrawerContent->AddPage(m_pLobbyDrawerPanel, "#MOM_Drawer_Lobby");
    m_iActivePage = DRAWER_TAB_USER;
}

void MenuDrawerPanel::PerformLayout()
{
    BaseClass::PerformLayout();

    m_pDrawerContent->SetVisible(m_bDrawerOpen);

    if (m_bDrawerOpen)
    {
        SetPos(ScreenWidth() - GetWide(), 0);
    }
    else
    {
        SetPos(ScreenWidth() - m_pDrawerHandleButton->GetWide(), 0);
    }
}

void MenuDrawerPanel::ApplySchemeSettings(IScheme* pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
}

void MenuDrawerPanel::OnCommand(const char* command)
{
    if (FStrEq(command, "DrawerToggle"))
    {
        ToggleDrawer();
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}

void MenuDrawerPanel::OnPageChanged()
{
    m_iActivePage = m_pDrawerContent->GetActivePageNum();
}

void MenuDrawerPanel::ToggleDrawer()
{
    m_bDrawerOpen = !m_bDrawerOpen;

    m_pDrawerHandleButton->SetText(m_bDrawerOpen ? ">" : "<");

    InvalidateLayout();
}

void MenuDrawerPanel::OnLobbyEnter()
{
    OpenDrawerTo(DRAWER_TAB_LOBBY);
}

void MenuDrawerPanel::OnSpecStart()
{
    
}

void MenuDrawerPanel::OnSpecStop()
{
    
}


void MenuDrawerPanel::OnSiteAuth()
{

}

void MenuDrawerPanel::OpenDrawerTo(DrawerTab_t tab)
{
    if (!m_bDrawerOpen)
        ToggleDrawer();

    if (m_pDrawerContent->GetActivePageNum() != tab)
        m_pDrawerContent->SetActivePage(m_pDrawerContent->GetPage(tab));
}

int MenuDrawerPanel::GetDrawerButtonWidth() const
{
    return m_pDrawerHandleButton ? m_pDrawerHandleButton->GetWide() : 0;
}

void MenuDrawerPanel::OnKeyCodeTyped(KeyCode code)
{
    if (code == KEY_ESCAPE)
    {
        if (m_bDrawerOpen && !engine->IsInGame())
        {
            ToggleDrawer();
        }
    }
    else
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}

void MenuDrawerPanel::OnLobbyLeave()
{
    m_pLobbyDrawerPanel->OnLobbyLeave();
}