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

#define MS(x, a, b) PP(x##_MAGIC, b, false, 10, 5, 0)
#include "list-magic-schools.h"
#undef MS
/* symbol					name				expon	power	weight	update*/
PP(SWORD_SPECIALIZATION,	"Swordmastery",		false,	15,		15,		PU_BONUS)
PP(HAFTED_SPECIALIZATION,	"Haftedmastery",	false,	15,		15,		PU_BONUS)
PP(POLEARM_SPECIALIZATION,	"Polearm-mastery",	false,	15,		15,		PU_BONUS)
PP(BOW_SPECIALIZATION,		"Bowmastery",		false,	15,		15,		PU_BONUS)
PP(UNARMED_STRIKE,			"Unarmed Strike",	true,	10,		5,		PU_BONUS)
PP(DODGING,					"Dodging",			false,	10,		10,		PU_BONUS)
PP(BACKSTAB,				"Backstabbing",		false,	10,		10,		0)
PP(RESILIENCE,				"Resilience",		false,	15,		5,		PU_HP)
