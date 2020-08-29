#pragma once

#include "vgui_controls/EditablePanel.h"

struct LobbyEnter_t;
struct LobbyChatUpdate_t;
struct LobbyDataUpdate_t;

class LobbyInfoPanel : public vgui::EditablePanel
{
public:
    DECLARE_CLASS_SIMPLE(LobbyInfoPanel, EditablePanel);

    LobbyInfoPanel(Panel* pParent);
    ~LobbyInfoPanel();

    void OnLobbyEnter(LobbyEnter_t *pParam);
    void OnLobbyLeave();

    void OnLobbyDataUpdate(LobbyDataUpdate_t *pData);
    void OnLobbyChatUpdate(LobbyChatUpdate_t *pData);

protected:
    void OnCommand(const char* command) override;
    void OnMousePressed(vgui::MouseCode code) override;
    void OnReloadControls() override;

private:
    void LobbyEnterSuccess();
    void UpdateLobbyMemberCount() const;
    void SetLobbyTypeImage() const;
    void UpdateLobbyName();
    void SetLobbyType() const;

    bool m_bInLobby;

    vgui::ImagePanel *m_pLobbyType;
    vgui::IImage *m_pLobbyTypePublic, *m_pLobbyTypeFriends, *m_pLobbyTypePrivate;

    vgui::Label *m_pMainStatus, *m_pSubStatus;

    vgui::Button *m_pLobbyToggleButton, *m_pInviteUserButton, *m_pLobbySettingsButton;
};