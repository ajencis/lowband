/**
 * \file list-player-powers.h
 * \brief player race and class powers
 *
 * Fields:
 * symbol - the flag name
 * name - the display name
 * power - the extent to which it uses up additional power slots
 * weight - its likelihood to appear as a tome
 * additional details in player_property.txt
 */

#define MS(x, a, b) PP(x##_MAGIC, b, 10, 5)
#include "list-magic-schools.h"
#undef MS
/* symbol					name					power	weight*/
PP(SWORD_SPECIALIZATION,	"Swordmastery",			15,		15)
PP(HAFTED_SPECIALIZATION,	"Haftedmastery",		15,		15)
PP(POLEARM_SPECIALIZATION,	"Polearm-mastery",		15,		15)
PP(BOW_SPECIALIZATION,		"Bowmastery",			15,		15)
PP(UNARMED_STRIKE,			"Unarmed Strike",		10,		5)
PP(UNARMOURED_AGILITY,		"Unarmoured Agility",	10,		5)
PP(BACKSTAB,				"Backstabbing",			10,		10)
