//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Draws the normal TF2 or HL2 HUD.
//
//=============================================================================
#include "cbase.h"
#include "clientmode_mom_normal.h"
#include "momentum/mom_shareddefs.h"
#include "vgui_int.h"
#include "hud.h"
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include "ClientTimesDisplay.h"
#include "momSpectatorGUI.h"
#include <vgui_controls/AnimationController.h>
#include "iinput.h"
#include "ienginevgui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//extern bool g_bRollingCredits; MOM_TODO: reinstate this boolean!!

ConVar fov_desired("fov_desired", "90", FCVAR_ARCHIVE | FCVAR_USERINFO, "Sets the base field-of-view.\n", true, 90.0, true, 179.0);

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
vgui::HScheme g_hVGuiCombineScheme = 0;

// Instance the singleton and expose the interface to it.
IClientMode *GetClientModeNormal()
{
    static ClientModeMOMNormal g_ClientModeNormal;
    return &g_ClientModeNormal;
}

//-----------------------------------------------------------------------------
// Purpose: this is the viewport that contains all the hud elements
//-----------------------------------------------------------------------------
class CHudViewport : public CBaseViewport
{
private:
    DECLARE_CLASS_SIMPLE(CHudViewport, CBaseViewport);

protected:
    virtual void ApplySchemeSettings(vgui::IScheme *pScheme)
    {
        BaseClass::ApplySchemeSettings(pScheme);

        gHUD.InitColors(pScheme);

        SetPaintBackgroundEnabled(false);
    }

    IViewPortPanel *CreatePanelByName(const char *pzName)
    {
        IViewPortPanel *panel = BaseClass::CreatePanelByName(pzName);
        if (!panel)
        {
            if (!Q_strcmp(PANEL_TIMES, pzName))
            {
                panel = new CClientTimesDisplay(this);
            }
            else if (!Q_strcmp(PANEL_SPECMENU, pzName))
            {
                panel = new CMOMSpectatorMenu(this);
            }
            else if (!Q_strcmp(PANEL_SPECGUI, pzName))
            {
                panel = new CMOMSpectatorGUI(this);
            }
        }


        return panel;
    }

    virtual void CreateDefaultPanels(void)
    {
        AddNewPanel(CreatePanelByName(PANEL_TIMES), "PANEL_TIMES");

        BaseClass::CreateDefaultPanels();// MOM_TODO: do we want the other panels?
    };

};


//-----------------------------------------------------------------------------
// ClientModeHLNormal implementation
//-----------------------------------------------------------------------------
ClientModeMOMNormal::ClientModeMOMNormal()
{
    m_pHudMenuStatic = NULL;
    m_pViewport = new CHudViewport();
    m_pViewport->Start(gameuifuncs, gameeventmanager);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeMOMNormal::~ClientModeMOMNormal()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeMOMNormal::Init()
{
    BaseClass::Init();

    m_pHudMenuStatic = static_cast<CHudMenuStatic*>(GET_HUDELEMENT(CHudMenuStatic));

    // Load up the combine control panel scheme
    g_hVGuiCombineScheme = vgui::scheme()->LoadSchemeFromFileEx(enginevgui->GetPanel(PANEL_CLIENTDLL), IsXbox() ? "resource/ClientScheme.res" : "resource/CombinePanelScheme.res", "CombineScheme");
    if (!g_hVGuiCombineScheme)
    {
        Warning("Couldn't load combine panel scheme!\n");
    }
}

bool ClientModeMOMNormal::ShouldDrawCrosshair(void)
{
    return true;//MOM_TODO: reinstate the g_bRollingCredits when hud_credits is copied over.
    //return (g_bRollingCredits == false);
}

int ClientModeMOMNormal::HudElementKeyInput(int down, ButtonCode_t keynum, const char *pszCurrentBinding)
{
    if (m_pHudMenuStatic && m_pHudMenuStatic->IsMenuDisplayed())
    {
        if (down >= 1 && keynum >= KEY_0 && keynum <= KEY_9)
        {
            m_pHudMenuStatic->SelectMenuItem(keynum - KEY_0);
            return 0;//The hud menu static swallowed the key input
        }
    }
        
    return BaseClass::HudElementKeyInput(down, keynum, pszCurrentBinding);
}
int ClientModeMOMNormal::HandleSpectatorKeyInput(int down, ButtonCode_t keynum, const char *pszCurrentBinding)
{
    // MOM_TODO: re-enable this in beta when we add movie-style controls to the spectator menu!
    /*
    // we are in spectator mode, open spectator menu
    if (down && pszCurrentBinding && Q_strcmp(pszCurrentBinding, "+duck") == 0)
    {
        m_pViewport->ShowPanel(PANEL_SPECMENU, true);
        return 0; // we handled it, don't handle twice or send to server
    }
    */
    if (down && pszCurrentBinding && Q_strcmp(pszCurrentBinding, "+attack") == 0)
    {
        engine->ClientCmd("spec_next");
        return 0;
    }
    else if (down && pszCurrentBinding && Q_strcmp(pszCurrentBinding, "+attack2") == 0)
    {
        engine->ClientCmd("spec_prev");
        return 0;
    }
    else if (down && pszCurrentBinding && Q_strcmp(pszCurrentBinding, "+jump") == 0)
    {
        engine->ClientCmd("spec_mode");
        return 0;
    }
    
    return 1;
}
