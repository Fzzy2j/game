#include "cbase.h"

#include "ChatPanel.h"
#include "ChatHistory.h"

#include <ctime>
#include <steam/isteamuser.h>

#include "ChatEntry.h"
#include "ChatLine.h"
#include "ChatFilterPanel.h"
#include "c_playerresource.h"
#include "vgui_controls/Label.h"

#include "hud_element_helper.h"
#include "hud_macros.h"
#include "hud_spectatorinfo.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "mom_shareddefs.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "voice_status.h"
#include "engine/IEngineSound.h"

#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include "tier0/memdbgon.h"

using namespace vgui;

#define MAX_CHAT_LENGTH 128

extern ConVar cl_chatfilters;

static MAKE_TOGGLE_CONVAR(cl_showtextmsg, "1", 0, "Enable/disable text messages printing on the screen.\n");
static MAKE_TOGGLE_CONVAR(mom_chat_timestamps_enable, "0", FCVAR_REPLICATED | FCVAR_ARCHIVE, "Toggles timestamps on chat messages. 0 = OFF, 1 = ON\n");
static MAKE_TOGGLE_CONVAR(cl_mute_all_comms, "1", FCVAR_ARCHIVE, "If 1, then all communications from a player will be blocked when that player is muted, including chat messages.\n");

static void __MsgFunc_SayText(bf_read &read);
static void __MsgFunc_SayText2(bf_read &read);
static void __MsgFunc_TextMsg(bf_read &read);

ChatPanel *g_pChatPanel = nullptr;

ChatPanel::ChatPanel() : BaseClass(nullptr, "ChatPanel")
{
    SetProportional(true);
    m_bIsVisible = m_bTyping = false;
    m_flHistoryFadeTime = gpGlobals->curtime;
    m_hChatFont = INVALID_FONT;

    m_pSpectatorInfo = nullptr;

    HScheme chatScheme = scheme()->LoadSchemeFromFileEx(enginevgui->GetPanel(PANEL_CLIENTDLL), "resource/ChatScheme.res", "ChatScheme");
    SetScheme(chatScheme);

    g_pVGuiLocalize->AddFile("resource/chat_%language%.txt");

    m_nMessageMode = 0;

    ivgui()->AddTickSignal(GetVPanel());

    m_pFiltersButton = new Button(this, "ChatFiltersButton", "Filters", this, "FiltersToggle");
    m_pFiltersButton->SetVisible(true);
    m_pFiltersButton->SetEnabled(true);
    m_pFiltersButton->SetMouseInputEnabled(true);
    m_pFiltersButton->SetKeyBoardInputEnabled(false);

    m_pChatHistory = new ChatHistory(this);
    m_pChatHistory->SetMaximumCharCount((MAX_CHAT_LENGTH - 1) * 100);
    m_pChatHistory->SetVisible(true);

    m_ChatLine = new ChatLine(this);
    m_ChatLine->SetVisible(false);

    m_pChatInput = new ChatEntry(this);
    m_pChatInput->SetVisible(false);
    m_pChatInput->SetMouseInputEnabled(true);
    m_pChatInput->SetKeyBoardInputEnabled(true);

    m_pFilterPanel = new ChatFilterPanel(this);

    m_iFilterFlags = cl_chatfilters.GetInt();

    m_pTypingMembers = new Label(this, "TypingMembers", "");

    LoadControlSettings("resource/ui/BaseChat.res");

    ListenForGameEvent("lobby_leave");
    ListenForGameEvent("lobby_update_msg");
    ListenForGameEvent("lobby_spec_update_msg");

    //@tuxxi: I tired to query this automatically but we can only query steamgroups we are members of.. rip.
    // So i'm just hard coding this here for now
    // https://partner.steamgames.com/doc/api/ISteamFriends#RequestClanOfficerList

    // MOM_TODO: query web API for officers / members/ other groups instead of this crap
    m_vMomentumOfficers.AddToTail(76561198018587940ull); // tuxxi
    m_vMomentumOfficers.AddToTail(76561197979963054ull); // gocnak
    m_vMomentumOfficers.AddToTail(76561198047369620ull); // rusty
    m_vMomentumOfficers.AddToTail(76561197982874432ull); // juxtapo

    HOOK_MESSAGE(SayText);
    HOOK_MESSAGE(SayText2);
    HOOK_MESSAGE(TextMsg);
}

ChatPanel::~ChatPanel()
{
    ivgui()->RemoveTickSignal(GetVPanel());
}

void ChatPanel::Init()
{
    if (g_pChatPanel)
        return;

    g_pChatPanel = new ChatPanel;
}


void ChatPanel::ApplySchemeSettings(IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    m_pChatHistory->SetVerticalScrollbar(false);

    m_cDefaultTextColor = pScheme->GetColor("OffWhite", COLOR_WHITE);
}

void ChatPanel::OnKeyCodeReleased(KeyCode code)
{
    if (code == KEY_ENTER || code == KEY_PAD_ENTER || code == KEY_ESCAPE)
    {
        if (code != KEY_ESCAPE)
        {
            OnChatEntrySend();
        }

        if (m_nMessageMode != MESSAGE_MODE_MENU)
        {
            StopMessageMode();
        }
    }
    else
    {
        BaseClass::OnKeyCodeReleased(code);
    }
}

void ChatPanel::OnCommand(const char *command)
{
    if (FStrEq(command, "FiltersToggle"))
    {
        m_pChatInput->RequestFocus();

        if (m_pFilterPanel->IsVisible())
        {
            m_pFilterPanel->SetVisible(false);
        }
        else
        {
            m_pFilterPanel->SetVisible(true);
            m_pFilterPanel->MakePopup();
            m_pFilterPanel->SetMouseInputEnabled(true);
        }
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}

void ChatPanel::PerformLayout()
{
    BaseClass::PerformLayout();

    int pWide, pTall;
    GetSize(pWide, pTall);

    const auto iFilterTall = m_pFiltersButton->GetTall();
    const auto iInputTall = m_pChatInput->GetTall();

    m_pChatHistory->SetBounds(0, 0, pWide, pTall - iFilterTall - iInputTall - 4);
    m_pChatInput->SetWide(pWide);
}

void ChatPanel::OnTick()
{
    FadeChatHistory();
}

void ChatPanel::OnThink()
{
    BaseClass::OnThink();

    if (m_LobbyID.IsValid() && m_nMessageMode != MESSAGE_MODE_NONE && m_pChatInput)
    {
        const bool isSomethingTyped = m_pChatInput->GetTextLength() > 0;
        if (isSomethingTyped != m_bTyping)
        {
            m_bTyping = isSomethingTyped;
            SteamMatchmaking()->SetLobbyMemberData(m_LobbyID, LOBBY_DATA_TYPING, m_bTyping ? "y" : nullptr);
        }
    }

    const int count = m_vTypingMembers.Count();
    if (m_bIsVisible)
    {
        if (count > 0)
        {
            CUtlString typingText;
            if (count <= 3)
            {
                FOR_EACH_VEC(m_vTypingMembers, i)
                {
                    typingText.Append(SteamFriends()->GetFriendPersonaName(CSteamID(m_vTypingMembers[i])));
                    typingText.Append(i < count - 1 ? ", " : " ");
                }
                typingText.Append("typing...");
            }
            else
            {
                typingText.Format("%d people are typing...", count);
            }

            m_pTypingMembers->SetText(typingText.Get());
        }
        else
        {
            m_pTypingMembers->SetText("");
        }
    }
}

void ChatPanel::Clear()
{
    StopMessageMode();
}

void ChatPanel::GetTimestamp(char *pBuffer, int maxLen)
{
    if (!mom_chat_timestamps_enable.GetBool())
    {
        Q_snprintf(pBuffer, maxLen, "");
        return;
    }

    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    Q_snprintf(pBuffer, maxLen, "%c%s[%02d:%02d] ", COLOR_HEXCODE, "CCCCCC", tm->tm_hour, tm->tm_min);
}

void ChatPanel::OnLobbyMessage(LobbyChatMsg_t *pParam)
{
    const CSteamID msgSender = CSteamID(pParam->m_ulSteamIDUser);
    /*
    if (msgSender == SteamUser()->GetSteamID().ConvertToUint64())
    {
        //DevLog("Got our own message! Just ignoring it...\n");
        return;
    }
    */
    const bool isMomentumTeam = m_vMomentumOfficers.IsValidIndex(m_vMomentumOfficers.Find(pParam->m_ulSteamIDUser));
    char personName[MAX_PLAYER_NAME_LENGTH];
    Q_strncpy(personName, SteamFriends()->GetFriendPersonaName(msgSender), MAX_PLAYER_NAME_LENGTH);

    const char *spectatingText = SteamMatchmaking()->GetLobbyMemberData(m_LobbyID, msgSender, LOBBY_DATA_IS_SPEC);
    const bool isSpectating = spectatingText != nullptr && Q_strlen(spectatingText) > 0;

    char timestamp[16];
    GetTimestamp(timestamp, 16);

    char message[4096];
    // MOM_TODO: This won't be just text in the future, if we capitalize on being able to send binary data. Wrap this is
    // something and parse it
    SteamMatchmaking()->GetLobbyChatEntry(CSteamID(pParam->m_ulSteamIDLobby), pParam->m_iChatID,
        nullptr, message, 4096, nullptr);
    SetCustomColor(COLOR_RED);
    ChatPrintf(1, CHAT_FILTER_NONE, "%s%c%s%s%c: %s",
        timestamp,
        isMomentumTeam ? COLOR_CUSTOM : COLOR_PLAYERNAME,
        isSpectating ? "*SPEC* " : "",
        personName,
        COLOR_NORMAL,
        message);
}

void ChatPanel::OnLobbyDataUpdate(LobbyDataUpdate_t *pParam)
{
    if (pParam->m_ulSteamIDLobby != m_LobbyID.ConvertToUint64())
        return;

    // If something other than the lobby and local player...
    if (pParam->m_bSuccess &&
        pParam->m_ulSteamIDLobby != pParam->m_ulSteamIDMember &&
        pParam->m_ulSteamIDMember != SteamUser()->GetSteamID().ConvertToUint64())
    {
        // Typing Status
        const char *typingText = SteamMatchmaking()->GetLobbyMemberData(m_LobbyID, pParam->m_ulSteamIDMember, LOBBY_DATA_TYPING);
        const bool isTyping = typingText != nullptr && Q_strlen(typingText) == 1;
        const int typingIndex = m_vTypingMembers.Find(pParam->m_ulSteamIDMember);
        const bool isValidIndex = m_vTypingMembers.IsValidIndex(typingIndex);
        if (isTyping)
        {
            if (!isValidIndex)
                m_vTypingMembers.AddToTail(pParam->m_ulSteamIDMember);
        }
        else if (isValidIndex)
        {
            m_vTypingMembers.FastRemove(typingIndex);
        }
    }
}

void ChatPanel::OnLobbyEnter(LobbyEnter_t *pParam)
{
    if (pParam->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess)
        m_LobbyID = pParam->m_ulSteamIDLobby;
}

void ChatPanel::SpectatorUpdate(const CSteamID& personID, const CSteamID& target)
{
    if (!m_pSpectatorInfo)
        m_pSpectatorInfo = GET_HUDELEMENT(CHudSpectatorInfo);

    if (m_pSpectatorInfo)
        m_pSpectatorInfo->SpectatorUpdate(personID, target);
}

void ChatPanel::Reset()
{
    Clear();
    m_flHistoryFadeTime = gpGlobals->curtime;
}

void ChatPanel::Printf(int iFilter, const char *fmt, ...)
{
    va_list marker;
    char msg[4096];

    va_start(marker, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, marker);
    va_end(marker);

    char timestamp[16];
    GetTimestamp(timestamp, 16);

    ChatPrintf(0, iFilter, "%s%c%s", timestamp, COLOR_NORMAL, msg);
}

void ChatPanel::StartMessageMode(int iMessageModeType)
{
    m_nMessageMode = iMessageModeType;

    m_pChatInput->SetText(L"");

    m_pChatHistory->SetVerticalScrollbar(true);
    m_pChatHistory->ResetAllFades(true);
    m_pChatHistory->SetPaintBorderEnabled(true);
    m_pChatHistory->SetVisible(true);

    SETUP_PANEL(this);
    m_pChatInput->SetVisible(true);
    surface()->CalculateMouseVisible();
    m_pChatInput->SetPaintBorderEnabled(true);

    // Place the mouse cursor near the text so people notice it.
    if (iMessageModeType == MESSAGE_MODE_HUD)
    {
        int x, y, w, h;
        m_pChatHistory->GetBounds(x, y, w, h);
        x += w / 2;
        y += h / 2;
        m_pChatHistory->LocalToScreen(x, y);
        input()->SetCursorPos(x, y);
        m_pChatInput->RequestFocus();
    }

    m_flHistoryFadeTime = gpGlobals->curtime + CHAT_HISTORY_FADE_TIME;

    m_pFilterPanel->SetVisible(false);

    engine->ClientCmd_Unrestricted("gameui_preventescapetoshow\n");

    m_bIsVisible = true;
    m_pTypingMembers->SetVisible(true);
}

void ChatPanel::StopMessageMode(float fFadeoutOverride /*= CHAT_HISTORY_FADE_TIME*/)
{
    engine->ClientCmd_Unrestricted("gameui_allowescapetoshow\n");

    m_pChatHistory->SetPaintBorderEnabled(false);
    m_pChatHistory->GotoTextEnd();
    m_pChatHistory->SetVerticalScrollbar(false);
    m_pChatHistory->ResetAllFades(false, true, fFadeoutOverride);
    m_pChatHistory->SelectNoText();

    m_pChatInput->SetText(L"");

    m_pFilterPanel->SetVisible(false);

    m_flHistoryFadeTime = gpGlobals->curtime + fFadeoutOverride;

    m_nMessageMode = MESSAGE_MODE_NONE;

    if (m_LobbyID.IsValid())
        SteamMatchmaking()->SetLobbyMemberData(m_LobbyID, LOBBY_DATA_TYPING, nullptr);

    // Can't be typing if we close the chat
    m_bIsVisible = m_bTyping = false;
    m_pTypingMembers->SetVisible(false);

    PostMessage(GetVParent(), new KeyValues("StopMessageMode"));
}

void ChatPanel::OnChatEntrySend()
{
    Send();
}

void ChatPanel::OnChatEntryStopMessageMode()
{
    StopMessageMode();
}

void ChatPanel::SetFilterFlag(int iFilter)
{
    m_iFilterFlags = iFilter;

    cl_chatfilters.SetValue(m_iFilterFlags);
}

void ChatPanel::Send()
{
    char ansi[MAX_CHAT_LENGTH];
    m_pChatInput->GetText(ansi, sizeof(ansi));

    int len = Q_strlen(ansi);

    // remove the \n
    if (len > 0 && ansi[len - 1] == '\n')
    {
        ansi[len - 1] = '\0';
    }

    if (len > 0)
    {
        // Let the game rules at it
        if (GameRules())
        {
            GameRules()->ModifySentChat(ansi, ARRAYSIZE(ansi));
        }

        // MOM_TODO FORWARD ME TO A VALIDATOR / SENDER ON CLIENT
        char szbuf[256];
        Q_snprintf(szbuf, sizeof(szbuf), "say \"%s\"", ansi);

        engine->ClientCmd_Unrestricted(szbuf);
    }

    m_pChatInput->SetText(L"");
}

int ChatPanel::ComputeBreakChar(int width, const char *text, int textlen)
{
    HFont font = m_ChatLine->GetFont();

    int currentlen = 0;
    int lastbreak = textlen;
    for (int i = 0; i < textlen; i++)
    {
        char ch = text[i];

        if (ch <= 32)
        {
            lastbreak = i;
        }

        wchar_t wch[2];

        g_pVGuiLocalize->ConvertANSIToUnicode(&ch, wch, sizeof(wch));

        int a, b, c;

        surface()->GetCharABCwide(font, wch[0], a, b, c);
        currentlen += a + b + c;

        if (currentlen >= width)
        {
            // If we haven't found a whitespace char to break on before getting
            //  to the end, but it's still too long, break on the character just before
            //  this one
            if (lastbreak == textlen)
            {
                lastbreak = MAX(0, i - 1);
            }
            break;
        }
    }

    if (currentlen >= width)
    {
        return lastbreak;
    }
    return textlen;
}

void ChatPanel::ChatPrintf(int iPlayerIndex, int iFilter, const char *fmt, ...)
{
    va_list marker;
    char msg[4096];

    va_start(marker, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, marker);
    va_end(marker);

    // Strip any trailing '\n'
    const size_t pMsgSize = strlen(msg);
    if (pMsgSize > 0 && msg[pMsgSize - 1] == '\n')
    {
        msg[pMsgSize - 1] = 0;
    }

    // Strip leading \n characters ( or notify/color signifiers ) for empty string check
    char *pmsg = msg;
    while (*pmsg && (*pmsg == '\n' || (*pmsg > 0 && *pmsg < COLOR_MAX)))
    {
        pmsg++;
    }

    if (!*pmsg)
        return;

    // Now strip just newlines, since we want the color info for printing
    pmsg = msg;
    while (*pmsg && (*pmsg == '\n'))
    {
        pmsg++;
    }

    if (!*pmsg)
        return;

    if (iFilter != CHAT_FILTER_NONE)
    {
        if (!(iFilter & GetFilterFlags()))
            return;
    }

    // If a player is muted for voice, also mute them for text because jerks gonna jerk.
    if (cl_mute_all_comms.GetBool() && iPlayerIndex != 0)
    {
        if (GetClientVoiceMgr() && GetClientVoiceMgr()->IsPlayerBlocked(iPlayerIndex))
            return;
    }

    m_ChatLine->SetText("");

    int iNameStart = 0;
    int iNameLength = 0;

    player_info_t sPlayerInfo;
    if (iPlayerIndex == 0)
    {
        Q_memset(&sPlayerInfo, 0, sizeof(player_info_t));
        Q_strncpy(sPlayerInfo.name, "Console", sizeof(sPlayerInfo.name));
    }
    else
    {
        engine->GetPlayerInfo(iPlayerIndex, &sPlayerInfo);
    }

    int bufSize = (strlen(pmsg) + 1) * sizeof(wchar_t);
    wchar_t *wbuf = static_cast<wchar_t *>(_alloca(bufSize));
    if (wbuf)
    {
        Color clrNameColor = GetClientColor(iPlayerIndex);

        m_ChatLine->SetExpireTime();

        g_pVGuiLocalize->ConvertANSIToUnicode(pmsg, wbuf, bufSize);

        // find the player's name in the unicode string, in case there is no color markup
        const char *pName = sPlayerInfo.name;

        if (pName)
        {
            wchar_t wideName[MAX_PLAYER_NAME_LENGTH];
            g_pVGuiLocalize->ConvertANSIToUnicode(pName, wideName, sizeof(wideName));

            const wchar_t *nameInString = wcsstr(wbuf, wideName);

            if (nameInString)
            {
                iNameStart = (nameInString - wbuf);
                iNameLength = wcslen(wideName);
            }
        }

        m_ChatLine->SetVisible(false);
        m_ChatLine->SetNameStart(iNameStart);
        m_ChatLine->SetNameLength(iNameLength);
        m_ChatLine->SetNameColor(clrNameColor);

        m_ChatLine->InsertAndColorizeText(wbuf, iPlayerIndex);

        if (m_nMessageMode == MESSAGE_MODE_NONE)
            m_pChatHistory->GotoTextEnd();
    }
}



void ChatPanel::FadeChatHistory()
{
    float frac = (m_flHistoryFadeTime - gpGlobals->curtime) / CHAT_HISTORY_FADE_TIME;

    int alpha = frac * m_iHistoryAlpha;
    alpha = clamp(alpha, 0, m_iHistoryAlpha);

    const auto histBg = m_pChatHistory->GetBgColor();
    if (m_nMessageMode > MESSAGE_MODE_NONE)
    {
        SetAlpha(255);
        m_pChatHistory->SetBgColor(Color(histBg.r(), histBg.g(), histBg.b(), m_iHistoryAlpha - alpha));
        m_pChatInput->SetAlpha((m_iHistoryAlpha * 2) - alpha);
        SetBgColor(Color(GetBgColor().r(), GetBgColor().g(), GetBgColor().b(), m_iHistoryAlpha - alpha));
        m_pFiltersButton->SetAlpha((m_iHistoryAlpha * 2) - alpha);
    }
    else
    {
        m_pChatHistory->SetBgColor(Color(histBg.r(), histBg.g(), histBg.b(), alpha));
        SetBgColor(Color(GetBgColor().r(), GetBgColor().g(), GetBgColor().b(), alpha));

        m_pChatInput->SetAlpha(alpha);
        m_pFiltersButton->SetAlpha(alpha);
    }
}

void ChatPanel::FireGameEvent(IGameEvent *event)
{
    if (FStrEq(event->GetName(), "lobby_leave"))
    {
        m_LobbyID.Clear();
        m_vTypingMembers.RemoveAll();
    }
    else if (FStrEq(event->GetName(), "lobby_spec_update_msg"))
    {
        const auto type = event->GetInt("type");
        const auto person = Q_atoui64(event->GetString("id"));
        const auto target = Q_atoui64(event->GetString("target"));

        CSteamID personID = CSteamID(person);
        CSteamID targetID = CSteamID(target);

        const char *pName = SteamFriends()->GetFriendPersonaName(personID);

        if (type == SPEC_UPDATE_STOP && CBasePlayer::GetLocalPlayer())
        {
            Printf(CHAT_FILTER_JOINLEAVE | CHAT_FILTER_SERVERMSG, "%s has respawned.", pName);
        }
        else if (type == SPEC_UPDATE_STARTED && CBasePlayer::GetLocalPlayer())
        {
            const char *spectateText = target != 1 ? "%s is now spectating." : "%s is now watching a replay.";
            Printf(CHAT_FILTER_JOINLEAVE | CHAT_FILTER_SERVERMSG, spectateText, pName);
        }
        else if (type == SPEC_UPDATE_CHANGETARGET)
        {
            if (target > 1)
            {
                const char *pTargetName = SteamFriends()->GetFriendPersonaName(targetID);
                DevLog("%s is now spectating %s.\n", pName, pTargetName);
            }
            // Printf(CHAT_FILTER_JOINLEAVE | CHAT_FILTER_SERVERMSG,
            //    "%s is now spectating %s.", pName, pTargetName);
        }

        SpectatorUpdate(personID, target);
    }
    else if (FStrEq(event->GetName(), "lobby_update_msg"))
    {
        uint8 type = event->GetInt("type");

        bool isJoin = (type == LOBBY_UPDATE_MEMBER_JOIN_MAP) || (type == LOBBY_UPDATE_MEMBER_JOIN);
        bool isMap = (type == LOBBY_UPDATE_MEMBER_JOIN_MAP) || (type == LOBBY_UPDATE_MEMBER_LEAVE_MAP);

        uint64 person = Q_atoui64(event->GetString("id"));

        CSteamID personID = CSteamID(person);
        const char *pName = SteamFriends()->GetFriendPersonaName(personID);
        Printf(CHAT_FILTER_JOINLEAVE | CHAT_FILTER_SERVERMSG, "%s has %s the %s.", pName, isJoin ? "joined" : "left", isMap ? "map" : "lobby");

        if (!isJoin)
            SpectatorUpdate(personID, k_steamIDNil);
    }
}

int ChatPanel::GetFilterForString(const char *pString)
{
    if (!Q_stricmp(pString, "#HL_Name_Change"))
    {
        return CHAT_FILTER_NAMECHANGE;
    }

    return CHAT_FILTER_NONE;
}

Color ChatPanel::GetTextColorForClient(TextColor colorNum, int clientIndex)
{
    Color c;
    switch (colorNum)
    {
    case COLOR_CUSTOM:
        c = m_ColorCustom;
        break;

    case COLOR_PLAYERNAME:
        c = GetClientColor(clientIndex);
        break;

    case COLOR_LOCATION:
        c = m_cDefaultTextColor;
        break;

    case COLOR_ACHIEVEMENT:
        {
            IScheme *pSourceScheme = scheme()->GetIScheme(scheme()->GetScheme("SourceScheme"));
            if (pSourceScheme)
            {
                c = pSourceScheme->GetColor("SteamLightGreen", GetBgColor());
            }
            else
            {
                c = GetDefaultTextColor();
            }
        }
        break;
    default:
        c = GetDefaultTextColor();
        break;
    }

    return Color(c[0], c[1], c[2], 255);
}

Color ChatPanel::GetClientColor(int clientIndex)
{
    if (clientIndex == 0) // console msg
    {
        return Color(153, 255, 153, 255);
    }

    return Color(255, 178, 0, 255);
}

static void __MsgFunc_SayText(bf_read& msg)
{
    char szString[256];

    int client = msg.ReadByte();
    msg.ReadString(szString, sizeof(szString));
    bool bWantsToChat = msg.ReadByte();

    if (bWantsToChat)
    {
        // print raw chat text
        g_pChatPanel->ChatPrintf(client, CHAT_FILTER_NONE, "%s", szString);
    }
    else
    {
        // try to lookup translated string
        g_pChatPanel->Printf(CHAT_FILTER_NONE, "%s", hudtextmessage->LookupString(szString));
    }

    CLocalPlayerFilter filter;
    C_BaseEntity::EmitSound(filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message");

    Msg("%s", szString);
}

inline char *RemoveColorMarkup(char *str)
{
    char *out = str;
    for (char *in = str; *in != 0; ++in)
    {
        if (*in > 0 && *in < COLOR_MAX)
        {
            if (*in == COLOR_HEXCODE || *in == COLOR_HEXCODE_ALPHA)
            {
                // skip the next six or eight characters
                const int nSkip = (*in == COLOR_HEXCODE ? 6 : 8);
                for (int i = 0; i < nSkip && *in != 0; i++)
                {
                    ++in;
                }

                // if we reached the end of the string first, then back up
                if (*in == 0)
                {
                    --in;
                }
            }

            continue;
        }
        *out = *in;
        ++out;
    }
    *out = 0;

    return str;
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
inline char *ConvertCRtoNL(char *str)
{
    for (char *ch = str; *ch != 0; ch++)
        if (*ch == '\r')
            *ch = '\n';
    return str;
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
inline wchar_t *ConvertCRtoNL(wchar_t *str)
{
    for (wchar_t *ch = str; *ch != 0; ch++)
        if (*ch == L'\r')
            *ch = L'\n';
    return str;
}

inline void StripEndNewlineFromString(char *str)
{
    int s = strlen(str) - 1;
    if (s >= 0)
    {
        if (str[s] == '\n' || str[s] == '\r')
            str[s] = 0;
    }
}

inline void StripEndNewlineFromString(wchar_t *str)
{
    int s = wcslen(str) - 1;
    if (s >= 0)
    {
        if (str[s] == L'\n' || str[s] == L'\r')
            str[s] = 0;
    }
}

//-----------------------------------------------------------------------------
// Purpose: Reads a string from the current message and checks if it is translatable
//-----------------------------------------------------------------------------
inline wchar_t *ReadLocalizedString(bf_read &msg, OUT_Z_BYTECAP(outSizeInBytes) wchar_t *pOut, int outSizeInBytes,
    bool bStripNewline, OUT_Z_CAP(originalSize) char *originalString = nullptr, int originalSize = 0)
{
    char szString[2048];
    szString[0] = 0;
    msg.ReadString(szString, sizeof(szString));

    if (originalString)
    {
        Q_strncpy(originalString, szString, originalSize);
    }

    const wchar_t *pBuf = g_pVGuiLocalize->Find(szString);
    if (pBuf)
    {
        V_wcsncpy(pOut, pBuf, outSizeInBytes);
    }
    else
    {
        g_pVGuiLocalize->ConvertANSIToUnicode(szString, pOut, outSizeInBytes);
    }

    if (bStripNewline)
        StripEndNewlineFromString(pOut);

    return pOut;
}

//-----------------------------------------------------------------------------
// Purpose: Reads a string from the current message, converts it to unicode, and strips out color codes
//-----------------------------------------------------------------------------
wchar_t *ReadChatTextString(bf_read &msg, OUT_Z_BYTECAP(outSizeInBytes) wchar_t *pOut, int outSizeInBytes)
{
    char szString[2048];
    szString[0] = 0;
    msg.ReadString(szString, sizeof(szString));

    g_pVGuiLocalize->ConvertANSIToUnicode(szString, pOut, outSizeInBytes);

    StripEndNewlineFromString(pOut);

    // converts color control characters into control characters for the normal color
    for (wchar_t *test = pOut; test && *test; ++test)
    {
        if (*test && (*test < COLOR_MAX))
        {
            if (*test == COLOR_HEXCODE || *test == COLOR_HEXCODE_ALPHA)
            {
                // mark the next seven or nine characters. one for the control character and six or eight for the code
                // itself.
                const int nSkip = (*test == COLOR_HEXCODE ? 7 : 9);
                for (int i = 0; i < nSkip && *test != 0; i++, test++)
                {
                    *test = COLOR_NORMAL;
                }

                // if we reached the end of the string first, then back up
                if (*test == 0)
                {
                    --test;
                }
            }
            else
            {
                *test = COLOR_NORMAL;
            }
        }
    }

    return pOut;
}

static void __MsgFunc_SayText2(bf_read& msg)
{
    // Got message during connection
    if (!g_PR)
        return;

    int client = msg.ReadByte();
    bool bWantsToChat = msg.ReadByte();

    wchar_t szBuf[6][256];
    char untranslated_msg_text[256];
    wchar_t *msg_text = ReadLocalizedString(msg, szBuf[0], sizeof(szBuf[0]), false, untranslated_msg_text, sizeof(untranslated_msg_text));

    // keep reading strings and using C format strings for substituting the strings into the localised text string
    ReadChatTextString(msg, szBuf[1], sizeof(szBuf[1])); // player name
    ReadChatTextString(msg, szBuf[2], sizeof(szBuf[2])); // chat text
    ReadLocalizedString(msg, szBuf[3], sizeof(szBuf[3]), true);
    ReadLocalizedString(msg, szBuf[4], sizeof(szBuf[4]), true);

    g_pVGuiLocalize->ConstructString(szBuf[5], sizeof(szBuf[5]), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);

    char ansiString[512];
    g_pVGuiLocalize->ConvertUnicodeToANSI(ConvertCRtoNL(szBuf[5]), ansiString, sizeof(ansiString));

    if (bWantsToChat)
    {
        int iFilter = CHAT_FILTER_NONE;

        if (client > 0 && (g_PR->GetTeam(client) != g_PR->GetTeam(GetLocalPlayerIndex())))
        {
            iFilter = CHAT_FILTER_PUBLICCHAT;
        }

        // print raw chat text
        g_pChatPanel->ChatPrintf(client, iFilter, "%s", ansiString);

        Msg("%s\n", RemoveColorMarkup(ansiString));

        CLocalPlayerFilter filter;
        C_BaseEntity::EmitSound(filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message");
    }
    else
    {
        // print raw chat text
        g_pChatPanel->ChatPrintf(client, g_pChatPanel->GetFilterForString(untranslated_msg_text), "%s", ansiString);
    }
}


//-----------------------------------------------------------------------------
// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in
// titles.txt the next (optional) one to four strings are parameters for that string (which can also be message names if
// they begin with '#')
//-----------------------------------------------------------------------------
static void __MsgFunc_TextMsg(bf_read& msg)
{
    char szString[2048];
    int msg_dest = msg.ReadByte();

    wchar_t szBuf[5][256];
    wchar_t outputBuf[256];

    for (int i = 0; i < 5; ++i)
    {
        msg.ReadString(szString, sizeof(szString));
        char *tmpStr = hudtextmessage->LookupString(szString, &msg_dest);
        const wchar_t *pBuf = g_pVGuiLocalize->Find(tmpStr);
        if (pBuf)
        {
            // Copy pBuf into szBuf[i].
            const auto nMaxChars = sizeof(szBuf[i]) / sizeof(wchar_t);
            wcsncpy(szBuf[i], pBuf, nMaxChars);
            szBuf[i][nMaxChars - 1] = 0;
        }
        else
        {
            if (i)
            {
                StripEndNewlineFromString(tmpStr); // these strings are meant for substitution into the main strings, so
                                                   // cull the automatic end newlines
            }
            g_pVGuiLocalize->ConvertANSIToUnicode(tmpStr, szBuf[i], sizeof(szBuf[i]));
        }
    }

    if (!cl_showtextmsg.GetInt())
        return;

    int len;
    switch (msg_dest)
    {
    case HUD_PRINTCENTER:
        g_pVGuiLocalize->ConstructString(outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);
        internalCenterPrint->Print(ConvertCRtoNL(outputBuf));
        break;

    case HUD_PRINTNOTIFY:
        g_pVGuiLocalize->ConstructString(outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);
        g_pVGuiLocalize->ConvertUnicodeToANSI(outputBuf, szString, sizeof(szString));
        len = strlen(szString);
        if (len && szString[len - 1] != '\n' && szString[len - 1] != '\r')
        {
            Q_strncat(szString, "\n", sizeof(szString), 1);
        }
        Msg("%s", ConvertCRtoNL(szString));
        break;

    case HUD_PRINTTALK:
        g_pVGuiLocalize->ConstructString(outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);
        g_pVGuiLocalize->ConvertUnicodeToANSI(outputBuf, szString, sizeof(szString));
        len = strlen(szString);
        if (len && szString[len - 1] != '\n' && szString[len - 1] != '\r')
        {
            Q_strncat(szString, "\n", sizeof(szString), 1);
        }
        g_pChatPanel->Printf(CHAT_FILTER_NONE, "%s", ConvertCRtoNL(szString));
        Msg("%s", ConvertCRtoNL(szString));
        break;

    case HUD_PRINTCONSOLE:
        g_pVGuiLocalize->ConstructString(outputBuf, sizeof(outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);
        g_pVGuiLocalize->ConvertUnicodeToANSI(outputBuf, szString, sizeof(szString));
        len = strlen(szString);
        if (len && szString[len - 1] != '\n' && szString[len - 1] != '\r')
        {
            Q_strncat(szString, "\n", sizeof(szString), 1);
        }
        Msg("%s", ConvertCRtoNL(szString));
        break;
    }
}
