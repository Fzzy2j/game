#pragma once

struct UserData
{
    uint64 m_uID;
    char m_szAlias[32];
    int m_iCurrentLevel;
    int m_iCurrentXP;

    UserData();
    bool ParseFromKV(KeyValues *pUserKv);
    void SaveToKV(KeyValues *pKvOut);
};

class MomentumUserData : public CAutoGameSystem, public CGameEventListener
{
public:
    MomentumUserData();

    const UserData &GetLocalUserData() const { return m_LocalUserData; }

    void AddUserDataChangeListener(vgui::VPANEL hPanel);
    void RemoveUserDataChangeListener(vgui::VPANEL hPanel);

protected:
    void PostInit() override;
    void Shutdown() override;

    void FireGameEvent(IGameEvent *event) override;

private:
    bool SaveLocalUserData();

    void FireUserDataUpdate();
    void OnUserDataReceived(KeyValues *pDataKV);

    UserData m_LocalUserData;
    CUtlVector<vgui::VPANEL> m_vecUserDataChangeListeners;
};

extern MomentumUserData *g_pUserData;