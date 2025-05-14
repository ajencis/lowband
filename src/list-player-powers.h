/**
 * \file list-player-powers.h
 * \brief player race and class powers
 *
 * Fields:
 * symbol - the flag name
 * name - the display name
 * scale - how the power scales
 * power - the extent to which it uses up additional power slots
 * weight - its likelihood to appear as a tome
 * update - what to update if it changes
 * additional details in player_property.txt
 */

#define MS(x, a, b) PP(x##_MAGIC)
#include "list-magic-schools.h"
#undef MS
PP(SWORD_SPECIALIZATION)
PP(HAFTED_SPECIALIZATION)
PP(POLEARM_SPECIALIZATION)
PP(BOW_SPECIALIZATION)
PP(CROSSBOW_SPECIALIZATION)
PP(SLING_SPECIALIZATION)
PP(UNARMED_STRIKE)
PP(AGILITY)
PP(BACKSTAB)
PP(RUNNING)
PP(DUAL_WIELD)
PP(ANTIMAGIC)
PP(SPELL_POWER)
PP(SPELL_EASE)
PP(WHIRLWIND)
PP(CRITICAL_HITS)
PP(DEATH_TOUCH)
PP(UNLIGHT)
PP(GLOW)
PP(BREATH)
