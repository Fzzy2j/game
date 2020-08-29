#include "cbase.h"

#include "ChatEntry.h"
#include "ChatPanel.h"

#include "tier0/memdbgon.h"

using namespace vgui;

ChatEntry::ChatEntry(ChatPanel *parent) : BaseClass(parent, "ChatEntry")
{
    m_pChatParent = parent;
    SetCatchEnterKey(true);
    SetAllowNonAsciiCharacters(true);
    SetDrawLanguageIDAtLeft(true);
    SetMaximumCharCount(127);
}

void ChatEntry::ApplySchemeSettings(IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    const auto hFont = GetSchemeFont(pScheme, m_FontName, "Chat.Font");
    SetFont(hFont);

    SetFgColor(m_cTypingColor);

    SetMouseInputEnabled(true);
    SetPaintBorderEnabled(false);
}

void ChatEntry::OnKeyCodeTyped(KeyCode code)
{
    if (code == KEY_ENTER || code == KEY_PAD_ENTER || code == KEY_ESCAPE)
    {
        if (code != KEY_ESCAPE)
        {
            PostMessage(GetVParent(), new KeyValues("ChatEntrySend"));
        }

        if (m_pChatParent->GetMessageMode() == MESSAGE_MODE_MENU)
        {
            BaseClass::OnKeyCodeTyped(code);
        }
        else
        {
            PostMessage(GetVParent(), new KeyValues("ChatEntryStopMessageMode"));
        }
    }
    else if (code == KEY_TAB)
    {
        // Ignore tab, otherwise vgui will screw up the focus.
        return;
    }
    else
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}