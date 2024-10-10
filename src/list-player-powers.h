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

#define MS(x, a, b) PP(x##_MAGIC, b, false, 100, 5, 0)
#include "list-magic-schools.h"
#undef MS
/* symbol					name				scale	            power	weight	update*/
PP(SWORD_SPECIALIZATION,	"Swordmastery",		PP_SCALE_LINEAR,	150,	15,		PU_BONUS)
PP(HAFTED_SPECIALIZATION,	"Haftedmastery",	PP_SCALE_LINEAR,	150,	10,		PU_BONUS)
PP(POLEARM_SPECIALIZATION,	"Polearm-mastery",	PP_SCALE_LINEAR,	150,	10,		PU_BONUS)
PP(BOW_SPECIALIZATION,		"Bowmastery",		PP_SCALE_LINEAR,	100,	10,		PU_BONUS)
PP(CROSSBOW_SPECIALIZATION,	"Crossbowmastery",	PP_SCALE_LINEAR,	100,	5,		PU_BONUS)
PP(SLING_SPECIALIZATION,	"Slingmastery",		PP_SCALE_LINEAR,	100,	5,		PU_BONUS)
PP(UNARMED_STRIKE,			"Unarmed Strike",	PP_SCALE_SQUARE,	100,	5,		PU_BONUS)
PP(AGILITY,					"Agility",			PP_SCALE_SQRT,	    100,	10,		PU_BONUS)
PP(BACKSTAB,				"Backstabbing",		PP_SCALE_LINEAR,	100,	10,		PU_BONUS)
PP(RESILIENCE,				"Resilience",		PP_SCALE_LINEAR,	150,	5,		PU_BONUS | PU_HP)
