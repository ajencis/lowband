/**
 * \file player-properties.c 
 * \brief Class and race abilities
 *
 * Copyright (c) 1997-2020 Ben Harrison, James E. Wilson, Robert A. Koeneke,
 * Leon Marrick, Bahman Rabii, Nick McConnell
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "game-world.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-spell.h"
#include "player-calcs.h"
#include "player-properties.h"
#include "player-spell.h"
#include "player-util.h"
#include "ui-player-properties.h"
#include "game-input.h"



/**
 * L: Ability predicates
 */

static bool pred_HAS_BITE(const struct player_ability *abil, const struct player *p)
{
	int i;

	for (i = 0; i < p->mon.state.num_attacks; ++i) {
		const struct py_attack_roll *aroll = &p->mon.state.attacks[i];

		if (streq(aroll->name, "bite")) {
			return true;
		}
	}

	return false;
}

static bool pred_HAS_BREATH(const struct player_ability *abil, const struct player *p)
{
	struct monster_race *mr = lookup_player_monster(p);

	if (!mr) return false;

	return test_spells(mr->spell_flags, RST_BREATH);
}

static bool pred_HAS_MATCHING_SPELL(const struct player_ability *abil, const struct player *p)
{
	int i, j;

	if (!character_generated) return true;

	for (i = 0; i < z_info->spell_max; ++i) {
		struct player_spell *ps = player_spell_lookup(i);
		assert(ps);
		if (!(p->player_spell_flags[i] & PY_SPELL_WORKED)) continue;
		for (j = 0; j < MAX_SPELL_SCHOOLS; ++j) {
			int school = ps->school[j];
			if (school > MS_NONE) {
				if (abil->index == school) {
					return true;
				}
			}
		}
	}

	return false;
}


abil_predicate ability_predicates[] = {
	#define PRED(x) pred_##x,
	#include "list-ability-predicates.h"
	#undef PRED
	NULL
};



bool ability_satisfies_all_prereqs(const struct player_ability *abil, const struct player *p)
{
	int i;

	for (i = 0; i < ABIL_PRED_MAX; ++i) {
		if (!abil->prereqs[i]) continue;
		if (!ability_predicates[i](abil, p)) {
			return false;
		}
	}

	return true;
}



/**
 * ------------------------------------------------------------------------
 * Ability utilities
 * ------------------------------------------------------------------------ */

struct player_ability *lookup_player_ability(int idx, int type)
{
	struct player_ability *abil;

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->index == idx && abil->type == type) {
			return abil;
		}
	}

	return NULL;
}


bool class_has_ability(const struct player_class *class,
					   struct player_ability *ability)
{
	if ((ability->type == PY_ABIL_PLAYER) &&
			pf_has(class->pflags, ability->index)) {
		return true;
	} else if ((ability->type == PY_ABIL_OBJECT) &&
			of_has(class->flags, ability->index)) {
		return true;
	}

	return false;
}

bool race_has_ability(const struct player_race *race,
					  struct player_ability *ability)
{
	if ((ability->type == PY_ABIL_PLAYER) &&
		pf_has(race->pflags, ability->index)) {
		return true;
	} else if ((ability->type == PY_ABIL_OBJECT) &&
			of_has(race->flags, ability->index)) {
		return true;
	} else if ((ability->type == PY_ABIL_ELEMENT) &&
			(race->el_info[ability->index].res_level == ability->value)) {
		return true;
	}

	return false;
}

#define MAX_ABILITIES 32
/**
 * Browse known abilities -BR-
 */
static void view_abilities(void)
{
	struct player_ability *ability;
	int num_abilities = 0;
	struct player_ability ability_list[MAX_ABILITIES];

	/* Count the number of class powers we have */
	for (ability = player_abilities; ability && num_abilities < MAX_ABILITIES; ability = ability->next) {
		if (class_has_ability(player->class, ability)) {
			memcpy(&ability_list[num_abilities], ability,
				   sizeof(struct player_ability));
			ability_list[num_abilities++].group = PLAYER_FLAG_CLASS;
		}
	}

	/* Count the number of race powers we have */
	for (ability = player_abilities; ability && num_abilities < MAX_ABILITIES; ability = ability->next) {
		if (race_has_ability(player->race, ability)) {
			memcpy(&ability_list[num_abilities], ability,
				   sizeof(struct player_ability));
			ability_list[num_abilities++].group = PLAYER_FLAG_RACE;
		}
	}

	// L: powers get listed
	for (ability = player_abilities; ability && num_abilities < MAX_ABILITIES; ability = ability->next) {
		if ((ability->type == PY_ABIL_POWER) && player->mon.state.powers[ability->index] != 0) {
			memcpy(&ability_list[num_abilities], ability,
				   sizeof(struct player_ability));
			ability_list[num_abilities++].group = PLAYER_FLAG_POWER;
		}
	}

	// L: skills get listed too!
	for (ability = player_abilities; ability && num_abilities < MAX_ABILITIES; ability = ability->next) {
		if ((ability->type == PY_ABIL_SKILL) && player->mon.state.skills[ability->index] > 0) {
			memcpy(&ability_list[num_abilities], ability, sizeof(ability_list[0]));
			ability_list[num_abilities++].group = PLAYER_FLAG_SKILL;
		}
	}

	if (num_abilities == 0) {
		msg("You have no abilities.");
		return;
	}

	/* View choices until user exits */
	view_ability_menu(ability_list, num_abilities);

	return;
}


/**
 * Interact with abilities -BR-
 */
void do_cmd_abilities(void)
{
	/* View existing abilities */
	view_abilities();

	return;
}





bool mon_power_minimum(const struct monster *mon, int power, int min)
{
	return mon->state.powers[power] >= min;
}

int get_mon_power_scale(const struct monster *mon, int power, int scaleto)
{
	int lev = mon_lev(mon), result;

	result =  get_power_scale_state(&mon->state, power, scaleto, lev);

	return result;
}

bool mon_has_power(const struct monster *mon, int power)
{
	return mon_power_minimum(mon, power, 1);
}


const char *ability_subchoice_title(const struct player_ability *parent)
{
	if (parent->type == PY_ABIL_SKILL && parent->index == SKILL_MAGIC) {
		return "realm";
	}

	return NULL;
}

int ability_subchoice_choices(struct player_ability *parent)
{
	if (parent->index == SKILL_MAGIC && parent->type == PY_ABIL_SKILL) {
		return z_info->realm_max;
	}

	return 0;
}

const char *ability_subchoice_name(int id, const struct player_ability *parent)
{
	if (parent->index == SKILL_MAGIC && parent->type == PY_ABIL_SKILL) {
		struct magic_realm *realm = realm_by_index(id);
		assert(realm);
		return realm->name;
	}

	return NULL;
}

static bool ability_needs_subchoice(struct player_ability *abil, struct player *p)
{
	int curr = 0;

	if (p->extra_choice[abil->learn_index] >= 0) return false;
	if (abil->type == PY_ABIL_POWER) curr = p->extra_powers[abil->index];
	else if (abil->type == PY_ABIL_SKILL) curr = p->extra_skills[abil->index];

	if (curr <= 0 && p->extra_target[abil->learn_index] <= 0) return false;
	if (ability_subchoice_choices(abil) <= 0) return false;

	return true;
}

bool make_ability_subchoice(struct player *p)
{
	struct player_ability *abil;
	bool choice_made = false;

	for (abil = player_abilities; abil; abil = abil->next) {
		if (ability_needs_subchoice(abil, p)) {
			choice_made = textui_ability_subchoice(p, abil) || choice_made;
		}
	}

	return choice_made;
}



int attack_specialization_power(const struct monster *mon, const struct object *obj, const struct monster_blow *blow)
{
	if (obj) {
		if (obj->tval == TV_HAFTED) {
			return get_mon_power_scale(mon, PP_HAFTED_SPECIALIZATION, 100);
		}
		if (obj->tval == TV_POLEARM) {
			return get_mon_power_scale(mon, PP_POLEARM_SPECIALIZATION, 100);
		}
		if (obj->tval == TV_SWORD) {
			return get_mon_power_scale(mon, PP_SWORD_SPECIALIZATION, 100);
		}
		if (obj->tval == TV_BOW) {
			if (my_stristr(obj->kind->name, "sling")) {
				return get_mon_power_scale(mon, PP_SLING_SPECIALIZATION, 100);
			}
			if (my_stristr(obj->kind->name, "crossbow")) {
				return get_mon_power_scale(mon, PP_CROSSBOW_SPECIALIZATION, 100);
			}
			if (my_stristr(obj->kind->name, "bow")) {
				return get_mon_power_scale(mon, PP_BOW_SPECIALIZATION, 100);
			}
		}

		return 0;
	}

	if (blow) {
		return get_mon_power_scale(mon, PP_UNARMED_STRIKE, 50);
	}

	return get_mon_power_scale(mon, PP_UNARMED_STRIKE, 100);
}


