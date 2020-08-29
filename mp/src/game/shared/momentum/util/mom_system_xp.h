#pragma once

#include "igamesystem.h"

#define COSXP_MAX_LEVELS 500

class MomentumXPSystem : public CAutoGameSystem
{
public:
    MomentumXPSystem();

    void PostInit() override;


    // Returns the total amount of XP required for a specific level
    int GetCosmeticXPForLevel(int level);
    // Returns the amount of XP that one particular level has until the next level
    int GetCosmeticXPInLevel(int level);

private:
    int m_arrXPForLevels[COSXP_MAX_LEVELS];
    int m_arrXPInLevels[COSXP_MAX_LEVELS];
};

extern MomentumXPSystem *g_pXPSystem;