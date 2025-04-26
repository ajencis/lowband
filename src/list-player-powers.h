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

#define MS(x, a, b) PP(x##_MAGIC, b, false, 100, 5, 0, TOME_SKILL_MAGIC)
#include "list-magic-schools.h"
#undef MS
/* symbol					name				scale				power	weight	update*/
PP(SWORD_SPECIALIZATION,	"Swordmastery",		PP_SCALE_LINEAR,	150,	15,		PU_BONUS,			TOME_SKILL_TO_HIT_MELEE)
PP(HAFTED_SPECIALIZATION,	"Haftedmastery",	PP_SCALE_LINEAR,	150,	10,		PU_BONUS,			TOME_SKILL_TO_HIT_MELEE)
PP(POLEARM_SPECIALIZATION,	"Polearm-mastery",	PP_SCALE_LINEAR,	150,	10,		PU_BONUS,			TOME_SKILL_TO_HIT_MELEE)
PP(BOW_SPECIALIZATION,		"Bowmastery",		PP_SCALE_LINEAR,	100,	10,		PU_BONUS,			TOME_SKILL_TO_HIT_BOW)
PP(CROSSBOW_SPECIALIZATION,	"Crossbowmastery",	PP_SCALE_LINEAR,	100,	5,		PU_BONUS,			TOME_SKILL_TO_HIT_BOW)
PP(SLING_SPECIALIZATION,	"Slingmastery",		PP_SCALE_LINEAR,	100,	5,		PU_BONUS,			TOME_SKILL_TO_HIT_BOW)
PP(UNARMED_STRIKE,			"Unarmed Strike",	PP_SCALE_SQUARE,	100,	5,		PU_BONUS,			TOME_SKILL_TO_HIT_MELEE)
PP(AGILITY,					"Agility",			PP_SCALE_SQRT,		100,	10,		PU_BONUS,			TOME_SKILL_STEALTH)
PP(BACKSTAB,				"Backstabbing",		PP_SCALE_SQRT,		100,	10,		PU_BONUS,			TOME_PP_SWORD_SPECIALIZATION)
PP(RUNNING,					"Running",			PP_SCALE_SQRT,		100,	5,		PU_BONUS,			TOME_PP_AGILITY)
PP(DUAL_WIELD,				"Dual-Wielding",	PP_SCALE_LINEAR,	100,	5,		PU_BONUS,			TOME_SKILL_TO_HIT_MELEE)
PP(ANTIMAGIC,				"Antimagic",		PP_SCALE_SQRT,		200,	3,		PU_BONUS | PU_MANA,	TOME_SKILL_HEALTH)
PP(SPELL_POWER,				"Spellpower",		PP_SCALE_LINEAR,	150,	3,		0,					TOME_SKILL_MAGIC)
PP(SPELL_EASE,				"Spell Ease",		PP_SCALE_LINEAR,	150,	3,		0,					TOME_SKILL_MAGIC)
PP(WHIRLWIND,				"Whirlwind Attack",	PP_SCALE_SQRT,		200,	5,		0,					TOME_PP_AIR_MAGIC)
PP(CRITICAL_HITS,			"Critical Hits",	PP_SCALE_LINEAR,	150,	5,		PU_BONUS,			TOME_PP_SWORD_SPECIALIZATION)
PP(DEATH_TOUCH,				"Touch of Death",	PP_SCALE_LINEAR,	150,	3,		PU_BONUS,			TOME_PP_NECROMANCY_MAGIC)
PP(UNLIGHT,					"Unlight",			PP_SCALE_SQUARE,	100,	5,		PU_BONUS,			TOME_PP_DARKNESS_MAGIC)
PP(GLOW,					"Glow",				PP_SCALE_LINEAR,	100,	5,		0,					TOME_PP_LIGHT_MAGIC)
