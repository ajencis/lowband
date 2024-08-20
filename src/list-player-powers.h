/**
 * \file list-player-powers.h
 * \brief player race and class powers
 *
 * Fields:
 * symbol - the flag name
 * additional details in player_property.txt
 */

#define MS(x, a, b) PP(x##_SPECIALIZATION, b, 10)
#include "list-magic-schools.h"
#undef MS
PP(SWORD_SPECIALIZATION, "Swordmastery", 15)
PP(HAFTED_SPECIALIZATION, "Haftedmastery", 15)
PP(POLEARM_SPECIALIZATION, "Polearm-mastery", 15)
PP(BOW_SPECIALIZATION, "Bowmastery", 15)
PP(UNARMED_STRIKE, "Unarmed Strike", 10)
PP(UNARMOURED_AGILITY, "Unarmoured Agility", 10)
PP(BACKSTAB, "Backstabbing", 10)
