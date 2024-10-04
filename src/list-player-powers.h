/**
 * \file list-player-powers.h
 * \brief player race and class powers
 *
 * Fields:
 * symbol - the flag name
 * name - the display name
 * expon - whether the power scales exponentially
 * power - the extent to which it uses up additional power slots
 * weight - its likelihood to appear as a tome
 * update - what to update if it changes
 * additional details in player_property.txt
 */

#define MS(x, a, b) PP(x##_MAGIC, b, false, 100, 5, 0)
#include "list-magic-schools.h"
#undef MS
/* symbol					name				expon	power	weight	update*/
PP(SWORD_SPECIALIZATION,	"Swordmastery",		false,	150,	15,		PU_BONUS)
PP(HAFTED_SPECIALIZATION,	"Haftedmastery",	false,	150,	10,		PU_BONUS)
PP(POLEARM_SPECIALIZATION,	"Polearm-mastery",	false,	150,	10,		PU_BONUS)
PP(BOW_SPECIALIZATION,		"Bowmastery",		false,	100,	10,		PU_BONUS)
PP(CROSSBOW_SPECIALIZATION,	"Crossbowmastery",	false,	100,	5,		PU_BONUS)
PP(SLING_SPECIALIZATION,	"Slingmastery",		false,	100,	5,		PU_BONUS)
PP(UNARMED_STRIKE,			"Unarmed Strike",	true,	100,	5,		PU_BONUS)
PP(AGILITY,					"Agility",			false,	100,	10,		PU_BONUS)
PP(BACKSTAB,				"Backstabbing",		false,	100,	10,		PU_BONUS)
PP(RESILIENCE,				"Resilience",		false,	150,	5,		PU_BONUS | PU_HP)
