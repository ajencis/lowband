/**
 * \file list-player-powers.h
 * \brief player race and class powers
 *
 * Fields:
 * symbol - the flag name
 * additional details in player_property.txt
 */

#define MS(x, a, b) PP(x##_SPECIALIZATION, b)
#include "list-magic-schools.h"
#undef MS
PP(SWORD_SPECIALIZATION, "Swordmastery")
PP(HAFTED_SPECIALIZATION, "Haftedmastery")
PP(POLEARM_SPECIALIZATION, "Polearm-mastery")
PP(BOW_SPECIALIZATION, "Bowmastery")
PP(UNARMED_STRIKE, "Unarmed Strike")
PP(UNARMOURED_AGILITY, "Unarmoured Agility")
