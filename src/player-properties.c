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
#include "init.h"
#include "player-properties.h"
#include "player-spell.h"
#include "ui-player-properties.h"
#include "game-input.h"

/**
 * ------------------------------------------------------------------------
 * Ability utilities
 * ------------------------------------------------------------------------ */

struct player_ability *lookup_player_ability(int idx, int type)
{
	struct player_ability *abil;

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->index == idx && type == abil->type) {
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
		if ((ability->type == PY_ABIL_POWER) && player->state.powers[ability->index] > 0) {
			memcpy(&ability_list[num_abilities], ability,
				   sizeof(struct player_ability));
			ability_list[num_abilities++].group = PLAYER_FLAG_POWER;
		}
	}

	// L: skills get listed too!
	for (ability = player_abilities; ability && num_abilities < MAX_ABILITIES; ability = ability->next) {
		if ((ability->type == PY_ABIL_SKILL) && player->state.skills[ability->index] > 0) {
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


