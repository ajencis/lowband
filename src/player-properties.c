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
#include "cave.h"
#include "game-world.h"
#include "h-basic.h"
#include "init.h"
#include "message.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-properties.h"
#include "obj-tval.h"
#include "object.h"
#include "player-calcs.h"
#include "player-properties.h"
#include "player-enum.h"
#include "player-spell.h"
#include "player-util.h"
#include "player.h"
#include "ui-player-properties.h"
#include "game-input.h"
#include "z-form.h"
#include "z-util.h"



/**
 * L: Ability predicates
 */

typedef bool (*mon_abil_predicate)(const struct player_ability *abil, const struct monster_race *mon);

static bool pred_race_has_named_natural_attack(const struct monster_race *mr, const char *name)
{
	int i;

	for (i = 0; i < z_info->mon_blows_max && mr->blow[i].method; ++i) {
		if (!name) return true;
		else if (my_stristr(name, name)) return true;
	}

	return false;
}

static bool pred_race_has_natural_weapon(const struct player_ability *abil, const struct monster_race *mr)
{
	return pred_race_has_named_natural_attack(mr, NULL);
}

/**
 * tests the predicate foa all ultimate evolutions (ie evolutions that don't have resulting evolutions)
 * will return  true  if any meet the requirement if  any   is set, otherwise return true only if all meet
 * the prereq
 */
static bool pred_true_for_ultimate_evols(const struct player_ability *abil, const struct monster_race *mr, 
		mon_abil_predicate pred, bool any)
{
	if (mr->evol) {
		struct evolution *evol;

		for (evol = mr->evol; evol; evol = evol->next) {
			bool result = pred_true_for_ultimate_evols(abil, evol->race, pred, any);

			if (any == result) return result;
		}

		return !any;
	}

	return pred(abil, mr);
}

static bool pred_HAS_BITE(const struct player_ability *abil, const struct player *p)
{
	return pred_race_has_named_natural_attack(p->mon.race, "bite");
}

static bool pred_HAS_NATURAL_ATTACK(const struct player_ability *abil, const struct player *p)
{
	return pred_race_has_named_natural_attack(p->mon.race, NULL);
}

static bool pred_EVOL_HAS_NATURAL_ATTACK(const struct player_ability *abil, const struct player *p)
{
	return pred_true_for_ultimate_evols(abil, p->mon.race, pred_race_has_natural_weapon, true);
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

static bool pred_RACE_OR_CLASS_HAS_POWER(const struct player_ability *abil, const struct player *p)
{
	assert(abil->type == PY_ABIL_POWER);

	if (lookup_player_monster(p)->powers[abil->index] > 0) return true;
	if (p->class->c_powers[abil->index] > 0) return true;

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

struct player_ability *lookup_player_subability(int subid, int idx, int type)
{
	struct player_ability *abil;

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->index == idx && abil->type == type && abil->sub_id == subid) {
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

void ability_desc_base(char *buf, size_t bufsize, const struct player_ability *abil, bool second, bool positive)
{
	char verb[80] = "", adj[80] = "", comment[80]= "";
	const char *pronoun = second ? "you" : "user";

	if (abil->desc) {
		strnfmt(buf, bufsize, "%s%s", pronoun, abil->desc);
	}
	else {
		if (second) {
			strnfmt(verb, sizeof verb, "%s", abil->second_verb);
		} else {
			strnfmt(verb, sizeof verb, "%s", abil->third_verb);
		}

		if (positive && abil->pos_adjective) {
			strnfmt(adj, sizeof adj, " %s", abil->pos_adjective);
		} else if (!positive && abil->neg_adjective) {
			strnfmt(adj, sizeof adj, " %s", abil->neg_adjective);
		}

		if (abil->comment) {
			strnfmt(comment, sizeof comment, ", %s", abil->comment);
		}

		strnfmt(buf, bufsize, "%s %s%s%s.", pronoun, verb, adj, comment);
	}

	my_strcap(buf);
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




int ability_subprop_max(int type)
{
	if (type == SUBPROP_TYP_WEAP_KIND) {
		return z_info->k_max;
	}

	return 1;
}

bool abil_subid_valid(int subid, int type)
{
	if (type == SUBPROP_TYP_WEAP_KIND) {
		if (subid > z_info->k_max || subid < 0) {
			return false;
		}
		if (!tval_is_weapon_k(&k_info[subid])) {
			return false;
		}
		if (tval_is_ammo_k(&k_info[subid])) {
			return false;
		}
		return true;
	}

	return false;
}

static void kind_name_normalize(const struct object_kind *kind, char *buf, size_t bufsize)
{
	size_t ni, bi;

	for (ni = 0, bi = 0; bi < bufsize; ++ni) {
		if (kind->name[ni] == '\0') {
			buf[bi] = '\0';
			break;
		}

		if (kind->name[ni] == ' ') {
			if (bi <= 0 || buf[bi - 1] == ' ') {
				continue;
			}
		}

		else if (kind->name[ni] == '~' || kind->name[ni] == '#' || kind->name[ni] == '&') {
			continue;
		}

		buf[bi] = kind->name[ni];
		bi++;
	}
}

char *ability_subprop_name(const struct player_ability *abil, int subprop_type)
{
	char result[80], temp[80], subname[80];

	if (subprop_type == SUBPROP_TYP_WEAP_KIND) {
		kind_name_normalize(&k_info[abil->sub_id], subname, sizeof subname);

		strnfmt(temp, sizeof temp, "%s", abil->name);
		strnfmt(result, sizeof result, temp, subname);

		return string_make(result);

		/*count = -1;

		for (i = 0; i < z_info->k_max; ++i) {
			if (tval_is_weapon_k(&k_info[i]) && !tval_is_ammo_k(&k_info[i])) {
				count++;

				if (count == abil->sub_id) {
					kind_name_normalize(&k_info[i], subname, sizeof subname);

					dbg_log_fmt("subprop", "subprop %i is named %s / %s", count, k_info[i].name, subname);

					strnfmt(temp, sizeof temp, "%s", abil->name);
					strnfmt(result, sizeof result, temp, subname);
					return string_make(result);
				}
			}
		}

		return "error";*/
	}

	return NULL;
}

bool abil_subprop_currently_relevant(const struct monster *mon, const struct player_ability *abil)
{
	if (abil->type == PY_ABIL_POWER && abil->index == PP_ONE_WEAP_EXPERT) {
		int i;

		if (!mon->body.slots) {
			return false;
		}

		for (i = 0; i < mon->body.count; ++i) {
			if (mon->body.slots[i].obj && mon->body.slots[i].obj->kind->kidx == (uint16_t)abil->sub_id) {
				return true;
			}
		}

		return false;
	}

	return true;
}





bool mon_power_minimum(const struct monster *mon, int power, int min)
{
	return mon->state.powers[power] >= min;
}

/*int get_mon_power_scale(const struct monster *mon, int power, int scaleto)
{
	int lev = mon_lev(mon), result;

	result =  get_power_scale_state(&mon->state, power, scaleto, lev);

	return result;
}*/

bool mon_has_power(const struct monster *mon, int power)
{
	assert(mon);
	assert(power >= 0 && power < PP_MAX);

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

	if (abil->id < 0) return false;
	if (p->extra_choice[abil->id] >= 0) return false;
	curr = p->extra_learned[abil->id];

	if (curr <= 0 && p->extra_target[abil->id] <= 0) return false;
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


struct player_ability *attack_spec_type(const struct object *obj, const struct monster_blow *blow)
{
	if (!obj && !blow) {
		return lookup_player_ability(PP_UNARMED_STRIKE, PY_ABIL_POWER);
	}
	else if (!obj) {
		return lookup_player_ability(PP_NATURAL_COMBAT, PY_ABIL_POWER);
	}

	switch (obj->tval) {
	case TV_SWORD:
		return lookup_player_ability(PP_SWORD_SPECIALIZATION, PY_ABIL_POWER);
	case TV_HAFTED:
		return lookup_player_ability(PP_HAFTED_SPECIALIZATION, PY_ABIL_POWER);
	case TV_POLEARM:
		return lookup_player_ability(PP_POLEARM_SPECIALIZATION, PY_ABIL_POWER);
	case TV_BOW:
		if (my_stristr(obj->kind->name, "sling")) {
			return lookup_player_ability(PP_SLING_SPECIALIZATION, PY_ABIL_POWER);
		}
		if (my_stristr(obj->kind->name, "crossbow")) {
			return lookup_player_ability(PP_CROSSBOW_SPECIALIZATION, PY_ABIL_POWER);
		}
		if (my_stristr(obj->kind->name, "bow")) {
			return lookup_player_ability(PP_BOW_SPECIALIZATION, PY_ABIL_POWER);
		}
		return NULL;
	default:
		return NULL;
	}
}

struct player_ability *attack_expert_type(const struct object *obj, const struct monster_blow *blow)
{
	struct player_ability *result;

	if (!obj) {
		return NULL;
	}

	return lookup_player_subability((int)obj->kind->kidx, PP_ONE_WEAP_EXPERT, PY_ABIL_POWER);
}

int attack_specialization_power(const struct monster *mon, const struct object *obj, const struct monster_blow *blow)
{
	struct player_ability *abil = attack_spec_type(obj, blow);

	if (abil && abil->type == PY_ABIL_POWER) {
		return get_power_scale(mon, abil->index, 100);
	}
	return 0;
}

int attack_expertise_power(const struct monster *mon, const struct object *obj, const struct monster_blow *blow)
{
	struct player_ability *abil = attack_expert_type(obj, blow);

	if (abil && abil->type == PY_ABIL_POWER) {
		return get_power_scale(mon, abil->index, 100);
	}
	return 0;
}



/**
 * L: utilities for powers in general
 */
int get_skill_scale_state(const struct player_state *state, int skill, int scaleto)
{
	int base = state->skills[skill];

	assert(skill >= 0 && skill < SKILL_MAX);
	if (base <= 0) return 0;

	return (base * scaleto + 100 * 2 / 3) / 100;
}

int get_power_scale_state(const struct player_state *state, int power, int scaleto)
{
	int base, sign;
	double div, lev_fact;
	struct player_ability *abil = lookup_player_ability(power, PY_ABIL_POWER);

	assert(power < PP_MAX && power > PP_NONE);
	
	base = ABS(state->powers[power]);
	sign = SGN(state->powers[power]);

	div = MAX((base + 100.0) / 3.0, 50.0); // scale down if above 50

	lev_fact = exponentiate_dbl(base / div, abil->scale_num, abil->scale_den);

	return (int)(lev_fact * scaleto + 1.0 / 3.0) * sign;
}

int get_skill_scale(const struct monster *mon, int skill, int scaleto)
{
	return get_skill_scale_state(&mon->state, skill, scaleto);
}

int get_power_scale(const struct monster *mon, int power, int scaleto)
{
	return get_power_scale_state(&mon->state, power, scaleto);
}


/**
 * L: tries to improve a power that is practised
 */
static int py_extra_target(const struct player *p, const struct player_ability *abil)
{
	int base, xtra = 0;

	if (abil->id < 0) return 0;
	assert(abil->id < z_info->abil_id_max);

	base = p->extra_target[abil->id];

	if (abil->type == PY_ABIL_POWER && p->class) {
		xtra = p->class->c_powers[abil->index];
	} else if (abil->type == PY_ABIL_SKILL && p->class) {
		xtra = p->class->x_skills[abil->index];
	}

	if (p->class && pf_has(p->class->pflags, PF_EXTRA_LEARNING)) {
		xtra = MAX(xtra, base);
	}

	return base + xtra;
}

bool increase_ability(struct monster *mon, const struct player_ability *abil, bool verbose)
{
	struct player *p = mon->player;
	bool monster = abil->index == SKILL_MONSTER && abil->type == PY_ABIL_SKILL && !p;
	//char name[80];

	if (monster) {
		mon->mon_lev++;

		mflag_on(mon->mflag, MFLAG_UPDATE_STATE);

		return true;
	}

	if (!p) return false;
	if (abil->id < 0) return false;
	if (p->extra_learned[abil->id] >= py_extra_target(p, abil)) return false;
	assert(abil->id < z_info->abil_id_max);

	p->extra_learned[abil->id]++;

	if (verbose && mon_is_player(mon)) {
		//strnfmt(name, sizeof name, "%s", abil->name);

		msg("You feel more familiar with %s.", abil->name);
	}

	p->upkeep->update |= PU_BONUS;

	return true;
}

bool exercise_ability(struct monster *mon, const struct player_ability *abil, int efficacy)
{
	int learn_i = abil->id, target, curr, total, chance, bonus;
	bool monster = abil->index == SKILL_MONSTER && abil->type == PY_ABIL_SKILL && !mon->player;

	if (!abil || !mon) return false;
	if (learn_i < 0) return false;
	if (!mon->player && !monster) return false;
	if (efficacy <= 0) return false;

	if (monster) {
		struct evolution *evol;
		target = mon->race->level;

		for (evol = mon->race->evol; evol; evol = evol->next) {
			target = MAX(evol->race->level, target);
		}

		curr = mon->mon_lev;
	} else {
		target = py_extra_target(mon->player, abil);
		curr = mon->player->extra_learned[abil->id];
	}

	if (abil->type == PY_ABIL_POWER) {
		total = mon->state.powers[abil->index];
	} else if (abil->type == PY_ABIL_SKILL) {
		total = mon->state.skills[abil->index];
	} else {
		total = 0;
	}

	if (monster) {
		bonus = 0;
	} else {
		bonus = mon->player->learned_when[learn_i];
		bonus -= total * curr * 100;
	}

	if (bonus > 0) {
		total -= my_int_sqrt(bonus);
	}
	else {
		curr += my_int_sqrt(-bonus);
	}

	if (curr >= target) return false;
	if (curr >= efficacy) return false;
	if (!monster && total >= mon->player->lev) return false;

	chance = 100; // base 1 in 100

	chance *= MAX(1, total);
	chance *= MAX(1, curr);

	chance /= efficacy - curr;
	if (!monster) {
		chance /= mon->player->lev * 2 - total;
	}

	if (one_in_(chance)) {
		return increase_ability(mon, abil, true);
		if (!monster) {
			mon->player->learned_when[learn_i] = 0;
		}
	}

	return false;
}


/**
 * L: utilities for specific powers
 */

typedef void (power_mod_fn)(struct monster *, struct player_state *);

// agility power
int unarmoured_speed_bonus(struct monster *mon, struct player_state *s, int wgt)
{
	int wpen, bonus;

	wpen = wgt / 5 - get_power_scale_state(s, PP_AGILITY, 10);
	wpen = MAX(0, wpen);

	bonus = get_power_scale_state(s, PP_AGILITY, 10);
	bonus = MAX(0, bonus - wpen);

    s->speed += bonus;
	return bonus;
}

int unarmoured_ac_bonus(struct monster *mon, struct player_state *s, int wgt)
{
	int wpen, bonus;

	wpen = wgt - get_power_scale_state(s, PP_AGILITY, 250);
	wpen = MAX(0, wpen);
    bonus = get_power_scale_state(s, PP_AGILITY, 50);
	bonus = MAX(bonus / 2, bonus - wpen);

    s->to_a += bonus;
	return bonus;
}

static void calc_agility(struct monster *mon, struct player_state *s)
{
	unarmoured_speed_bonus(mon, s, s->armour_wgt);
	unarmoured_ac_bonus(mon, s, s->armour_wgt);
}

static int unlight_power_state(struct player_state *s, struct monster *mon)
{
	if (!cave) return 0;
	if (s->powers[PP_UNLIGHT] <= 0) return 0; 

	return -square_light(cave, mon->grid);
}

static int glow_power_state(struct player_state *s, struct monster *mon)
{
	if (!cave) return 0;
	if (s->powers[PP_GLOW] <= 0) return 0;

	return square_light(cave, mon->grid);
}

int unlight_power(struct monster *mon)
{
	return unlight_power_state(&mon->state, mon);
}

int glow_power(struct monster *mon)
{
	return glow_power_state(&mon->state, mon);
}

void calc_unlight(struct monster *mon, struct player_state *s)
{
	int power;

	if (!cave) return;
	if (s->powers[PP_UNLIGHT] < 0) return;
	power = -square_light(cave, mon->grid);

	s->el_info[ELEM_DARK].res_level += get_power_scale_state(s, PP_UNLIGHT, 3);

	adjust_skill_scale(&s->skills[SKILL_STEALTH], power, 25, 25);
	adjust_skill_scale(&s->skills[SKILL_SAVE], power, 25, 25);

	s->to_a += power * ABS(power);
}

void calc_glow(struct monster *mon, struct player_state *s)
{
	int power;

	if (!cave) return;
	if (s->powers[PP_GLOW] < 0) return;
	power = square_light(cave, mon->grid);

	s->el_info[ELEM_LIGHT].res_level += get_power_scale_state(s, PP_GLOW, 3);

	adjust_skill_scale(&s->skills[SKILL_SAVE], power, 15, 25);

	s->to_a += power * ABS(power);
}

void calc_running(struct monster *mon, struct player_state *s)
{
	s->num_moves += get_power_scale_state(s, PP_RUNNING, 10);
}

struct power_mod_datum {
	int power;
	power_mod_fn *func;
} power_mod_data[] = {
	{ PP_AGILITY, calc_agility },
	{ PP_UNLIGHT, calc_unlight },
	{ PP_GLOW, calc_glow },
	{ PP_RUNNING, calc_running }
};

void calc_power_effects_state(struct monster *mon, struct player_state *state)
{
	unsigned int i;

	for (i = 0; i < N_ELEMENTS(power_mod_data); ++i) {
		if (state->powers[power_mod_data[i].power]) {
			power_mod_data[i].func(mon, state);
		}
	}
}
