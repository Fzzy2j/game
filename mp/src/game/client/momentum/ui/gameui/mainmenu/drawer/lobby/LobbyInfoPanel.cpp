#include "cbase.h"

#include "LobbyInfoPanel.h"

#include <steam/isteammatchmaking.h>
#include <steam/isteamuser.h>



#include "fmtstr.h"
#include "ilocalize.h"
#include "controls/FileImage.h"

#include "util/mom_util.h"

#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"

#include "tier0/memdbgon.h"
#include "vgui/IInput.h"
#include "vgui_controls/Tooltip.h"

using namespace vgui;

static CSteamID s_LobbyID = k_steamIDNil;

LobbyInfoPanel::LobbyInfoPanel(Panel* pParent): BaseClass(pParent, "LobbyInfoPanel")
{
    SetProportional(true);

    m_bInLobby = false;

    m_pMainStatus = new Label(this, "MainStatusLabel", "#MOM_Drawer_Lobby_Searching");
    m_pSubStatus = new Label(this, "SubStatusLabel", "#MOM_Drawer_Lobby_Searching_Count");

    m_pLobbySettingsButton = new Button(this, "LobbySettingsButton", "Set", this, "LobbySettings");
    m_pInviteUserButton = new Button(this, "InviteUserButton", "Inv", this, "InviteUser");

    m_pLobbyToggleButton = new Button(this, "LobbyToggleButton", "#GameUI2_HostLobby", this, "LobbyToggle");

    m_pLobbyType = new ImagePanel(this, "LobbyType");
    m_pLobbyType->InstallMouseHandler(this);
    m_pLobbyType->SetVisible(false);
    m_pLobbyType->SetShouldScaleImage(true);

    LoadControlSettings("resource/ui/mainmenu/LobbyInfoPanel.res");

    m_pLobbyTypePublic = new FileImage("materials/vgui/icon/lobby_panel/lobby_public.png");
    m_pLobbyTypeFriends = new FileImage("materials/vgui/icon/lobby_panel/lobby_friends.png");
    m_pLobbyTypePrivate = new FileImage("materials/vgui/icon/lobby_panel/lobby_private.png");
}

LobbyInfoPanel::~LobbyInfoPanel()
{
    delete m_pLobbyTypePublic;
    delete m_pLobbyTypeFriends;
    delete m_pLobbyTypePrivate;
}

void LobbyInfoPanel::OnLobbyEnter(LobbyEnter_t *pParam)
{
    m_bInLobby = true;

    s_LobbyID = CSteamID(pParam->m_ulSteamIDLobby);

    LobbyEnterSuccess();
}

void LobbyInfoPanel::OnLobbyDataUpdate(LobbyDataUpdate_t *pParam)
{
    if (!m_bInLobby)
        return;

    if (pParam->m_ulSteamIDLobby != s_LobbyID.ConvertToUint64())
        return;

    if (pParam->m_ulSteamIDMember == pParam->m_ulSteamIDLobby)
    {
        UpdateLobbyMemberCount();
        UpdateLobbyName();
        SetLobbyTypeImage();
    }
}

void LobbyInfoPanel::OnLobbyChatUpdate(LobbyChatUpdate_t* pParam)
{
    if (pParam->m_rgfChatMemberStateChange & ( k_EChatMemberStateChangeEntered | k_EChatMemberStateChangeLeft | k_EChatMemberStateChangeDisconnected ))
    {
        UpdateLobbyMemberCount();
    }
}

void LobbyInfoPanel::OnLobbyLeave()
{
    m_bInLobby = false;

    s_LobbyID = k_steamIDNil;

    m_pLobbyToggleButton->SetText("#GameUI2_HostLobby");
    m_pInviteUserButton->SetVisible(false);
    m_pLobbyType->SetVisible(false);

    m_pSubStatus->SetText("");
    m_pMainStatus->SetText("#MOM_Drawer_Lobby_Searching");
}

void LobbyInfoPanel::OnCommand(const char* command)
{
    if (FStrEq(command, "LobbyToggle"))
    {
        if (m_bInLobby)
        {
            MomUtil::DispatchConCommand("mom_lobby_leave");
        }
        else
        {
            MomUtil::DispatchConCommand("mom_lobby_create");
        }
    }
    else if (FStrEq(command, "InviteUser"))
    {
        MomUtil::DispatchConCommand("mom_lobby_invite");
    }
    else if (FStrEq(command, "LobbySettings"))
    {
        Warning("TODO: Open a settings popup!!\n");
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}

void LobbyInfoPanel::OnMousePressed(vgui::MouseCode code)
{
    if (code == MOUSE_LEFT && input()->GetMouseOver() == m_pLobbyType->GetVPanel())
    {
        SetLobbyType(); // Cycle and update the lobby type
    }
}

void LobbyInfoPanel::OnReloadControls()
{
    BaseClass::OnReloadControls();

    if (s_LobbyID.IsValid())
    {
        LobbyEnterSuccess();
    }
    else
    {
        OnLobbyLeave();
    }
}

void LobbyInfoPanel::LobbyEnterSuccess()
{
    m_pLobbyToggleButton->SetText("#GameUI2_LeaveLobby");
    m_pInviteUserButton->SetVisible(true);

    UpdateLobbyMemberCount();
    UpdateLobbyName();
    SetLobbyTypeImage();
}

void LobbyInfoPanel::UpdateLobbyMemberCount() const
{
    if (s_LobbyID.IsValid())
    {
        const auto current = SteamMatchmaking()->GetNumLobbyMembers(s_LobbyID);
        const auto max = SteamMatchmaking()->GetLobbyMemberLimit(s_LobbyID);
        m_pSubStatus->SetText(CConstructLocalizedString(L"Players [localize]: %s1 / %s2", current, max));
    }
}

void LobbyInfoPanel::SetLobbyType() const
{
    CHECK_STEAM_API(SteamUser());
    CHECK_STEAM_API(SteamMatchmaking());
    // But only if they're the lobby owner
    const auto locID = SteamUser()->GetSteamID();
    if (s_LobbyID.IsValid() && locID == SteamMatchmaking()->GetLobbyOwner(s_LobbyID))
    {
        const auto pTypeStr = SteamMatchmaking()->GetLobbyData(s_LobbyID, LOBBY_DATA_TYPE);
        if (pTypeStr && Q_strlen(pTypeStr) == 1)
        {
            const auto nType = clamp<int>(Q_atoi(pTypeStr), k_ELobbyTypePrivate, k_ELobbyTypePublic);
            const auto newType = (nType + 1) % (k_ELobbyTypePublic + 1);
            engine->ClientCmd_Unrestricted(CFmtStr("mom_lobby_type %i", newType));
        }
    }
}

void LobbyInfoPanel::SetLobbyTypeImage() const
{
    const auto pTypeStr = SteamMatchmaking()->GetLobbyData(s_LobbyID, LOBBY_DATA_TYPE);
    if (pTypeStr && Q_strlen(pTypeStr) == 1)
    {
        const auto nType = clamp<int>(Q_atoi(pTypeStr), k_ELobbyTypePrivate, k_ELobbyTypePublic);

        static IImage *pImages[3] =
        {
            m_pLobbyTypePrivate,
            m_pLobbyTypeFriends,
            m_pLobbyTypePublic
        };
        static const char *pTTs[3] =
        {
            "#MOM_Lobby_Type_Private",
            "#MOM_Lobby_Type_FriendsOnly",
            "#MOM_Lobby_Type_Public"
        };

        m_pLobbyType->SetVisible(true);
        m_pLobbyType->SetImage(pImages[nType]);
        m_pLobbyType->GetTooltip()->SetEnabled(true);
        m_pLobbyType->GetTooltip()->SetText(pTTs[nType]);
    }
}

void LobbyInfoPanel::UpdateLobbyName()
{
    CHECK_STEAM_API(SteamMatchmaking());
    CHECK_STEAM_API(SteamFriends());

    if (!m_bInLobby)
        return;

    const auto ownerID = SteamMatchmaking()->GetLobbyOwner(s_LobbyID);
    const auto pOwnerName = SteamFriends()->GetFriendPersonaName(ownerID);

    KeyValuesAD data("Data");
    data->SetString("name", pOwnerName);

    m_pMainStatus->SetText(CConstructLocalizedString(L"%name%'s Lobby", static_cast<KeyValues*>(data)));
}