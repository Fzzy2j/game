#pragma once

#include "hudelement.h"
#include "vgui_controls/EditablePanel.h"

class CHudChat : public CHudElement, public vgui::EditablePanel
{
  public:
    DECLARE_CLASS_SIMPLE(CHudChat, EditablePanel);

    CHudChat(const char *pElementName);

    void StartMessageMode();

    void SetVisible(bool state) override;
    void OnThink() override;

    void PerformLayout() override;

    MESSAGE_FUNC(OnStopMessageMode, "StopMessageMode");
};