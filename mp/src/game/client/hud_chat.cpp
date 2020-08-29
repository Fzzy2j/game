#include "cbase.h"

#include "hud_chat.h"

#include "hud_element_helper.h"
#include "iclientmode.h"
#include "chat/ChatPanel.h"
#include "run/mom_run_safeguards.h"

#include "vgui/ISurface.h"

#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT(CHudChat);

using namespace vgui;

static CHudChat *g_pHudChat = nullptr;

CON_COMMAND(chat_open, "Opens the HUD chat window.\n")
{
    if (!g_pHudChat)
        return;

    g_pHudChat->StartMessageMode();
}

CHudChat::CHudChat(const char *pElementName) : CHudElement(pElementName), BaseClass(g_pClientMode->GetViewport(), "HudChat")
{
    g_pHudChat = this;

    SetHiddenBits(HIDEHUD_CHAT);

    surface()->CreatePopup(GetVPanel(), false, false, false, false, false);

    SetMouseInputEnabled(false);
    SetKeyBoardInputEnabled(false);
}

void CHudChat::StartMessageMode()
{
    // avoid softlock of starting message mode when hud/viewport isn't visible
    if (!g_pClientMode->GetViewport()->IsVisible())
        return;

    if (g_pRunSafeguards->IsSafeguarded(RUN_SAFEGUARD_CHAT_OPEN))
        return;

    SetMouseInputEnabled(true);
    SetKeyBoardInputEnabled(true);

    g_pChatPanel->SetParent(this);
    g_pChatPanel->StartMessageMode(MESSAGE_MODE_HUD);

    InvalidateLayout(true);
}

void CHudChat::SetVisible(bool state)
{
    BaseClass::SetVisible(state);

    if (!state)
    {
        g_pChatPanel->SetParent(nullptr);
    }
}

void CHudChat::OnThink()
{
    BaseClass::OnThink();

    if (g_pChatPanel->GetParent() != this)
    {
        g_pChatPanel->StopMessageMode(0.0f);
        g_pChatPanel->SetParent(this);
        InvalidateLayout(true);
        g_pChatPanel->InvalidateLayout();
    }
}

void CHudChat::PerformLayout()
{
    BaseClass::PerformLayout();

    int wide, tall;
    GetSize(wide, tall);

    g_pChatPanel->SetBounds(0, 0, wide, tall);
}

void CHudChat::OnStopMessageMode()
{
    SetMouseInputEnabled(false);
    SetKeyBoardInputEnabled(false);
}