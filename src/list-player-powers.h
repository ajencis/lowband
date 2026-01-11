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

#define MS(x) PP(x##_MAGIC)
#include "list-magic-schools.h"
#undef MS
PP(SPELL_POWER)
PP(SPELL_EASE)
PP(UNLIGHT)
PP(GLOW)
PP(WHIRLWIND)
PP(DEATH_TOUCH)
PP(SWORD_SPECIALIZATION)
PP(CRITICAL_HITS)
PP(BACKSTAB)
PP(HAFTED_SPECIALIZATION)
PP(STUNNING_BLOWS)
PP(POLEARM_SPECIALIZATION)
PP(BOW_SPECIALIZATION)
PP(CROSSBOW_SPECIALIZATION)
PP(SLING_SPECIALIZATION)
PP(DUAL_WIELD)
PP(UNARMED_STRIKE)
PP(AGILITY)
PP(RUNNING)
PP(ANTIMAGIC)
PP(BREATH)
PP(BREATH_BITE)
PP(BERSERK)
PP(NATURAL_COMBAT)
PP(FRIGHTENING_PRESENCE)
PP(ANIMATE_CHAINS)
PP(STENCH)
PP(REGENERATION)
