#pragma once

#include "vgui_controls/PropertyPage.h"

class UserComponent;

class DrawerPanel_Profile : public vgui::PropertyPage
{
public:
    DECLARE_CLASS_SIMPLE(DrawerPanel_Profile, PropertyPage);
    DrawerPanel_Profile(Panel *pParent);

protected:
    void OnResetData() override;


private:

    UserComponent *m_pUserComponent;
    vgui::PropertySheet *m_pStatsAndActivitySheet;

};