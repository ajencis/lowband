/**
 * \file player-util.c
 * \brief Player utility functions
 *
 * Copyright (c) 2011 The Angband Developers. See COPYING.
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
#include "cmd-core.h"
#include "effects.h"
#include "game-input.h"
#include "game-world.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "obj-chest.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-properties.h"
#include "player-quest.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "score.h"
#include "store.h"
#include "target.h"
#include "trap.h"
#include "ui-input.h"
#include "ui-player-properties.h"



int stat_max_max(struct player *p, int stat)
{
	assert(stat >= 0 && stat < STAT_MAX);

	int base = player->stat_max_max[stat];
	int bonus = lookup_player_monster(p)->stat_mod[stat];

	return base + bonus;
}


/**
 * L: unlock all classes that should be unlocked
 */
static bool unlock_classes(struct player *p)
{
	struct player_class *c;
	bool didunlock = false;

	assert(p->unlocked_classes);

	for (c = classes; c; c = c->next) {
		int power;

		assert(c->cidx < z_info->c_max);
		if (p->unlocked_classes[c->cidx]) {
			continue;
		}

		if (!c->unlockable) {
			p->unlocked_classes[c->cidx] = true;
			didunlock = true;
		}

		for (power = PP_NONE + 1; !p->unlocked_classes[c->cidx] && power < PP_MAX; ++power) {
			if (c->c_powers[power] > 0 && p->unlocked_tomes[power] > c->c_powers[power]) {
				p->unlocked_classes[c->cidx] = true;
				didunlock = true;
			}
		}
	}

	return didunlock;
}

static bool unlock_races(struct player *p)
{
	struct player_race *r;
	bool didunlock = false;

	assert(p->unlocked_races);

	for (r = races; r; r = r->next) {
		if (p->unlocked_races[r->ridx]) {
			continue;
		}

		if (!r->evol) {
			p->unlocked_races[r->ridx] = true;
			didunlock = true;
		}
	}

	return didunlock;
}

static bool unlock_tomes(struct player *p)
{
	bool didlearn = false;

	const struct player_ability *abil;

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->learn_index < 0) continue;
		bool ispower = abil->type == PY_ABIL_POWER;
		struct player_class *pc;

		for (pc = classes; pc; pc = pc->next) {
			if (!p->unlocked_classes[pc->cidx]) continue;

			if (ispower && pc->c_powers[abil->index] > p->unlocked_tomes[abil->learn_index]) {
				p->unlocked_tomes[abil->learn_index] = pc->c_powers[abil->index];
				didlearn = true;
			}

			if (!ispower && pc->c_skills[abil->index] > p->unlocked_tomes[abil->learn_index]) {
				p->unlocked_tomes[abil->learn_index] = pc->c_skills[abil->index];
				didlearn = true;
			}
		}
	}

	return didlearn;
}

/**
 * L: make sure at least one class is unlocked
 */
static bool guarantee_possible_class(struct player *p)
{
	struct player_class *c;
	for (c = classes; c; c = c->next) {
		if (p->unlocked_classes[c->cidx]) return false;
	}

	plog("Error: no classes unlocked!");
	p->unlocked_classes[0] = true;
	return true;
}

static bool guarantee_possible_race(struct player *p)
{
	struct player_race *r;
	for (r = races; r; r = r->next) {
		if (p->unlocked_races[r->ridx]) return false;
	}

	plog("Error: no races unlocked!");
	p->unlocked_races[0] = true;
	return true;
}

/**
 * L: unlock everything that should be unlocked
 */
bool unlock_all(struct player *p)
{
	if (!p->unlocked_classes) return false;

	bool didunlock = false;
	do {
		while (unlock_classes(p) || unlock_races(p) || unlock_tomes(p)) {
			didunlock = true;
		}
	} while (guarantee_possible_class(p) || guarantee_possible_race(p));

	return didunlock;
}

/**
 * Can the player unlock stuff currently?
 */
bool player_can_metaprogress(struct player *p)
{
	if (!OPT(p, birth_no_metaprogression)) {
		return true;
	}

	if (!p->unlocked_races[p->race->ridx]) {
		return false;
	}
	if (!p->unlocked_classes[p->class->cidx]) {
		return false;
	}

	return true;
}


/**
 * L: functions for players that are monsters
 */
struct monster_race *race_to_monster(const struct player_race *r)
{
	char name[80];
	struct monster_race *result;
	my_strcpy(name, r->name, sizeof name);
	my_struncap_full(name);
	result = lookup_monster(name);
	assert(result);
	return result;
}

struct monster_race *lookup_player_monster(const struct player *p)
{
	assert(p->mon.race);
	return p->mon.race;
}

static void change_player_body(struct player *p, struct player_body *new)
{
	char buf[80];
	int i;
	struct object *equipped_pile = NULL;
	struct object *equipped;

	// unequip all items, store them in equipped_pile
	if (p->mon.body.slots) {
		for (i = 0; i < p->mon.body.count; i++) {
			struct object *obj = p->mon.body.slots[i].obj;
			if (!obj) continue;

			bool anyleft;
			p->mon.body.slots[i].obj = NULL;
			p->upkeep->equip_cnt--;

			p->upkeep->update |= (PU_BONUS | PU_INVEN | PU_UPDATE_VIEW);
			p->upkeep->notice |= (PN_IGNORE);

			obj = gear_object_for_use(&p->mon, obj, obj->number, false, &anyleft);

			pile_insert(&equipped_pile, obj);
		}
	}

	assert(!p->upkeep->equip_cnt);

	// delete the player's body
	if (p->mon.body.slots) {
		for (i = 0; i < p->mon.body.count; i++) {
			string_free(p->mon.body.slots[i].name);
		}
		mem_free(p->mon.body.slots);
		p->mon.body.slots = NULL;
	}
	
	// remake the player's new body
	memcpy(&p->mon.body, new, sizeof(p->mon.body));
	my_strcpy(buf, new->name, sizeof(buf));
	p->mon.body.name = string_make(buf);
	p->mon.body.slots = mem_zalloc(p->mon.body.count * sizeof(struct equip_slot));
	for (i = 0; i < p->mon.body.count; i++) {
		p->mon.body.slots[i].type = new->slots[i].type;
		my_strcpy(buf, new->slots[i].name, sizeof(buf));
		p->mon.body.slots[i].name = string_make(buf);
	}

	// reequip the items or if we can't just put them in the inventory
	equipped = pile_last_item(equipped_pile);
	while (equipped) {
		pile_excise(&equipped_pile, equipped);
		int slot = wield_slot(&p->mon, equipped);
		if (slot >= 0 && !slot_object(&p->mon, slot)) {
			inven_carry(cave, &p->mon, equipped, false, false);
			inven_wield(cave, &p->mon, equipped, slot, false);
		}
		else {
			inven_carry(cave, &p->mon, equipped, true, false);
			combine_pack(&p->mon);
			pack_overflow(&p->mon, equipped);
		}
		equipped = pile_last_item(equipped_pile);
	}

	assert(!equipped_pile);
}

bool add_evolution(struct player *p, const struct monster_race *mr)
{
	++p->num_evol_choices;

	if (p->num_evol_choices > 1) {
		p->evol_choices = mem_realloc(p->evol_choices, sizeof *p->evol_choices * p->num_evol_choices);
	}
	else {
		p->evol_choices = mem_zalloc(sizeof *p->evol_choices * p->num_evol_choices);
	}
	
	assert(p->evol_choices);
	p->evol_choices[p->num_evol_choices - 1] = mr;

	return true;
}

static void remove_evolution(struct player *p, int which)
{
	assert(which < p->num_evol_choices && which >= 0);
	assert(p->evol_choices);

	const struct monster_race **old_evol_choices = p->evol_choices;
	int old_i, new_i;

	--p->num_evol_choices;

	if (p->num_evol_choices <= 0) {
		p->evol_choices = NULL;
	}
	else {
		p->evol_choices = mem_zalloc(sizeof *p->evol_choices * p->num_evol_choices);

		for (old_i = 0, new_i = 0; new_i < p->num_evol_choices; ++old_i) {
			if (old_i != which) {
				assert(new_i < p->num_evol_choices);
				p->evol_choices[new_i] = old_evol_choices[old_i];
				++new_i;
			}
		}
	}

	mem_free(old_evol_choices);
}

void remove_first_evolution(struct player *p)
{
	if (p->num_evol_choices > 0) {
		remove_evolution(p, 0);
	}
}

void remove_last_evolution(struct player *p)
{
	if (p->num_evol_choices > 0) {
		remove_evolution(p, p->num_evol_choices - 1);
	}
}

void change_player_monster(struct player *p, const struct monster_race *mon, bool init)
{
	int i;

	assert(mon);
	if (!init) {
		disturb(p);
		msg("You transform into a%s %s.", is_a_vowel(mon->name[0]) ? "n" : "", mon->name);
	}

	if (!init && mon->body && !streq(mon->body->name, p->mon.body.name)) {
		change_player_body(p, mon->body);
	}

	if (!p->mon.race) {
		p->mon.race = mem_zalloc(sizeof *p->mon.race);
	}

	memcpy(p->mon.race, mon, sizeof *p->mon.race);
	rearrange_monster(p->mon.race, true);

	for (i = STAT_NONE + 1; i < STAT_MAX; ++i) {
		p->stat_max[i] = MIN(p->stat_max[i], stat_max_max(p, i));
		p->stat_cur[i] = MIN(p->stat_cur[i], p->stat_max[i]);
	}

	player->upkeep->redraw |= (PR_MAP | PR_MISC);
	player->upkeep->update |= (PU_BONUS | PU_HP);
}

bool check_player_monster(struct player *p, bool init)
{
	const struct monster_race *selected = NULL;
	int numevols = 0;
	//struct evolution *e = curr ? curr->evol : p->race->evol;
	bool do_change = false;
	uint32_t xpneed;
	uint32_t currxp = init ? 0 : p->monster_xp;

	if (p->num_evol_choices <= 0 && !init) {
		select_evolution(p);
	}

	if (!p->evol_choices) return false;
	if (init && p->mon.race) return false;
	//assert(p->evol_choices);

	selected = p->evol_choices[0];

	if (selected) {
		int monlev = selected->level;

		++numevols;

		if (monlev < PY_MAX_LEVEL) {
			// monster is in the table
			xpneed = player_exp[monlev];
		}
		else if (player_exp[PY_MAX_LEVEL - 1] / PY_MAX_LEVEL < PY_MAX_EXP / (unsigned)monlev) {
			/* monster is out of the table but linear scaling of the highest value
			   in the table is less than the maximum possible */
			xpneed = player_exp[PY_MAX_LEVEL - 1] / PY_MAX_LEVEL * monlev;
		}
		else {
			/* monster is out of the table and would need more than the max possible
			   xp to choose */
			xpneed = PY_MAX_EXP;
		}
	}

	if (currxp >= xpneed) do_change = true;

	if (do_change) {
		change_player_monster(p, selected, init);
		if (!init) {
			remove_first_evolution(p);
			player_increase_stat(p);
		}
	}

	return do_change;
}

void player_race_name(struct player *p, char *buf, size_t bufsize)
{
	struct monster_race *mon = lookup_player_monster(p);

	if (!mon || !character_generated) {
		my_strcpy(buf, p->race->name, bufsize);
		return;
	}

	const char *src = mon->name;
	size_t buflen = bufsize / sizeof(buf[0]);

	if ((strlen(mon->name) > buflen - 1) && mon->short_name) {
		src = mon->short_name;
	}

	my_strcpy(buf, src, bufsize);
	my_strcap_full(buf);
}

bool player_increase_stat(struct player *p)
{
	int num = 0;
	int backup_num = 0;
	int choice = -1;
	int backup_choice = -1;
	int i;
	bool dummy = false;
	for (i = 0; i < STAT_MAX; i++) {
		if (p->stat_max[i] < stat_max_max(p, i)) {
			++num;
			if (one_in_(num)) {
				choice = i;
			}
		}
		if (p->stat_cur[i] < stat_max_max(p, i)) {
			++backup_num;
			if (one_in_(backup_num)) {
				backup_choice = i;
			}
		}
	}

	if (num) {
		p->stat_cur[choice] = p->stat_max[choice];
		effect_simple(EF_GAIN_STAT, source_player(), NULL, choice, 0, 0, 0, 0, &dummy);
		return true;
	}
	else if (backup_num) {
		effect_simple(EF_RESTORE_STAT, source_player(), NULL, backup_choice, 0, 0, 0, 0, &dummy);
	}
	return false;
}

/**
 * L: gets the next evolution selection for the player
 * returns true if an evolution was added to the player's evolution queue
 */
bool select_evolution(struct player *p)
{
	struct evolution *choice_evol;
	const struct monster_race *select;

	if (p->evol_choices) choice_evol = p->evol_choices[p->num_evol_choices - 1]->evol;
	else choice_evol = p->mon.race->evol;

	if (!choice_evol) return false;

	if (!choice_evol->next) {
		add_evolution(p, choice_evol->race);
		return true;
	}

	select = evolution_choice_menu_select(choice_evol, false);

	if (!select) return false;

	add_evolution(p, select);

	return true;
}

int expected_monster_evol_level(const struct monster_race *mr)
{
	const struct evolution *evol;
	int sum = 0, div = 0;

	for (evol = mr->evol; evol; evol = evol->next) {
		sum += expected_monster_evol_level(evol->race);
		div += 1;
	}

	if (div > 0) return sum / div;
	return mr->level;
}

int expected_max_evol_level(const struct player *p)
{
	const struct monster_race *curr;

	curr = p->evol_choices ? p->evol_choices[0] : lookup_player_monster(p);

	return expected_monster_evol_level(curr);
}


int get_power_scale_state(const struct player_state *ps, int power, int scaleto, int level)
{
	assert(power > 0 && power < PP_MAX);

	int powerlev = ps->powers[power], result;
	bool negate = false;

	if (powerlev <= 0) return 0;
	if (powerlev > level) {
		powerlev = (powerlev - level) / 2 + level;
	}

	powerlev = MAX(powerlev, 0);
	if (scaleto < 0) {
		scaleto = -scaleto;
		negate = true;
	}

	result = (powerlev * scaleto + 50 * 2 / 3) / 50;

	return negate ? -result : result;
}

int get_power_scale(const struct player *p, int power, int scaleto)
{
	return get_power_scale_state(&p->mon.state, power, scaleto, p->lev);
}


const char *lookup_power_name(int power)
{
	assert(power > PP_NONE && power < PP_MAX);

	struct player_ability *abil = lookup_player_ability(power, PY_ABIL_POWER);

	return abil->name;
}



static double btc_scale(int bonus)
{

	if (bonus <= 0) return 0;
	assert(bonus * bonus < INT_MAX / bonus);
	int result = my_int_sqrt(bonus * bonus * bonus);
	return result;
}

static int bonus_to_cost_base(int bonus, int factor)
{
	int scaleto = 10;
	static double scalefrom = -1;
	if (scalefrom == -1) scalefrom = btc_scale(50);
	int result = (int)((btc_scale(bonus) * scaleto * factor + 10 * scalefrom - 1)  / 10 / scalefrom);
	
	return result;
}

static int bonus_to_cost(int bonus, const struct player_ability *abil)
{
	if (abil->learn_index < 0) return 0;
	return bonus_to_cost_base(bonus, abil->cost);
}

int player_bonus_to_cost(int bonus, const struct player_ability *abil, struct player *p)
{
	assert(abil);
	int base = bonus_to_cost(bonus, abil);
	int discount = 0; // in percent

	if ((of_has(p->class->pflags, PF_EXTRA_LEARNING) || 
				of_has(p->race->pflags, PF_EXTRA_LEARNING)) &&
			abil->type != PY_ABIL_POWER) {
		discount += 35;
	}

	base = (base * (100 - discount) + 99) / 100;

	return base;
}

static int cost_to_bonus_base(int cost, int factor)
{
	int i;
	for (i = 0; i < 100; i++) {
		int tcost = bonus_to_cost_base(i, factor);
		if (tcost > cost) return i - 1;
	}
	return i;
}

static int cost_to_bonus(int cost, const struct player_ability *abil)
{
	if (abil->learn_index < 0) return 0;
	int factor = abil->cost;
	return cost_to_bonus_base(cost, factor);
}

static int tome_max_skill(const struct object *obj)
{
	int result = 0;
	if (obj) {
		if (of_has(obj->flags, OF_POWER_LEARN_5)) result = cost_to_bonus_base(10, 100);
		if (of_has(obj->flags, OF_POWER_LEARN_4)) result = cost_to_bonus_base(8, 100);
		if (of_has(obj->flags, OF_POWER_LEARN_3)) result = cost_to_bonus_base(6, 100);
		if (of_has(obj->flags, OF_POWER_LEARN_2)) result = cost_to_bonus_base(4, 100);
		if (of_has(obj->flags, OF_POWER_LEARN_1)) result = cost_to_bonus_base(2, 100);
	}

	return result;
}

uint16_t calc_extra_points_array(struct player *p, uint16_t *extra_abil)
{
	uint16_t sum = 0;
	const struct player_ability *abil;

	assert(player_abilities);
	assert(extra_abil);

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->learn_index < 0) continue;
		sum += player_bonus_to_cost(extra_abil[abil->learn_index], abil, p);
	}

	return sum;
}

void calc_extra_points(struct player *p, struct player_state *ps)
{
	if (!character_generated) {
		ps->extra_points_max = 0;
		ps->extra_points_used = 0;
	}
	ps->extra_points_used = calc_extra_points_array(p, p->extra_target);
	/*if (OPT(p, birth_level_one_learn)) {
		if (!character_generated) ps->extra_points_max = LEARN_MAX;
		else ps->extra_points_max = 0;
		return;
	}*/
	assert(p->extra_target);
	int sum = calc_extra_points_array(p, p->extra_target);
	int intbonus = adj_int_tome(ps->stat_ind[STAT_INT]);
	int mx = 0;
	int i;
	int start = pf_has(ps->pflags, PF_EXTRA_LEARNING) ? 0 : 5;
	int maxlearn = LEARN_MAX + intbonus;

	for (i = start; i <= p->lev && mx < LEARN_MAX; i += 5) {
		mx = MIN(mx + 10, maxlearn);
	}

	ps->extra_points_max = MAX(mx, sum);
	ps->extra_points_used = sum;
}

#if 0
static bool player_can_learn_from_tome(struct player *p, int index)
{
	int cpwr;
	char name[80];
	assert(index > TOME_NONE && index < TOME_MAX);

	if (index < PP_MAX) {
		cpwr = p->extra_powers[index];
		my_strcpy(name, player_powers[index].name, sizeof(name));
	}
	else if (index < PP_MAX + SKILL_MAX) {
		cpwr = p->extra_skills[index - PP_MAX];
		my_strcpy(name, skill_index_to_name(index - PP_MAX), sizeof(name));
		my_strcap_full(name);
	}
	
	int currcost = player_bonus_to_cost(cpwr, index, p);
	int nextcost = player_bonus_to_cost(cpwr + 1, index, p);

	// if we're not spending any points to learn then learn
	if (nextcost <= currcost) return true;
	// if we don't have any points left then we can't
	if (p->state.extra_points_max <= p->state.extra_points_used) return false;

	// ask the player if they're willing to spend points
	if (!get_forced_check(format("Learn %s? ", name))) {
		p->checked_tome_this_expedition = true;
		return false;
	}

	return true;
}
#endif

bool learn_realm(struct player *p, const struct magic_realm *realm)
{
	if (p->realm) return false;

	if (!get_forced_check(format("Learn %s magic? ", realm->name))) return false;

	p->realm = realm;

	msg("You feel that you understand %s magic.", realm->name);

	if (p->realm->realm_special[RLM_SPCL_INNATE]) {
		player_learn_spell_xp(p, true, 0);
	}

	p->upkeep->update |= PU_BONUS;

	return true;
}

bool learn_extra(struct player *p, const struct player_ability *abil)
{
	//if (!player_can_learn_from_tome(p, index)) return false;

	if (abil->type == PY_ABIL_POWER) {
		p->extra_powers[abil->index]++;
			
		// tell the player when they've learned something
		msg("You feel a bit more familiar with %s.", abil->name);
		
		//p->upkeep->update |= player_powers[abil->index].update;
	}
	else if (abil->type == PY_ABIL_SKILL) {
		p->extra_skills[abil->index]++;

		// tell the player when they've learned something
		char buf[80];
		my_strcpy(buf, abil->name,/*skill_index_to_name(abil->index),*/ sizeof(buf));
		my_strcap_full(buf);
		msg("You feel a bit more familiar with %s.", buf);
	}

	p->upkeep->update |= PU_BONUS;
	p->upkeep->redraw |= PR_STATUS;

	disturb(p);

	return true;
}

const struct player_ability *player_ability_by_learn_index(int learn_index)
{
	assert(learn_index < z_info->learn_max);
	const struct player_ability *abil;

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->learn_index == learn_index) return abil;
	}

	return NULL;
}

bool obj_can_learn_extra_from(const struct object *obj)
{
	int maxs = tome_max_skill(obj);
	int power = obj->pval;
	const struct player_ability *abil;

	if (of_has(obj->flags, OF_REALM_LEARN)) {
		return false;
	}

	if (maxs <= 0) return false;

	abil = player_ability_by_learn_index(power);

	//if (maxs <= 0) return false;
	//if (power <= TOME_NONE || power >= TOME_MAX) return false;

	if (abil->type == PY_ABIL_POWER) {
		if (player->extra_powers[abil->index] >= maxs) return false;
	}
	else {
		if (player->extra_skills[abil->index] >= maxs) return false;
	}
	return true;
}

#if 0
static bool check_learn_skill(struct player *p, int skill, int xpgain)
{
	assert(skill < SKILL_MAX);
	uint32_t chance = p->state.skills[skill];

	chance *= chance;
	chance *= p->extra_skills[skill];

	if (pf_has(p->state.flags, PF_EXTRA_LEARNING)) chance /= 5;
	chance /= xpgain;
	chance = MIN(chance, 0x10000000U);

	if (one_in_(chance)) {
		return learn_extra(p, skill + PP_MAX);
	}

	return false;
}

static bool check_learn_power(struct player *p, int power, int xpgain)
{
	assert(power < PP_MAX);
	uint32_t chance;

	chance = p->state.powers[power] + 10;

	chance *= chance;
	chance *= p->extra_powers[power];

	if (pf_has(p->state.flags, PF_EXTRA_LEARNING)) chance /= 2;
	chance /= xpgain;
	chance = MIN(chance, 0x10000000U);

	if (one_in_(chance)) {
		return learn_extra(p, power);
	}

	return false;
}
#endif

#if 0
static bool learn_from_tome(struct player *p, struct object *obj, int xpgain)
{
	if (!obj) return false;
	if (obj->number < 1) return false;
	if (!obj_can_learn_extra_from(obj)) return false;
	if (xpgain <= 0) return false;

	int power = obj->pval, currcost, nextcost;
	uint16_t currlearned;
	bool learned = false;
	uint32_t chance; // one_in_(chance) to learn
	int mx = tome_max_skill(obj);
	const struct magic_realm *realm;

	if (of_has(obj->flags, OF_REALM_LEARN)) {
		realm = realm_by_index(obj->pval);
		assert(realm);
		if (!p->checked_tome_this_expedition && one_in_(p->depth + 5)) {
			p->checked_tome_this_expedition = true;
			return learn_realm(p, realm);
		}
		return false;
	}
	
	if (power < PP_MAX) {
		if (!check_learn_power(p, power, xpgain)) return false;

		if (player_bonus_to_cost(p->extra_powers[power], power, p) >= mx) {
			char buf[80];
			object_desc(buf, sizeof(buf), obj, ODESC_EXTRA, p);
			// after learning we're at the max
			msg("You feel you've learned everything you can from your %s.", buf);
		}
		return true;
	}
	else if (power < PP_MAX + SKILL_MAX) {
		int skill_ind = power - PP_MAX;
		if (!check_learn_skill(p, skill_ind, xpgain)) return false;
		
		if (player_bonus_to_cost(p->extra_skills[skill_ind], power, p) >= mx) {
			char buf[80];
			object_desc(buf, sizeof(buf), obj, ODESC_EXTRA, p);
			// after learning we're at the max
			msg("You feel you've learned everything you can from your %s.", buf);
		}
		return true;
	}

	return false;

	currcost = player_bonus_to_cost(currlearned, power, p);
	nextcost = player_bonus_to_cost(currlearned + 1, power, p);

	// higher-level tomes are more complicated
	chance *= mx;
	// harder to learn the more you know
	chance *= (currlearned + 10);
	chance /= xpgain;
	// easier to learn if you have more info
	chance /= obj->number * obj->number * 100;
	// bonus for skillmasters
	if (pf_has(p->state.pflags, PF_EXTRA_LEARNING)) chance /= 5;
	// paranoia
	chance = MIN(chance, 0x10000000U);

	if (one_in_(chance) && (!p->checked_tome_this_expedition || nextcost <= currcost)) {
		learned = learn_extra(p, power);
		if (learned && nextcost >= mx) {
			char buf[80];
			object_desc(buf, sizeof(buf), obj, ODESC_EXTRA, p);
			// after learning we're at the max
			msg("You feel you've learned everything you can from your %s.", buf);
		}
	}

	return learned;
}
#endif

static int tome_max_learnable_parents_array(const struct player_ability *abil, int *powers_array, int *skills_array)
{
	int div = 0, sum = 0;
	int i;
	for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
		const struct player_ability *prnt = abil->parent[i];
		if (prnt) {
			if (prnt->type == PY_ABIL_POWER) {
				sum += powers_array[prnt->index];
				div += 50;
			} else if (prnt->type == PY_ABIL_SKILL) {
				sum += skills_array[prnt->index] / 2;
				div += 50;
			}
		}
	}

	if (div > 0) {
		// need parents to be at ~50/4 before you can learn
		// learnable at max by ~50*3/4
		int abil_max_cost = bonus_to_cost(LEARN_MAX, abil);
		int max_cost = abil_max_cost * sum * 2 / div - abil_max_cost / 4;
		max_cost = MAX(0, MIN(abil_max_cost, max_cost));

		return cost_to_bonus(max_cost, abil);
	}

	return LEARN_MAX;
}

static int tome_max_learnable_parents(const struct player_ability *abil, struct player *p)
{
	return tome_max_learnable_parents_array(abil, p->mon.state.powers, p->mon.state.skills);
}

static int player_extra_target(struct player *p, const struct player_ability *abil)
{
	int base = p->extra_target[abil->learn_index];
	int max = tome_max_learnable_parents(abil, p);

	return MIN(base, max);
}

bool check_learn_powers(struct player *p, int xpgain)
{
	bool learned = false;
	const struct player_ability *abil;

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->learn_index < 0) continue;
		int curr_total, curr_lrnd;
		int target = player_extra_target(p, abil);
		unsigned int chance;

		if (OPT(p, birth_level_one_learn)) {
			target = (target * p->lev + PY_MAX_LEVEL - 1) / PY_MAX_LEVEL;
		}

		if (abil->type == PY_ABIL_POWER) {
			curr_total = p->mon.state.powers[abil->index];
			curr_lrnd = p->extra_powers[abil->index];
		}
		else {
			curr_total = p->mon.state.skills[abil->index];
			curr_lrnd = p->extra_skills[abil->index];
		}

		if (target <= curr_lrnd) continue;

		chance = 1;

		chance *= curr_total + 10;
		chance *= curr_lrnd + 10;
		chance *= cave->depth;

		chance /= xpgain;
		chance /= my_int_sqrt(target - curr_lrnd);

		if (one_in_(chance)) {
			learned = true;
			learn_extra(p, abil);
		}
	}

	return learned;
}
#if 0
	int i;
	struct object *obj;
	bool learned = false;
	if (xpgain <= 0) return false;

	for (i = 0; i < SKILL_MAX; ++i) {
		int currcost = player_bonus_to_cost(p->extra_skills[i], i + PP_MAX, p);
		int nextcost = player_bonus_to_cost(p->extra_skills[i] + 1, i + PP_MAX, p);

		if (currcost >= nextcost && check_learn_skill(p, i, xpgain)) return true;
	}

	for (i = PP_NONE + 1; i < PP_MAX; ++i) {

		int currcost = player_bonus_to_cost(p->extra_powers[i], i, p);
		int nextcost = player_bonus_to_cost(p->extra_powers[i] + 1, i, p);

		if (currcost >= nextcost && check_learn_power(p, i, xpgain)) return true;
	}

	int maxtomes = p->mon.body.count + z_info->pack_size;
	struct object **tomes = mem_zalloc((maxtomes) * sizeof(*tomes));
	int tind = 0;

	// collect tomes from inventory
	for (obj = p->mon.gear; obj && (tind < maxtomes); obj = obj->next) {
		if (obj_can_learn_extra_from(obj)) {
			tomes[tind] = obj;
			++tind;
		}
	}

	// collect tomes from equipment
	for (i = 0; (i < p->mon.body.count) && (tind < maxtomes); i++) {
		obj = p->mon.body.slots[i].obj;
		if (obj && obj_can_learn_extra_from(obj)) {
			tomes[tind] = obj;
			++tind;
		}
	}

	if (tind > 0) {
		// if we've found something try to learn a couple times
		int choice = randint0(tind);
		learned = learn_from_tome(p, tomes[choice], xpgain);
	}

	mem_free(tomes);

	return learned;
#endif

/**
 * returns  NONE  if there is none
 */
const struct player_ability *tome_parent(const struct player_ability *abil)
{
	return abil->parent[0];
	/*assert(tome_ind < TOME_MAX && tome_ind > TOME_NONE);
	int result = tome_parents[tome_ind];
	assert(result < TOME_MAX && result >= TOME_NONE);
	return tome_parents[tome_ind];*/
}

#if 0
/**
 * If a skill/power is a subpower of another skill/power what is the maximal target for the
 * former given a particular value for the latter?
 */
static int tome_max_cost_parent(int parent_level, struct player_ability *abil, struct player_ability *parent)
{
	int high_power = parent->type == PY_ABIL_POWER ? 50 : 100;

	int tome_max_cost = bonus_to_cost(LEARN_MAX, abil);

	int max_cost = tome_max_cost * (parent_level * 3 / 2 - high_power / 3) / high_power;

	return max_cost;
}
#endif

static void max_learnable_object(struct object *obj, int *learn_array, int array_max) 
{
	int max_learn = tome_max_skill(obj);
	if (max_learn) {
		int tome_ind = obj->pval;
		assert(tome_ind < array_max);

		learn_array[tome_ind] = MAX(learn_array[tome_ind], max_learn);
	}
}

bool tome_max_learnable_extra_array(bool metaprog, int *learn_array, int *extra_array,
	int *curr_powers, int *curr_skills, struct player *p)
{
	memset(learn_array, 0, z_info->learn_max * sizeof (*learn_array));

	struct object *obj;
	const struct player_ability *abil;
	bool extra = false;
	int i;

	if (!metaprog || !p) {
		for (i = 0; i < z_info->learn_max; ++i) {
			learn_array[i] = LEARN_MAX;
		}
	} else {
		for (obj = p->mon.gear; obj; obj = obj->next) {
			max_learnable_object(obj, learn_array, z_info->learn_max);
		}

		for (i = 0; i < z_info->learn_max; ++i) {
			learn_array[i] = MAX(learn_array[i], p->unlocked_tomes[i]);
		}
	}

	if (extra_array) {
		for (i = 0; i < z_info->learn_max; ++i) {
			if (extra_array[i] > learn_array[i]) {
				learn_array[i] = extra_array[i];
				extra = true;
			}
		}
	}

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->learn_index < 0) continue;
		int tome_parent_max = tome_max_learnable_parents_array(abil, curr_powers, curr_skills);
		learn_array[abil->learn_index] = MIN(learn_array[abil->learn_index], tome_parent_max);
		/*struct player_ability *parent = tome_parent(abil);
		if (parent) {
			int curr_learned = abil->type == PY_ABIL_POWER ? p->state.powers[abil->index] : p->state.skills[abil->index];
			int tome_parent_max = tome_max_learnable_parent(curr_learned, abil, parent);
			if (learn_array[abil->learn_index] > tome_parent_max) {
				learn_array[abil->learn_index] = tome_parent_max;
			}
		}*/
	}

	{
		abil = lookup_player_ability(PP_DIVINATION_MAGIC, PY_ABIL_POWER);
		assert(abil);
	}

	return extra;
}

bool tome_max_learnable_extra(struct player *p, int *learn_array, int *extra_array)
{
	return tome_max_learnable_extra_array(!OPT(p, birth_no_metaprogression), learn_array, extra_array,
			p->mon.state.powers, p->mon.state.skills, p);
}

void tome_max_learnable(struct player *p, int *learn_array)
{
	tome_max_learnable_extra(p, learn_array, NULL);
}

int tome_next_increment(struct player *p, const struct player_ability *abil, int curr_bonus)
{
	int curr_cost = player_bonus_to_cost(curr_bonus, abil, p);
	int n_cost, nn_cost;
	int next_bonus;

	n_cost = player_bonus_to_cost(curr_bonus, abil, p);
	for (next_bonus = curr_bonus; next_bonus < 50; ++next_bonus) {
		nn_cost = player_bonus_to_cost(next_bonus + 1, abil, p);
		// we want the max power at the target cost, so keep going until we're about to go past target
		// also make sure we have increased the cost just in case one bonus increase increases costs by > 1
		if ((n_cost > curr_cost) && (nn_cost > n_cost)) return next_bonus;

		n_cost = nn_cost;
	}

	return next_bonus;
}

int tome_prev_increment(struct player *p, const struct player_ability *abil, int curr_bonus)
{
	int curr_cost = player_bonus_to_cost(curr_bonus, abil, p);
	int p_cost;
	int prev_bonus;

	for (prev_bonus = curr_bonus; prev_bonus > 0; --prev_bonus) {
		p_cost = player_bonus_to_cost(prev_bonus, abil, p);

		if (p_cost < curr_cost) return prev_bonus;
	}

	return prev_bonus;
}

int player_class_power_array(const struct player_class *c, int extra_power, int power)
{
	assert(power >= 0 && power < PP_MAX);
	int base = c->c_powers[power];
	// extra-learning makes class reflect learned powers
	if (pf_has(c->pflags, PF_EXTRA_LEARNING)) {
		base = MAX(base, extra_power);
	}
	return base;
}

int player_class_power(struct player *p, int power)
{
	return player_class_power_array(p->class, p->extra_powers[power], power);
}

int player_race_power_array(const struct player_race *r, int extra_power, int power)
{
	assert(power >= 0 && power < PP_MAX);
	int base = r->r_powers[power];
	// extra-learning makes race reflect learned powers
	if (pf_has(r->pflags, PF_EXTRA_LEARNING)) {
		base = MAX(base, extra_power / 2);
	}
	return base;
}

int player_race_power(struct player *p, int power)
{
	return player_race_power_array(p->race, p->extra_powers[power], power);
}

int class_x_skill(const struct player_class *c, int extra, int skill)
{
	assert(skill >= 0 && skill < SKILL_MAX);
	int xtra = c->x_skills[skill];
	// extra-learning makes class reflect known skills
	if (pf_has(c->pflags, PF_EXTRA_LEARNING)) {
		xtra = MAX(xtra, extra * 3 / 4);
	}
	return xtra;
}

int player_class_x_skill(struct player *p, int skill)
{
	return class_x_skill(p->class, p->extra_skills[skill], skill);
}


int class_c_skill(const struct player_class *c, int extra, int skill)
{
	assert(skill >= 0 && skill < SKILL_MAX);
	int base = c->c_skills[skill];
	// extra-learning makes class reflect known skills
	if (pf_has(c->pflags, PF_EXTRA_LEARNING)) {
		base = MAX(base, extra * 1 / 4);
	}
	return base;
}

int player_class_c_skill(struct player *p, int skill)
{
	return class_c_skill(p->class, p->extra_skills[skill], skill);
}

void player_race_r_skill(const struct monster_race *r, bool evolved, int skills[SKILL_MAX])
{
	int i;
	for (i = 0; i < SKILL_MAX; i++) {
		skills[i] = r->skills[i];
	}
	// juvenile monsters get the bonuses of their evolved forms
	if (r->level == 0 && r->evol) {
		for (i = 0; i < SKILL_MAX; i++) {
			int bonus = 25;
			struct evolution *evol;
			for (evol = r->evol; evol; evol = evol->next) {
				bonus = MIN(evol->race->skills[i], bonus);
			}
			skills[i] += bonus;
		}
	}
}

void player_race_x_skill(const struct monster_race *r, bool evolved, int skills[SKILL_MAX])
{
	player_race_r_skill(r, evolved, skills);
}

void player_race_elem_info(const struct player_race *r, bool evolved, struct element_info el_info[ELEM_MAX])
{
	int i;
	//struct monster_race *mr = race_to_monster(r);

	for (i = 0; i < ELEM_MAX; i++) {
		el_info[i].res_level = r->el_info[i].res_level;
	}

	if (!evolved && r->evol) {
		bool evol_does_resist[ELEM_MAX] = { false };
		for (i = 0; i < ELEM_MAX; i++) {
			struct evolution *e;
			int evol_resist = 1;
			evol_does_resist[i] = true;
			for (e = r->evol; e; e = e->next) {
				evol_resist = MIN(evol_resist, e->race->el_info[i].res_level);
				/*if (!rf_has(e->race->flags, elem_matches[i].mval)) {
					evol_does_resist[elem] = false;
				}*/
			}
		}
		for (i = 0; i < ELEM_MAX; ++i) {
			if (evol_does_resist[i]) {
				el_info[i].res_level = MAX(el_info[i].res_level, 1);
			}
		}
	}
}

void skill_stat(const struct magic_realm *realm, const int indices[STAT_MAX], int skill, int *stat1, int *stat2)
{
	int primary_stat, secondary_stat, primary_index, secondary_index;

	*stat1 = STAT_NONE;
	*stat2 = STAT_NONE;

	if (skill == SKILL_MAGIC && realm) {
		primary_stat = realm->stat;
	}
	else {
		primary_stat = skill_stats[skill].primary_stat;
	}
	secondary_stat = skill_stats[skill].secondary_stat;

	primary_index = primary_stat == STAT_NONE ? -1 : indices[primary_stat];
	secondary_index = secondary_stat == STAT_NONE ? -1 : indices[secondary_stat];

	if (primary_index < 0 && secondary_index < 0) {
		return;
	} else if (secondary_index < 0) {
		*stat1 = secondary_stat;
	} else if (primary_index < 0) {
		*stat1 = primary_stat;
	}
	else if (secondary_index < primary_index) {
		*stat1 = primary_stat;
	} else {
		*stat1 = primary_stat;
		*stat2 = secondary_stat;
	}
}

void player_skill_stats(struct player *p, struct player_state *ps, int skill, int *stat1, int *stat2)
{
	skill_stat(get_player_realm(p), p->mon.state.stat_ind, skill, stat1, stat2);
}

/**
 * L: the stat used is either the primary stat or the average of the
 * primary and secondary stats, whichever is higher
 * returns the primary stat ind if there is no secondary stat and vice versa
 * returns -1 if there are no appropriate stats at all
 */
int skill_stat_ind(const struct magic_realm *realm, const int indices[STAT_MAX], int skill)
{
	int stat1, stat2;
	skill_stat(realm, indices, skill, &stat1, &stat2);
	//player_skill_stats(p, ps, skill, &stat1, &stat2);

	if (stat1 != STAT_NONE && stat2 != STAT_NONE) {
		return (indices[stat1] + indices[stat2]) / 2;
	}
	else if (stat1 != STAT_NONE) {
		return indices[stat1];
	}
	else if (stat2 != STAT_NONE) {
		return indices[stat2];
	}
	return -1;
}

int player_skill_stat_ind(const struct player *p, const struct player_state *ps, int skill)
{
	return skill_stat_ind(get_player_realm(p), ps->stat_ind, skill);
}

/**
 * L: upon gaining xp, consider adding spells to those known
 * clericy casters don't use spellbooks, they get granted spells by their god
 * we can remove spells to make room for the new spells, but we will only remove spells
 * that the caster hasn't cast yet
 */
bool player_learn_spell_xp(struct player *p, bool initial, int xp)
{
	int i, j, currnum = 0, maxnum = 0; // current number of forgettable spells, maximum number of forgettable spells
	struct player_spell *ps;
	int learned_num = 0;
	int forgotten[3] = { -1, -1, -1 }; // track which ones we forgot so we don't relearn them
	int forgottenind = 0;
	const struct magic_realm *realm = get_player_realm(p);

	// only some casters learn spells this way
	if (!realm || !realm->realm_special[RLM_SPCL_INNATE]) {
		return false;
	}

	// don't get spells until skill 3
	if (p->mon.state.skills[SKILL_MAGIC] < 3) {
		return false;
	}

	if (!initial) {
		if (xp <= 0) {
			return false;
		}

		int freq = p->mon.state.skills[SKILL_MAGIC];
		freq = MIN(turn * 13 / z_info->day_length / 10, freq);
		freq = MAX(freq, 3);
		freq = freq * freq / xp;

		// don't change spells too often
		if (!one_in_(freq)) {
			return false;
		}
	}

	// count number of spells we already have
	for (i = 0; i < z_info->spell_max; ++i) {
		if ((p->player_spell_flags[i] & PY_SPELL_LEARNED) &&
				!(p->player_spell_flags[i] & PY_SPELL_WORKED)) {
			++currnum;
		}
	}

	// always give 3 spells at birth
	if (initial) {
		maxnum = 3;
		maxnum = MIN(maxnum, p->upkeep->new_spells);
	} else {
		int maxnum2 = randint1(3);
		maxnum = randint1(3);
		maxnum = MAX(maxnum, maxnum2);
		maxnum = MIN(maxnum, p->upkeep->new_spells);
	}

	// while we haven't forgotten enough
	for (forgottenind = 0; currnum >= maxnum && forgottenind < 3; --currnum, ++forgottenind) {
		int total_so_far = 0;

		// check every spell
		for (i = 0; i < z_info->spell_max; ++i) {

			// only forget spells that we know but haven't cast yet
			if ((p->player_spell_flags[i] & PY_SPELL_LEARNED) &&
					!(p->player_spell_flags[i] & PY_SPELL_WORKED)) {

				// even chance for all spells
				if (one_in_(currnum - total_so_far)) {
					forgotten[forgottenind] = i;
					p->player_spell_flags[i] &= (~PY_SPELL_LEARNED);
					--currnum;
					break;
				}
				++total_so_far;
			}
		}
	}

	while (currnum < maxnum) {
		struct player_spell *choice = NULL;
		int power_total = 0;

		for (ps = spells; ps; ps = ps->next) {
			int power = gener_spell_power(p, ps);

			if (ps->smana > p->msp) continue;

			// learn only spells we can cast at a reasonable level
			if (power > 5 || (initial && power > 0)) {
				power += ps->slevel / 2;
				bool skip = false;

				// skip spells we just forgot
				for (j = 0; j < 3; ++j) {
					if (forgotten[j] == ps->sidx) {
						skip = true;
					}
				}

				if (!skip) {
					power_total += power;
					if (randint0(power_total) < power) {
						choice = ps;
					}
				}
			}
		}

		if (!choice) break;

		// actually learn it
		gener_spell_learn(p, choice, false);
		++learned_num;
		++currnum;
	}

	if (character_generated && learned_num) {
		msg(learned_num == 1 ? "You feel a new spell in your mind." : "You feel new spells in your mind.");
		p->upkeep->update |= PU_SPELLS;
	}

	return learned_num ? true : false;
}


int antimagic_fail_increase(struct player *p)
{
	return get_power_scale(p, PP_ANTIMAGIC, 75);
}

int antimagic_radius(struct player *p)
{
	if (p->mon.state.powers[PP_ANTIMAGIC] <= 0) return 0;
	return get_power_scale(p, PP_ANTIMAGIC, 3) + 2;
}


/**
 * L: unlight players like to be in the dark
 * scales up to UNLIGHT_MAX_POWER
 */
int unlight_power_state(struct player_state *ps, struct player *p)
{
	if (!cave || !character_dungeon) return 0;
	if (ps->powers[PP_UNLIGHT] <= 0) return 0;
	int bonus = -square_light(cave, p->mon.grid);
	int malus = get_power_scale_state(ps, PP_UNLIGHT, UNLIGHT_MAX_POWER, p->lev);
	return bonus - malus;
}

int unlight_power(struct player *p)
{
	return unlight_power_state(&p->mon.state, p);
}


int glow_power_state(struct player_state *ps, struct player *p)
{
	if (!cave || !character_dungeon) return 0;
	if (ps->powers[PP_GLOW] <= 0) return 0;
	int bonus = square_light(cave, p->mon.grid);
	int malus = get_power_scale_state(ps, PP_GLOW, UNLIGHT_MAX_POWER, p->lev);
	return bonus - malus;
}

int glow_power(struct player *p)
{
	return glow_power_state(&p->mon.state, p);
}

/**
 * L: radius of darkness from an unlight player, also the depth of darkness
 * at their square
 */
int unlight_radius(struct player *p)
{
	return get_power_scale(p, PP_UNLIGHT, UNLIGHT_MAX_POWER * 2);
}


int player_grid_visibility(struct loc grid, struct player *p, struct chunk *c)
{
	int darkest = 1;
	int brightest = 10;
	int light = square_light(c, grid);
	int unl_rad = unlight_radius(p);
	int dist = distance(p->mon.grid, grid);
	bool p_is_unlight = p->mon.state.powers[PP_UNLIGHT] ? true : false;

	darkest -= get_power_scale(p, PP_UNLIGHT, UNLIGHT_MAX_POWER * 4);
	brightest -= get_power_scale(p, PP_UNLIGHT, 10);

	if (p_is_unlight && dist <= unl_rad && light <= 0) return PY_SEE_VISIBLE;
	if (light == 0) return PY_SEE_TOO_DARK;
	if (light > brightest) return PY_SEE_TOO_BRIGHT;
	if (light < darkest) return PY_SEE_TOO_DARK;

	return PY_SEE_VISIBLE;
}


/**
 * Increment to the next or decrement to the preceeding level
   accounting for the stair skip value in constants
   Keep in mind to check all intermediate level for unskippable
   quests
*/
int dungeon_get_next_level(struct player *p, int dlev, int added)
{
	int target_level, i;

	/* Get target level */
	target_level = dlev + added * z_info->stair_skip;

	/* Don't allow levels below max */
	if (target_level > z_info->max_depth - 1) {
		target_level = z_info->max_depth - 1;
	}

	/* Don't allow levels above the town */
	if (target_level < 0) target_level = 0;

	/* Check intermediate levels for quests */
	for (i = dlev; i <= target_level; i++) {
		if (is_quest(p, i)) return i;
	}

	return target_level;
}

/**
 * Set recall depth for a player recalling from town
 */
void player_set_recall_depth(struct player *p)
{
	/* Account for forced descent */
	if (OPT(p, birth_force_descend)) {
		/* Force descent to a lower level if allowed */
		if (p->max_depth < z_info->max_depth - 1
				&& !is_quest(p, p->max_depth)) {
			p->recall_depth = dungeon_get_next_level(p,
				p->max_depth, 1);
		}
	}

	/* Players who haven't left town before go to level 1 */
	p->recall_depth = MAX(p->recall_depth, 1);
}

/**
 * Give the player the choice of persistent level to recall to.  Note that if
 * a level greater than the player's maximum depth is chosen, we silently go
 * to the maximum depth.
 */
bool player_get_recall_depth(struct player *p)
{
	bool level_ok = false;
	int new = 0;

	while (!level_ok) {
		const char *prompt =
			"Which level do you wish to return to (0 to cancel)? ";
		int i;

		/* Choose the level */
		new = get_quantity(prompt, p->max_depth);
		if (new == 0) {
			return false;
		}

		/* Is that level valid? */
		for (i = 0; i < chunk_list_max; i++) {
			if (chunk_list[i]->depth == new) {
				level_ok = true;
				break;
			}
		}
		if (!level_ok) {
			msg("You must choose a level you have previously visited.");
		}
	}
	p->recall_depth = new;
	return true;
}

/**
 * Change dungeon level - e.g. by going up stairs or with WoR.
 */
void dungeon_change_level(struct player *p, int dlev)
{
	/* New depth */
	p->depth = dlev;

	/* If we're returning to town, update the store contents
	   according to how long we've been away */
	if (!dlev && daycount) {
		store_update();
	}

	/* Leaving, make new level */
	p->upkeep->generate_level = true;

	/* Save the game when we arrive on the new level. */
	p->upkeep->autosave = true;
}


/**
 * Returns what an incoming damage amount would be after applying a player's
 * damage reduction.
 *
 * \param p is the player of interest.
 * \param dam is the incoming damaage amount.
 * \return the damage after the player's damage reduction, if any.
 */
int player_apply_damage_reduction(struct player *p, int dam)
{
	/* Mega-Hack -- Apply "invulnerability" */
	if (p->mon.m_timed[TMD_INVULN] && (dam < 9000)) return 0;

	dam -= p->mon.state.dam_red;
	if (dam > 0 && p->mon.state.perc_dam_red) {
		dam -= (dam * p->mon.state.perc_dam_red) / 100 ;
	}

	return (dam < 0) ? 0 : dam;
}

static bool phoenix_resurrect(struct player *p)
{
	int avail_mana = available_mana(cave, p->mon.grid);

	if (!pf_has(p->mon.state.pflags, PF_PHOENIX_RESURRECT)) return false;
	if (p->mon.m_timed[TMD_PHOENIX_CD]) return false;
	if (avail_mana < 5) return false;

	p->mon.hp = 0;
	p->mon.m_timed[TMD_PHOENIX] = 1;

	return true;
}

/**
 * Decreases players hit points and sets death flag if necessary
 *
 * \param p is the player of interest.
 * \param dam is the amount of damage to apply.  If dam is less than
 * or equal to zero, nothing will be done.  The amount of damage should have
 * been processed with player_apply_damage_reduction(); that is not done
 * internally here so the caller can display messages that include the amount of
 * damage.
 * \param kb_str is the null-terminated string describing the cause of the
 * damage.
 * 
 * L: now returns whether the calling function should stop (either the) player
 * is dead or is cheating death and thus changing levels
 *
 * Hack -- this function allows the user to save (or quit) the game
 * when he dies, since the "You die." message is shown before setting
 * the player to "dead".
 */
bool take_hit(struct player *p, int dam, const char *kb_str)
{
	int old_chp = p->mon.hp;

	int warning = (p->mon.maxhp * p->opts.hitpoint_warn / 10);

	/* Paranoia */
	if (p->is_dead || dam <= 0) return p->is_dead;

	if (p->mon.m_timed[TMD_PHOENIX]) return false;

	/* Disturb */
	disturb(p);

	assert(dam >= 0);

	/* Hurt the player */
	if ((int)p->mon.hp - dam < INT16_MIN) p->mon.hp = INT16_MIN;
	else p->mon.hp -= dam;

	/* Reward COMBAT_REGEN characters with mana for their lost hitpoints
	 * Unenviable task of separating what should and should not cause rage
	 * If we eliminate the most exploitable cases it should be fine.
	 * All traps and lava currently give mana, which could be exploited  */
	if (player_has(p, PF_COMBAT_REGEN)  && !streq(kb_str, "poison")
			&& !streq(kb_str, "a fatal wound") && !streq(kb_str, "starvation")) {
		/* lose X% of hitpoints get X% of spell points */
		int32_t sp_gain = (((int32_t)MAX(p->msp, 10)) * 65536)
			/ (int32_t)p->mon.maxhp * dam;
		player_adjust_mana_precise(p, sp_gain);
	}

	/* Display the hitpoints */
	p->upkeep->redraw |= (PR_HP);

	/* Dead player */
	if (p->mon.hp < 0) {
		/* From hell's heart I stab at thee */
		if (p->mon.m_timed[TMD_BLOODLUST]
				&& (p->mon.hp + (p->mon.m_timed[TMD_BLOODLUST] * (p->mon.maxhp + 25) / 125) >= 0)) {
			if (randint0(10)) {
				msg("Your lust for blood keeps you alive!");
			} else {
				msg("So great was his prowess and skill in warfare, the Elves said: ");
				msg("'The Mormegil cannot be slain, save by mischance.'");
			}
		} else if (phoenix_resurrect(p)) {
			msgt(MSG_DEATH, "You die.");
			event_signal(EVENT_MESSAGE_FLUSH);
			return p->is_dead;
		} else {
			/*
			 * Note cause of death.  Do it here so EVENT_CHEAT_DEATH
			 * handlers or things looking for the "Die? " prompt
			 * (the borg, for instance), have access to it.
			 */
			my_strcpy(p->died_from, kb_str, sizeof(p->died_from));

			if ((p->wizard || OPT(p, cheat_live))
					&& !get_check("Die? ")) {
				event_signal(EVENT_CHEAT_DEATH);
				return true;
			} else {
				/* Hack -- Note death */
				msgt(MSG_DEATH, "You die.");
				event_signal(EVENT_MESSAGE_FLUSH);

				/* No longer a winner */
				p->total_winner = false;

				/* Note death */
				p->is_dead = true;

				/* Dead */
				return p->is_dead;
			}
		}
	}

	/* Hitpoint warning */
	if (p->mon.hp < warning) {
		/* Hack -- bell on first notice */
		if (old_chp > warning) {
			bell();
		}

		/* Message */
		msgt(MSG_HITPOINT_WARN, "*** LOW HITPOINT WARNING! ***");
		event_signal(EVENT_MESSAGE_FLUSH);
	}

	return p->is_dead;
}

bool check_berserk(struct monster *mon, struct monster *o_mon)
{
	int berserk = get_mon_power_scale(mon, PP_BERSERK, 25);

	if (!mon || !mon->race) {
		return false;
	}
	if (!o_mon || !o_mon->race) {
		return false;
	}
	/*if (!monster_is_visible(o_mon)) {
		// can't get mad at something you can't see
		return false;
	}*/
	if (berserk <= 0) {
		return false;
	}
	if (mon->m_timed[TMD_SLOW]) {
		// too tired to berserk
		return false;
	}
	// somewhere between the amount of hp lost and the ratio of hp lost to max hp
	// 25 max hp = up to 25 increase; 100 max hp = up to 40 increase (with max roll at 0 hp)
	int increase = (randint1(mon->maxhp) - mon->hp * 2 / 3) * (berserk + 25) / (mon->maxhp + 25);
	
	if (increase >= 0) {
		// higher increase the less you are already
		increase -= mon->m_timed[TMD_BLOODLUST] / 3 - 5;

		return mon_inc_timed(mon, TMD_BLOODLUST, MAX(increase, 0), MON_TMD_FLG_NOTIFY | MON_TMD_FLG_NOFAIL);
		//return player_inc_timed(p, TMD_BLOODLUST, MAX(increase, 0), true, true, false);
	}
	return false;
}

void take_max_sp_dam(struct player *p, int dam)
{
	if (p->is_dead) return;
	if (dam <= 5) return;
	int quantity = (int)my_sqrt((double)dam - 5);

	p->sp_burn += quantity;
	msg("You feel weaker.");

	p->upkeep->update |= PU_MANA;
}

/**
 * Win or not, know inventory, home items and history upon death, enter score
 */
void death_knowledge(struct player *p)
{
	struct store *home = &stores[f_info[FEAT_HOME].shopnum - 1];
	struct object *obj;
	time_t death_time = (time_t)0;

	/* Retire in the town in a good state */
	if (p->total_winner) {
		p->depth = 0;
		my_strcpy(p->died_from, "Ripe Old Age", sizeof(p->died_from));
		p->exp = p->max_exp;
		p->lev = p->max_lev;
		p->au += 10000000L;
	}

	player_learn_all_runes(p);
	for (obj = p->mon.gear; obj; obj = obj->next) {
		object_flavor_aware(p, obj);
		obj->known->effect = obj->effect;
		obj->known->activation = obj->activation;
	}

	for (obj = home->stock; obj; obj = obj->next) {
		object_flavor_aware(p, obj);
		obj->known->effect = obj->effect;
		obj->known->activation = obj->activation;
	}

	history_unmask_unknown(p);

	/* Get time of death */
	(void)time(&death_time);
	enter_score(p, &death_time);

	/* Hack -- Recalculate bonuses */
	p->upkeep->update |= (PU_BONUS);
	handle_stuff(p);
}

/**
 * L: unlock tomes on death or victory
 */
bool tomes_unlock(struct player *p)
{
	bool can_unlock = false;
	const char *prevent_unlock = NULL;
	bool add_space = true;
	bool did_unlock = false;
	int i;

	if (!player_can_metaprogress(p)) prevent_unlock = "metaprogression is turned off";
	if (p->noscore) prevent_unlock = "character is a cheater";

	if (p->is_dead && streq(p->died_from, "Retiring")) can_unlock = true;
	if (p->total_winner) can_unlock = true;

	if (!can_unlock) return false;

	for (i = 0; i < PP_MAX; ++i) {
		if (p->extra_powers[i] > p->unlocked_tomes[i]) {

			if (add_space) {
				message_add(" ", MSG_GENERIC);
				add_space = false;
			}

			if (prevent_unlock) {
				msg("You would unlock %s [%s] but %s.", lookup_power_name(i), p->extra_powers[i], prevent_unlock);
			}
			else {
				msg("Unlocked %s [%s]!", lookup_power_name(i), p->extra_powers[i]);
				p->unlocked_tomes[i] = p->extra_powers[i];
				did_unlock = true;
			}
		}
	}
	/*for (i = 0; i < SKILL_MAX; ++i) {
		if (add_space) {
			message_add(" ", MSG_GENERIC);
			add_space = false;
		}

		if (prevent_unlock) {
			msg("You would unlock %s but %s.", skill_index_to_name(i), prevent_unlock);
		}
		else {
			msg("Unlocked %s!", skill_index_to_name(i));
			p->unlocked_tomes[i + PP_MAX] = p->extra_skills[i];
			did_unlock = true;
		}
	}*/

	if (did_unlock) message_add(" ", MSG_GENERIC);

	return did_unlock;
}

static bool race_is_evolution(struct monster_race *or, struct monster_race *mr)
{
	struct evolution *evol;

	if (or == mr) return true;

	for (evol = or->evol; evol; evol = evol->next) {
		if (race_is_evolution(evol->race, mr)) return true;
	}
	return false;
}

static bool unlock_by_race(struct player *p, struct monster_race *mr, bool first)
{
	struct player_race *pr;
	bool unlockedany = false;
	bool addspace = first;

	const char *prevent_unlock = NULL;

	if (p->noscore) prevent_unlock = "character is a cheater";

	for (pr = races; pr; pr = pr->next) {
		bool unlock_race = false;
		struct evolution *evol;

		if (p->unlocked_races[pr->ridx]) continue;

		for (evol = pr->evol; evol && !unlock_race; evol = evol->next) {
			if (race_is_evolution(evol->race, mr)) {
				unlock_race = true;
			}
		}

		if (unlock_race) {
			if (addspace && !unlockedany) {
				message_add(" ", MSG_GENERIC);
				addspace = false;
			}

			if (prevent_unlock) {
				msg("You would unlock %s but %s.", pr->name, prevent_unlock);
			}
			else {
				msg("Unlocked %s!", pr->name);
				p->unlocked_races[pr->ridx] = true;
				unlockedany = true;
			}
		}
	}

	return unlockedany;
}

bool races_unlock(struct player *p)
{
	struct monster_race *mr;
	int i;
	bool didunlock;

	if (cave->depth > 0) return false;
	if (!player_can_metaprogress(p)) return false;
	if (p->noscore) return false;

	for (i = cave_monster_max(cave); i >= 0; --i) {
		struct monster *mon = cave_monster(cave, i);

		if (!mon || !mon->race) continue;
		if (mon->reaction < MON_REACT_ALLY && mon->faction != '@') continue;

		didunlock = unlock_by_race(p, mon->race, !didunlock) || didunlock;
	}

	mr = lookup_player_monster(p);
	if (mr) {
		unlock_by_race(p, mr, !didunlock);
	}

	if (didunlock) {
		message_add(" ", MSG_GENERIC);
	}

	return didunlock;
}

/**
 * Energy per move, taking extra moves into account
 */
int energy_per_move(struct player *p)
{
	int num = p->mon.state.num_moves;
	int energy = z_info->move_energy;

	/*
	   old         new
	-3 -> 7/4   -3 -> 16/13
	-2 -> 5/3   -2 -> 14/12
	-1 -> 3/2   -1 -> 12/11
	 0 -> 1/1    0 -> 10/10
	 1 -> 1/2    1 -> 10/11
	 2 -> 1/3    2 -> 10/12
	 3 -> 1/4    3 -> 10/13
	*/
	int result =  (energy * (10 + ABS(num) - num)) / (10 + ABS(num));
	return result;
}

/**
 * Modify a stat value by a "modifier", return new value
 *
 * Stats go up: 3,4,...,17,18,18/10,18/20,...,18/220
 * Or even: 18/13, 18/23, 18/33, ..., 18/220
 *
 * Stats go down: 18/220, 18/210,..., 18/10, 18, 17, ..., 3
 * Or even: 18/13, 18/03, 18, 17, ..., 3
 */
int16_t modify_stat_value(int value, int amount)
{
	int i;

	/* Reward or penalty */
	if (amount > 0) {
		/* Apply each point */
		for (i = 0; i < amount; i++) {
			/* One point at a time */
			if (value < 18) value++;

			/* Ten "points" at a time */
			else value += 10;
		}
	} else if (amount < 0) {
		/* Apply each point */
		for (i = 0; i < (0 - amount); i++) {
			/* Ten points at a time */
			if (value >= 18+10) value -= 10;

			/* Hack -- prevent weirdness */
			else if (value > 18) value = 18;

			/* One point at a time */
			else if (value > 3) value--;
		}
	}

	/* Return new value */
	return (value);
}

/**
 * Swap player's stats at random, retaining information so they can be
 * reverted to their original state.
 */
void player_scramble_stats(struct player *p)
{
	int max1, cur1, max2, cur2, i, j, swap;

	/* Fisher-Yates shuffling algorithm */
	for (i = STAT_MAX - 1; i > 0; --i) {
		j = randint0(i);

		max1 = p->stat_max[i];
		cur1 = p->stat_cur[i];
		max2 = p->stat_max[j];
		cur2 = p->stat_cur[j];

		p->stat_max[i] = max2;
		p->stat_cur[i] = cur2;
		p->stat_max[j] = max1;
		p->stat_cur[j] = cur1;

		/* Record what we did */
		swap = p->stat_map[i];
		assert(swap >= 0 && swap < STAT_MAX);
		p->stat_map[i] = p->stat_map[j];
		assert(p->stat_map[i] >= 0 && p->stat_map[i] < STAT_MAX);
		p->stat_map[j] = swap;
	}

	/* Mark what else needs to be updated */
	p->upkeep->update |= (PU_BONUS);
}

/**
 * Revert all prior swaps to the player's stats.  Has no effect if the
 * stats have not been swapped.
 */
void player_fix_scramble(struct player *p)
{
	/* Figure out what stats should be */
	int new_cur[STAT_MAX];
	int new_max[STAT_MAX];
	int i;

	for (i = 0; i < STAT_MAX; ++i) {
		assert(p->stat_map[i] >= 0 && p->stat_map[i] < STAT_MAX);
		new_cur[p->stat_map[i]] = p->stat_cur[i];
		new_max[p->stat_map[i]] = p->stat_max[i];
	}

	/* Apply new stats and reset stat_map */
	for (i = 0; i < STAT_MAX; ++i) {
		p->stat_cur[i] = new_cur[i];
		p->stat_max[i] = new_max[i];
		p->stat_map[i] = i;
	}

	/* Mark what else needs to be updated */
	p->upkeep->update |= (PU_BONUS);
}

/**
 * Regenerate one turn's worth of hit points
 */
void player_regen_hp(struct player *p)
{
	int32_t hp_gain;
	int percent = 0; // max 32k -> 50% of mhp; more accurately "pertwobytes"
	int fed_pct, old_chp = p->mon.hp;

	/* Default regeneration */
	if (p->mon.m_timed[TMD_FOOD] >= PY_FOOD_FULL) {
		percent = PY_REGEN_FULL;
	} else if (p->mon.m_timed[TMD_FOOD] >= PY_FOOD_WEAK) {
		percent = PY_REGEN_NORMAL;
	} else if (p->mon.m_timed[TMD_FOOD] >= PY_FOOD_FAINT) {
		percent = PY_REGEN_WEAK;
	} else if (p->mon.m_timed[TMD_FOOD] >= PY_FOOD_STARVE) {
		percent = PY_REGEN_FAINT;
	}

	// L: regeneration is now much more food-based
	fed_pct = p->mon.m_timed[TMD_FOOD] / z_info->food_value - 100;
	if (fed_pct > 0) fed_pct *= 2;
	percent = MAX(percent + fed_pct, 0);

	/* Various things speed up regeneration */
	if (player_of_has(p, OF_HI_REGEN)) {
		percent *= 25;
	}
	else if (player_of_has(p, OF_REGEN) || p->mon.m_timed[TMD_REGEN]) {
		percent *= 3;
	}
	if (player_resting_can_regenerate(p)) {
		percent *= 2;
	}

	/* Some things slow it down */
	if (player_of_has(p, OF_IMPAIR_HP)) {
		percent /= 2;
	}

	/* Various things interfere with physical healing */
	if (p->mon.m_timed[TMD_PARALYZED]) percent = 0;
	if (p->mon.m_timed[TMD_POISONED]) percent = 0;
	if (p->mon.m_timed[TMD_STUN]) percent = 0;
	if (p->mon.m_timed[TMD_CUT]) percent = 0;

	/* Extract the new hitpoints */
	hp_gain = p->mon.maxhp * percent + PY_REGEN_HPBASE;
	player_adjust_hp_precise(p, hp_gain);

	/* Notice changes */
	if (old_chp != p->mon.hp) {
		equip_learn_flag(p, OF_REGEN);
		equip_learn_flag(p, OF_IMPAIR_HP);
	}
}


/**
 * Update the player's light fuel
 */
void player_update_light(struct player *p)
{
	/* Check for light being wielded */
	struct object *obj = slot_object(&p->mon, slot_by_type(&p->mon, EQUIP_LIGHT, true));

	/* Burn some fuel in the current light */
	if (obj && tval_is_light(obj)) {
		bool burn_fuel = true;

		/* Turn off the wanton burning of light during the day in the town */
		if (!p->depth && is_daytime())
			burn_fuel = false;

		/* If the light has the NO_FUEL flag, well... */
		if (of_has(obj->flags, OF_NO_FUEL))
		    burn_fuel = false;

		/* Use some fuel (except on artifacts, or during the day) */
		if (burn_fuel && obj->timeout > 0) {
			/* Decrease life-span */
			obj->timeout--;

			/* Hack -- notice interesting fuel steps */
			if ((obj->timeout < 100) || (!(obj->timeout % 100)))
				/* Redraw stuff */
				p->upkeep->redraw |= (PR_EQUIP);

			/* Hack -- Special treatment when blind */
			if (p->mon.m_timed[TMD_BLIND]) {
				/* Hack -- save some light for later */
				if (obj->timeout == 0) obj->timeout++;
			} else if (obj->timeout == 0) {
				/* The light is now out */
				disturb(p);
				msg("Your light has gone out!");

				/* If it's a torch, now is the time to delete it */
				if (of_has(obj->flags, OF_BURNS_OUT)) {
					bool dummy;
					struct object *burnt =
						gear_object_for_use(&p->mon, obj, 1,
						false, &dummy);
					if (burnt->known)
						object_delete(p->cave, NULL, &burnt->known);
					object_delete(cave, p->cave, &burnt);
				}
			} else if ((obj->timeout < 50) && (!(obj->timeout % 20))) {
				/* The light is getting dim */
				disturb(p);
				msg("Your light is growing faint.");
			}
		}
	}

	/* Calculate torch radius */
	p->upkeep->update |= (PU_TORCH);
}

/**
 * Find the player's best digging tool.  If forbid_stack is true, ignores
 * stacks of more than one item.
 */
struct object *player_best_digger(struct player *p, bool forbid_stack)
{
	int weapon_slot = slot_by_type(&p->mon, EQUIP_WEAPON, true);
	struct object *current_weapon = slot_object(&p->mon, weapon_slot);
	struct object *obj, *best = NULL;
	/* Prefer any melee weapon over unarmed digging, i.e. best == NULL. */
	int best_score = -1;
	struct player_state local_state;

	if (weapon_slot == -1) return NULL;

	for (obj = p->mon.gear; obj; obj = obj->next) {
		int score, old_number;
		if (!tval_is_melee_weapon(obj)) continue;
		if (obj->number < 1 || (forbid_stack && obj->number > 1)) continue;
		/* Don't use it if it has a sticky curse. */
		if (!obj_can_takeoff(obj)) continue;

		/* Swap temporarily for the calc_bonuses() computation. */
		old_number = obj->number;
		if (obj != current_weapon) {
			obj->number = 1;
			p->mon.body.slots[weapon_slot].obj = obj;
		}

		/*
		 * Avoid side effects from using update set to false
		 * with calc_bonuses().
		 */
		local_state.stat_ind[STAT_STR] = 0;
		local_state.stat_ind[STAT_DEX] = 0;
		calc_bonuses(p, &p->mon, &local_state, true, false);
		score = local_state.skills[SKILL_DIGGING];

		/* Swap back. */
		if (obj != current_weapon) {
			obj->number = old_number;
			p->mon.body.slots[weapon_slot].obj = current_weapon;
		}

		if (score > best_score) {
			best = obj;
			best_score = score;
		}
	}

	return best;
}

static struct monster *player_nearest_monster(struct player *p, struct chunk *c)
{
	int i, closestdist = 0;
	struct monster *closest = NULL;

	// find the closest foe
	for (i = 1; i < cave_monster_max(cave); ++i) {
		struct monster *mon = cave_monster(cave, i);

		if (!mon || !mon->race) continue;
		if (!monster_is_visible(mon)) continue;
		if (!projectable(c, p->mon.grid, mon->grid, PROJECT_INFO)) continue;
		if (monster_is_camouflaged(mon)) continue;
		int dist = distance(p->mon.grid, mon->grid);
		if (closest && dist > closestdist) continue;
		// no check for allies in a berserker rage

		closest = mon;
		closestdist = dist;
	}

	return closest;
}

/**
 * Melee a random adjacent monster
 */
static bool player_bloodlust_attack_monster(struct player *p, struct monster *mon)
{
	if (player_can_attack_monster(p, mon) && target_set_monster(mon)) {
		char mdesc[80];

		if (p->mon.m_timed[TMD_IMAGE]) {
			my_strcpy(mdesc, "something", sizeof(mdesc));
		} else {
			monster_desc(mdesc, sizeof(mdesc), mon, MDESC_TARG);
		}

		disturb(p);

		msg("You furiously lash out at %s!", mdesc);

		cmdq_push(CMD_MELEE);
		// we have to use DIR_TARGET in case we attack something not adjacent
		cmd_set_arg_target(cmdq_peek(), "target", DIR_TARGET);
		event_signal(EVENT_MESSAGE_FLUSH);

		return true;
	}

	return false;
}

static bool player_bloodlust_charge_monster(struct player *p, struct monster *mon, struct chunk *c)
{
	int i, dir;
	struct loc difference = loc_diff(mon->grid, p->mon.grid);
	struct loc target_grid = difference;
	struct loc target_grids[3] = { 0 };

	if (!target_set_monster(mon)) return false;

	target_grid.x = MAX(-1, MIN(1, target_grid.x));
	target_grid.y = MAX(-1, MIN(1, target_grid.y));

	/* grids to try to go to in order of preference: diagonally towards,
	   then horizontally and vertically depending on which is more direct */
	target_grids[0] = target_grid;
	if (ABS(difference.x) > ABS(difference.y)) {
		target_grids[1] = loc(target_grid.x, 0);
		target_grids[2] = loc(0, target_grid.y);
	}
	else {
		target_grids[1] = loc(0, target_grid.y);
		target_grids[2] = loc(target_grid.x, 0);
	}

	for (i = 0; i < 3; ++i) {
		struct loc targ_grid_abs;
		if (loc_is_zero(target_grids[i])) continue;

		for (dir = 1; dir <= 9; ++dir) {
			if (loc_eq(ddgrid[dir], target_grids[i])) break;
		}

		targ_grid_abs = loc_sum(p->mon.grid, ddgrid[dir]);
		
		if (!square_ispassable(c, targ_grid_abs)) continue;
		if (square_monster(c, targ_grid_abs)) continue;
		if (distance(p->mon.grid, mon->grid) <= distance(targ_grid_abs, mon->grid)) continue;
		
		char mdesc[80];
		if (p->mon.m_timed[TMD_IMAGE]) {
			my_strcpy(mdesc, "something", sizeof(mdesc));
		} else {
			monster_desc(mdesc, sizeof(mdesc), mon, MDESC_TARG);
		}

		disturb(p); // make sure we don't repeat commands

		msg("You furiously charge at %s!", mdesc);

		cmdq_push(CMD_WALK);
		cmd_set_arg_direction(cmdq_peek(), "direction", dir);
		event_signal(EVENT_MESSAGE_FLUSH);
		return true;
	}

	return false;
}

bool bloodlust_override(struct player *p, struct chunk *c)
{
	int currtmd = p->mon.m_timed[TMD_BLOODLUST];
	struct monster *target;

	if (p->mon.m_timed[TMD_PARALYZED] || p->mon.m_timed[TMD_COMMAND]) return false;

	if (p->skip_cmd_coercion) return false;
	//if (currtmd <= (randint0(30) + 5)) return false;
	if (!currtmd) return false;

	target = player_nearest_monster(p, c);

	if (target) {
		if (player_bloodlust_attack_monster(p, target)) return true;
		if (player_bloodlust_charge_monster(p, target, c)) return true;
	}

	msg("You have run out of enemies to fight!");

	player_over_exert(p, PY_EXERT_CONF, 100, currtmd * 2);
	player_over_exert(p, PY_EXERT_FAINT, 75, currtmd * 3 / 2);
	player_over_exert(p, PY_EXERT_CUT, 50, p->mon.maxhp / 10);

	player_dec_timed(p, TMD_BLOODLUST, currtmd, true, false);

	return false;
}

/**
 * Have random bad stuff happen to the player from over-exertion
 *
 * This function uses the PY_EXERT_* flags
 */
void player_over_exert(struct player *p, int flag, int chance, int amount)
{
	if (chance <= 0) return;

	/* CON damage */
	if (flag & PY_EXERT_CON) {
		if (randint0(100) < chance) {
			/* Hack - only permanent with high chance (no-mana casting) */
			bool perm = (randint0(100) < chance / 2) && (chance >= 50);
			msg("You have damaged your health!");
			player_stat_dec(p, STAT_CON, perm);
		}
	}

	/* Fainting */
	if (flag & PY_EXERT_FAINT) {
		if (randint0(100) < chance) {
			msg("You faint from the effort!");

			/* Bypass free action */
			(void)player_inc_timed(p, TMD_PARALYZED,
				randint1(amount), true, true, false);
		}
	}

	/* Scrambled stats */
	if (flag & PY_EXERT_SCRAMBLE) {
		if (randint0(100) < chance) {
			(void)player_inc_timed(p, TMD_SCRAMBLE,
				randint1(amount), true, true, true);
		}
	}

	/* Cut damage */
	if (flag & PY_EXERT_CUT) {
		if (randint0(100) < chance) {
			msg("Wounds appear on your body!");
			(void)player_inc_timed(p, TMD_CUT, randint1(amount),
				true, true, false);
		}
	}

	/* Confusion */
	if (flag & PY_EXERT_CONF) {
		if (randint0(100) < chance) {
			(void)player_inc_timed(p, TMD_CONFUSED,
				randint1(amount), true, true, true);
		}
	}

	/* Hallucination */
	if (flag & PY_EXERT_HALLU) {
		if (randint0(100) < chance) {
			(void)player_inc_timed(p, TMD_IMAGE, randint1(amount),
				true, true, true);
		}
	}

	/* Slowing */
	if (flag & PY_EXERT_SLOW) {
		if (randint0(100) < chance) {
			msg("You feel suddenly lethargic.");
			(void)player_inc_timed(p, TMD_SLOW, randint1(amount),
				true, true, false);
		}
	}

	/* HP */
	if (flag & PY_EXERT_HP) {
		if (randint0(100) < chance) {
			int dam = player_apply_damage_reduction(p,
				randint1(amount));
			char dam_text[32] = "";

			if (dam > 0 && OPT(p, show_damage)) {
				strnfmt(dam_text, sizeof(dam_text),
					" (%d)", dam);
			}
			msg("You cry out in sudden pain!%s", dam_text);
			take_hit(p, dam, "over-exertion");
		}
	}
}


/**
 * See how much damage the player will take from terrain.
 *
 * \param p is the player to check
 * \param grid is the location of the terrain
 * \param actual, if true, will cause the player to learn the appropriate
 * runes if equipment or effects mitigate the damage.
 */
int player_check_terrain_damage(struct player *p, struct loc grid, bool actual)
{
	int dam_taken = 0;

	if (square_isfiery(cave, grid)) {
		int base_dam = 100 + randint1(100);
		int res = p->mon.state.el_info[ELEM_FIRE].res_level;

		/* Fire damage */
		dam_taken = adjust_dam(p, ELEM_FIRE, base_dam, RANDOMISE, res,
			actual);

		/* Feather fall makes one lightfooted. */
		if (player_of_has(p, OF_FEATHER)) {
			dam_taken /= 2;
			if (actual) {
				equip_learn_flag(p, OF_FEATHER);
			}
		}
	}

	return dam_taken;
}

/**
 * Terrain damages the player
 */
void player_take_terrain_damage(struct player *p, struct loc grid)
{
	return;
}

/**
 * Find a player shape from the name
 */
struct player_shape *lookup_player_shape(const char *name)
{
	struct player_shape *shape = shapes;
	while (shape) {
		if (streq(shape->name, name)) {
			return shape;
		}
		shape = shape->next;
	}
	msg("Could not find %s shape!", name);
	return NULL;
}

/**
 * Find a player shape index from the shape name
 */
int shape_name_to_idx(const char *name)
{
	struct player_shape *shape = lookup_player_shape(name);
	if (shape) {
		return shape->sidx;
	} else {
		return -1;
	}
}

/**
 * Find a player shape from the index
 */
struct player_shape *player_shape_by_idx(int index)
{
	struct player_shape *shape = shapes;
	while (shape) {
		if (shape->sidx == index) {
			return shape;
		}
		shape = shape->next;
	}
	msg("Could not find shape %d!", index);
	return NULL;
}

/**
 * Give shapechanged players a choice of returning to normal shape and
 * performing a command, just returning to normal shape without acting, or
 * canceling.
 *
 * \param p the player
 * \param cmd the command being performed
 * \return true if the player wants to proceed with their command
 */
bool player_get_resume_normal_shape(struct player *p, struct command *cmd)
{
	if (player_is_shapechanged(p)) {
		msg("You cannot do this while in %s form.", p->shape->name);
		char prompt[100];
		strnfmt(prompt, sizeof(prompt),
		        "Change back and %s (y/n) or (r)eturn to normal? ",
		        cmd_verb(cmd->code));
		char answer = get_char(prompt, "yrn", 3, 'n');

		// Change back to normal shape
		if (answer == 'y' || answer == 'r') {
			player_resume_normal_shape(p);
		}

		// Players may only act if they return to normal shape
		return answer == 'y';
	}

	// Normal shape players can proceed as usual
	return true;
}

/**
 * Revert to normal shape
 */
void player_resume_normal_shape(struct player *p)
{
	p->shape = lookup_player_shape("normal");
	msg("You resume your usual shape.");

	/* Kill vampire attack */
	(void) player_clear_timed(p, TMD_ATT_VAMP, true, false);

	/* Update */
	p->upkeep->update |= (PU_BONUS);
	p->upkeep->redraw |= (PR_TITLE | PR_MISC);
	handle_stuff(p);
}

/**
 * Check if the player is shapechanged
 */
bool player_is_shapechanged(const struct player *p)
{
	return streq(p->shape->name, "normal") ? false : true;
}

/**
 * Check if the player is immune from traps
 */
bool player_is_trapsafe(const struct player *p)
{
	if (p->mon.m_timed[TMD_TRAPSAFE]) return true;
	if (player_of_has(p, OF_TRAP_IMMUNE)) return true;
	return false;
}

/**
 * Return true if the player can cast a spell.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_cast(const struct player *p, bool show_msg)
{
	const struct magic_realm *realm = get_player_realm(p);

	if (p->mon.state.skills[SKILL_MAGIC] <= 0 || !realm) {
		if (show_msg) {
			msg("You do not know magic.");
		}
		return false;
	}

	if (p->mon.m_timed[TMD_BLIND] || no_light(p)) {
		if (show_msg) {
			msg("You cannot see!");
		}
		return false;
	}

	if (p->mon.m_timed[TMD_CONFUSED]) {
		if (show_msg) {
			msg("You are too confused!");
		}
		return false;
	}

	if (realm->realm_special[RLM_SPCL_HP_CAST] && pf_has(p->mon.state.pflags, PF_UNDEAD)) {
		if (show_msg) {
			msg("You have no blood with which to cast!");
		}
		return false;
	}

	return true;
}

/**
 * Return true if the player can study a spell.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_study(const struct player *p, bool show_msg)
{
	if (!player_can_cast(p, show_msg)) {
		return false;
	}

	if (!p->upkeep->new_spells) {
		if (show_msg) {
			int count;
			struct magic_realm *r = class_magic_realms(p->class, &count), *r1;
			char buf[120];

			my_strcpy(buf, r->spell_noun, sizeof(buf));
			my_strcat(buf, "s", sizeof(buf));
			r1 = r->next;
			mem_free(r);
			r = r1;
			if (count > 1) {
				while (r) {
					count--;
					if (count) {
						my_strcat(buf, ", ", sizeof(buf));
					} else {
						my_strcat(buf, " or ", sizeof(buf));
					}
					my_strcat(buf, r->spell_noun, sizeof(buf));
					my_strcat(buf, "s", sizeof(buf));
					r1 = r->next;
					mem_free(r);
					r = r1;
				}
			}
			msg("You cannot learn any new %s!", buf);
		}
		return false;
	}

	return true;
}

/**
 * Return true if the player can read scrolls or books.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_read(const struct player *p, bool show_msg)
{
	if (p->mon.m_timed[TMD_BLIND]) {
		if (show_msg)
			msg("You can't see anything.");

		return false;
	}

	if (no_light(p)) {
		if (show_msg)
			msg("You have no light to read by.");

		return false;
	}

	if (p->mon.m_timed[TMD_CONFUSED]) {
		if (show_msg)
			msg("You are too confused to read!");

		return false;
	}

	if (p->mon.m_timed[TMD_AMNESIA]) {
		if (show_msg)
			msg("You can't remember how to read!");

		return false;
	}

	return true;
}

/**
 * Return true if the player can fire something with a launcher.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_fire(struct player *p, bool show_msg)
{
	// L: keep track of this when calcing bonuses
	if (!p->mon.state.has_ranged_attack) {
		if (show_msg) {
			msg("You have nothing to fire with.");
		}
		return false;
	}

	return true;
}

/**
 * Return true if the player can refuel their light source.
 *
 * \param p is the player
 * \param show_msg should be set to true if a failure message should be
 * displayed.
 */
bool player_can_refuel(struct player *p, bool show_msg)
{
	struct object *obj = slot_object(&p->mon, slot_by_type(&p->mon, EQUIP_LIGHT, true));

	if (obj && of_has(obj->flags, OF_TAKES_FUEL)) {
		return true;
	}

	if (show_msg) {
		msg("Your light cannot be refuelled.");
	}

	return false;
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_cast_prereq(void)
{
	return player_can_cast(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_study_prereq(void)
{
	const struct magic_realm *realm = get_player_realm(player);

	if (!realm || player->mon.state.skills[SKILL_MAGIC] <= 0) {
		msg("You don't know magic!");
		return false;
	}
	if (realm->realm_special[RLM_SPCL_INNATE]) {
		msg("You don't learn spells from books.");
		return false;
	}
	return true;
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_read_prereq(void)
{
	/*
	 * Accommodate hacks elsewhere:  'r' is overloaded to mean
	 * release a commanded monster when TMD_COMMAND is active.
	 */
	return (player->mon.m_timed[TMD_COMMAND]) ?
		true : player_can_read(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_fire_prereq(void)
{
	return player_can_fire(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_refuel_prereq(void)
{
	return player_can_refuel(player, true);
}

/**
 * Prerequisite function for command. See struct cmd_info in ui-input.h and
 * it's use in ui-game.c.
 */
bool player_can_debug_prereq(void)
{
	if (player->noscore & NOSCORE_DEBUG) {
		return true;
	}
	if (confirm_debug()) {
		/* Mark savefile */
		player->noscore |= NOSCORE_DEBUG;
		return true;
	}
	return false;
}


/**
 * Return true if the player has access to a book that has unlearned spells.
 *
 * \param p is the player
 */
bool player_book_has_unlearned_spells(struct player *p)
{
	int i, j;
	int item_max = z_info->pack_size + z_info->floor_size;
	struct object **item_list = mem_zalloc(item_max * sizeof(struct object *));
	int item_num;

	/* Check if the player can learn new spells */
	if (!p->upkeep->new_spells) {
		mem_free(item_list);
		return false;
	}

	/* Check through all available books */
	item_num = scan_items(item_list, item_max, p, USE_INVEN | USE_FLOOR,
		obj_can_study);
	for (i = 0; i < item_num; i++) {
		const struct class_book *book = player_object_to_book(p, item_list[i]);
		if (!book) continue;

		/* Extract spells */
		for (j = 0; j < book->num_spells; j++)
			if (spell_okay_to_study(p, book->spells[j].sidx)) {
				/* There is a spell the player can study */
				mem_free(item_list);
				return true;
			}
	}

	mem_free(item_list);
	return false;
}

/**
 * Apply confusion, if needed, to a direction
 *
 * Display a message and return true if direction changes.
 */
bool player_confuse_dir(struct player *p, int *dp, bool too)
{
	int dir = *dp;

	if (p->mon.m_timed[TMD_CONFUSED]) {
		if ((dir == 5) || (randint0(100) < 75)) {
			/* Random direction */
			dir = ddd[randint0(8)];
		}

	/* Running attempts always fail */
	if (too) {
		msg("You are too confused.");
		return true;
	}

	if (*dp != dir) {
		msg("You are confused.");
		*dp = dir;
		return true;
	}
	}

	return false;
}

/**
 * Return true if the provided count is one of the conditional REST_ flags.
 */
bool player_resting_is_special(int16_t count)
{
	switch (count) {
		case REST_COMPLETE:
		case REST_ALL_POINTS:
		case REST_SOME_POINTS:
			return true;
	}

	return false;
}

/**
 * Return true if the player is resting.
 */
bool player_is_resting(const struct player *p)
{
	return (p->upkeep->resting > 0 ||
			player_resting_is_special(p->upkeep->resting));
}

/**
 * Return the remaining number of resting turns.
 */
int16_t player_resting_count(const struct player *p)
{
	return p->upkeep->resting;
}

/**
 * In order to prevent the regeneration bonus from the first few turns, we have
 * to store the number of turns the player has rested. Otherwise, the first
 * few turns will have the bonus and the last few will not.
 */
static int player_turns_rested = 0;
static bool player_rest_disturb = false;

/**
 * Set the number of resting turns.
 *
 * \param count is the number of turns to rest or one of the REST_ constants.
 */
void player_resting_set_count(struct player *p, int16_t count)
{
	/* Cancel if player is disturbed */
	if (player_rest_disturb) {
		p->upkeep->resting = 0;
		player_rest_disturb = false;
		return;
	}

	/* Ignore if the rest count is negative. */
	if ((count < 0) && !player_resting_is_special(count)) {
		p->upkeep->resting = 0;
		return;
	}

	/* Save the rest code */
	p->upkeep->resting = count;

	/* Truncate overlarge values */
	if (p->upkeep->resting > 9999) p->upkeep->resting = 9999;
}

/**
 * Cancel current rest.
 */
void player_resting_cancel(struct player *p, bool disturb)
{
	player_resting_set_count(p, 0);
	player_turns_rested = 0;
	player_rest_disturb = disturb;
}

/**
 * Return true if the player should get a regeneration bonus for the current
 * rest.
 */
bool player_resting_can_regenerate(const struct player *p)
{
	return player_turns_rested >= REST_REQUIRED_FOR_REGEN ||
		player_resting_is_special(p->upkeep->resting);
}

/**
 * Perform one turn of resting. This only handles the bookkeeping of resting
 * itself, and does not calculate any possible other effects of resting (see
 * process_world() for regeneration).
 */
void player_resting_step_turn(struct player *p)
{
	if (autocast(p)) return;

	/* Timed rest */
	if (p->upkeep->resting > 0) {
		/* Reduce rest count */
		p->upkeep->resting--;

		/* Redraw the state */
		p->upkeep->redraw |= (PR_STATE);
	}

	/* Take a turn */
	p->upkeep->energy_use = z_info->move_energy;

	/* Increment the resting counters */
	p->resting_turn++;
	player_turns_rested++;
}

/**
 * Handle the conditions for conditional resting (resting with the REST_
 * constants).
 */
void player_resting_complete_special(struct player *p)
{
	/* Complete resting */
	if (!player_resting_is_special(p->upkeep->resting)) return;

	if (p->upkeep->resting == REST_ALL_POINTS) {
		if ((p->mon.hp == p->mon.maxhp) && (p->csp == p->msp))
			/* Stop resting */
			disturb(p);
	} else if (p->upkeep->resting == REST_COMPLETE) {
		if ((p->mon.hp == p->mon.maxhp) &&
			(p->csp == p->msp || player_has(p, PF_COMBAT_REGEN) || !p->floor_mana) &&
			!p->mon.m_timed[TMD_BLIND] && !p->mon.m_timed[TMD_CONFUSED] &&
			!p->mon.m_timed[TMD_POISONED] && !p->mon.m_timed[TMD_AFRAID] &&
			!p->mon.m_timed[TMD_TERROR] && !p->mon.m_timed[TMD_STUN] &&
			!p->mon.m_timed[TMD_CUT] && !p->mon.m_timed[TMD_SLOW] &&
			!p->mon.m_timed[TMD_PARALYZED] && !p->mon.m_timed[TMD_IMAGE] &&
			!p->word_recall && !p->deep_descent)
			/* Stop resting */
			disturb(p);
	} else if (p->upkeep->resting == REST_SOME_POINTS) {
		if ((p->mon.hp == p->mon.maxhp) || (p->csp == p->msp)) {
			/* Stop resting */
			disturb(p);
		}
	}
}

/* Record the player's last rest count for repeating */
static int player_resting_repeat_count = 0;

/**
 * Get the number of resting turns to repeat.
 *
 * \param p The current player.
 */
int player_get_resting_repeat_count(struct player *p)
{
	return player_resting_repeat_count;
}

/**
 * Set the number of resting turns to repeat.
 *
 * \param count is the number of turns requested for rest most recently.
 */
void player_set_resting_repeat_count(struct player *p, int16_t count)
{
	player_resting_repeat_count = count;
}

/**
 * L: percentage penalty that the monster gets with natural healing while
 * not resting
 */
static int mon_non_rest_penalty(struct monster *mon)
{
	int base = 90;
	int regen = get_mon_power_scale(mon, PP_REGENERATION, 90);

	return MAX(0, base - regen);
}


void regen_hp(struct monster *mon)
{
	int32_t hp_gain, hp_gain_resting;
	int percent = 0, percent_resting; // max 32k -> 50% of mhp; more accurately "pertwobytes"
	int old_chp = mon->hp;
	struct player *p = mon->player;
	int food;
	
	if (!p || pf_has(mon->state.pflags, PF_NO_FOOD)) {
		food = 50 * z_info->food_value;
	} else {
		food = mon->m_timed[TMD_FOOD];
	}

	if (mon->hp >= mon->maxhp) return;

	/* Default regeneration */
	if (food >= PY_FOOD_FULL) {
		percent = PY_REGEN_FULL;
	} else if (food >= PY_FOOD_WEAK) {
		percent = PY_REGEN_NORMAL;
	} else if (food >= PY_FOOD_FAINT) {
		percent = PY_REGEN_WEAK;
	} else if (food >= PY_FOOD_STARVE) {
		percent = PY_REGEN_FAINT;
	}

	/* Various things speed up regeneration */
	percent *= (100 + get_mon_power_scale(mon, PP_REGENERATION, 200));
	percent /= 100;

	/*if (p && !player_resting_can_regenerate(p)) {
		percent *= 100 - mon_non_rest_penalty(mon);
		percent /= 100;
	}*/
	/*if (of_has(mon->state.flags, OF_HI_REGEN)) {
		percent *= 25;
	}
	else if (of_has(mon->state.flags, OF_REGEN) || mon->m_timed[TMD_REGEN]) {
		percent *= 3;
	}*/
	/*if (player_resting_can_regenerate(p)) {
		percent *= 2;
	}*/

	/* Some things slow it down */
	if (of_has(mon->state.flags, OF_IMPAIR_HP)) {
		percent /= 2;
	}

	/* Various things interfere with physical healing */
	if (mon->m_timed[TMD_PARALYZED]) percent = 0;
	if (mon->m_timed[TMD_POISONED]) percent = 0;
	if (mon->m_timed[TMD_STUN]) percent = 0;
	if (mon->m_timed[TMD_CUT]) percent = 0;

	percent_resting = percent;

	percent *= 100 - mon_non_rest_penalty(mon);
	percent /= 100;

	/* Extract the new hitpoints */
	hp_gain = mon->maxhp * percent + PY_REGEN_HPBASE;
	hp_gain_resting = mon->maxhp * percent_resting + PY_REGEN_HPBASE;

	if (mon_is_player(mon) && player_turns_rested > 0) {
		//int temp = hp_gain;
		hp_gain += player_turns_rested * PY_REGEN_HPBASE * (player_turns_rested / 10 + 5);
		hp_gain = MIN(hp_gain, hp_gain_resting);
	}

	if (p) {
		player_adjust_hp_precise(p, hp_gain);
		/* Notice changes */
		if (old_chp != p->mon.hp) {
			equip_learn_flag(p, OF_REGEN);
			equip_learn_flag(p, OF_IMPAIR_HP);
		}
	} else {
		int amt = hp_gain >> 16;
		int amt_frac = hp_gain - amt;
		if (amt_frac < randint0(1 << 15)) {
			++amt;
		}
		mon->hp += amt;
	}

	mon->hp = MIN(mon->hp, mon->maxhp);
}


/**
 * Regenerate one turn's worth of mana
 */
void player_regen_mana(struct player *p)
{
	int32_t sp_gain;
	int percent, old_csp = p->csp;
	int oldfeel;

	/* Save the old spell points */
	old_csp = p->csp;

	/* Default regeneration */
	percent = PY_REGEN_NORMAL;

	/* L: Limited abount of mana per floor */
	percent *= square(cave, player->mon.grid)->mana;
	percent += 24;
	percent /= 25;

	/* Various things speed up regeneration, but shouldn't punish healthy BGs */
	if (!(player_has(p, PF_COMBAT_REGEN) && p->mon.hp > p->mon.maxhp / 2)) {
		if (player_of_has(p, OF_REGEN)) {
			percent *= 2;
		}
		if (player_resting_can_regenerate(p)) {
			percent *= 2;
		}
	}

	/* Some things slow it down */
	if (player_has(p, PF_COMBAT_REGEN)) {
		percent /= -2;
	} else if (player_of_has(p, OF_IMPAIR_MANA)) {
		percent /= 2;
	}

	/* Regenerate mana */
	sp_gain = (int32_t)(p->msp * percent);
	if (percent > 0) {
		sp_gain += PY_REGEN_MNBASE;
	}
	sp_gain = player_adjust_mana_precise(p, sp_gain);

	/* SP degen heals BGs at double efficiency vs casting */
	if (sp_gain < 0  && player_has(p, PF_COMBAT_REGEN)) {
		convert_mana_to_hp(p, -sp_gain * 2);
	}

	/* Notice changes */
	if (old_csp != p->csp) {
		if (player->depth) {
            oldfeel = (p->floor_mana + 14) / 15;
			p->floor_mana = MAX(0, p->floor_mana + old_csp - p->csp);
			if ((p->floor_mana + 14) / 15 != oldfeel) {
			    //display_mana_feeling();
			}
		}
		p->upkeep->redraw |= (PR_MANA);
		equip_learn_flag(p, OF_REGEN);
		equip_learn_flag(p, OF_IMPAIR_MANA);
	}
}

void player_adjust_hp_precise(struct player *p, int32_t hp_gain)
{
	int16_t old_16 = p->mon.hp;
	/* Load it all into 4 byte format */
	int32_t old_32 = ((int32_t) old_16) * 65536 + p->chp_frac, new_32;

	/* Check for overflow */
	if (hp_gain >= 0) {
		new_32 = (old_32 < INT32_MAX - hp_gain) ?
			old_32 + hp_gain : INT32_MAX;
	} else {
		new_32 = (old_32 > INT32_MIN - hp_gain) ?
			old_32 + hp_gain : INT32_MIN;
	}

	/* Break it back down */
	if (new_32 < 0) {
		/*
		 * Don't use right bitwise shift on negative values:  whether
		 * the left bits are zero or one depends on the system.
		 */
		int32_t remainder = new_32 % 65536;

		p->mon.hp = (int16_t) (new_32 / 65536);
		if (remainder) {
			assert(remainder < 0);
			p->chp_frac = (uint16_t) (65536 + remainder);
			assert(p->mon.hp > INT16_MIN);
			p->mon.hp -= 1;
		} else {
			p->chp_frac = 0;
		}
	} else {
		p->mon.hp = (int16_t)(new_32 >> 16);   /* div 65536 */
		p->chp_frac = (uint16_t)(new_32 & 0xFFFF); /* mod 65536 */
	}

	/* Fully healed */
	if (p->mon.hp >= p->mon.maxhp) {
		p->mon.hp = p->mon.maxhp;
		p->chp_frac = 0;
	}

	if (p->mon.hp != old_16) {
		p->upkeep->redraw |= (PR_HP);
	}
}


/**
 * Accept a 4 byte signed int, divide it by 65k, and add
 * to current spell points. p->csp and csp_frac are 2 bytes each.
 */
int32_t player_adjust_mana_precise(struct player *p, int32_t sp_gain)
{
	int16_t old_16 = p->csp;
	/* Load it all into 4 byte format*/
	int32_t old_32 = ((int32_t) p->csp) * 65536 + p->csp_frac, new_32;

	if (sp_gain == 0) return 0;

	/* Check for overflow */
	if (sp_gain > 0) {
		if (old_32 < INT32_MAX - sp_gain) {
			new_32 = old_32 + sp_gain;
		} else {
			new_32 = INT32_MAX;
			sp_gain = 0;
		}
	} else if (old_32 > INT32_MIN - sp_gain) {
		new_32 = old_32 + sp_gain;
	} else {
		new_32 = INT32_MIN;
		sp_gain = 0;
	}

	/* Break it back down*/
	if (new_32 < 0) {
		/*
		 * Don't use right bitwise shift on negative values:  whether
		 * the left bits are zero or one depends on the system.
		 */
		int32_t remainder = new_32 % 65536;

		p->csp = (int16_t) (new_32 / 65536);
		if (remainder) {
			assert(remainder < 0);
			p->csp_frac = (uint16_t) (65536 + remainder);
			assert(p->csp > INT16_MIN);
			p->csp -= 1;
		} else {
			p->csp_frac = 0;
		}
	} else {
		p->csp = (int16_t)(new_32 >> 16);   /* div 65536 */
		p->csp_frac = (uint16_t)(new_32 & 0xFFFF);    /* mod 65536 */
	}

	/* Max/min SP */
	if (p->csp >= p->msp) {
		p->csp = p->msp;
		p->csp_frac = 0;
		sp_gain = 0;
	} else if (p->csp < 0) {
		p->csp = 0;
		p->csp_frac = 0;
		sp_gain = 0;
	}

	/* Notice changes */
	if (old_16 != p->csp) {
		p->upkeep->redraw |= (PR_MANA);
	}

	if (sp_gain == 0) {
		/* Recalculate */
		new_32 = ((int32_t) p->csp) * 65536 + p->csp_frac;
		sp_gain = new_32 - old_32;
	}

	return sp_gain;
}

void convert_mana_to_hp(struct player *p, int32_t sp_long) {
	int32_t hp_gain, sp_ratio;

	if (sp_long <= 0 || p->msp == 0 || p->mon.maxhp == p->mon.hp) return;

	/* Total HP from max */
	hp_gain = ((int32_t)(p->mon.maxhp - p->mon.hp)) * 65536;
	hp_gain -= (int32_t)p->chp_frac;

	/* Spend X% of SP get X/2% of lost HP. E.g., at 50% HP get X/4% */
	/* Gain stays low at msp<10 because MP gains are generous at msp<10 */
	/* sp_ratio is max sp to spent sp, doubled to suit target rate. */
	sp_ratio = (((int32_t)MAX(10, (int32_t)p->msp)) * 131072) / sp_long;

	/* Limit max healing to 25% of damage; ergo spending > 50% msp
	 * is inefficient */
	if (sp_ratio < 4) {sp_ratio = 4;}
	hp_gain /= sp_ratio;

	/* DAVIDTODO Flavorful comments on large gains would be fun and informative */

	player_adjust_hp_precise(p, hp_gain);
}


/**
 * Check if the player state has the given OF_ flag.
 */
bool player_of_has(const struct player *p, int flag)
{
	assert(p);
	return of_has(p->mon.state.flags, flag);
}

/**
 * Check if the player resists (or better) an element
 */
bool player_resists(const struct player *p, int element)
{
	return (p->mon.state.el_info[element].res_level > 0);
}

/**
 * Check if the player resists (or better) an element
 */
bool player_is_immune(const struct player *p, int element)
{
	return (p->mon.state.el_info[element].res_level == 3);
}

/**
 * Places the player at the given coordinates in the cave.
 */
void player_place(struct chunk *c, struct player *p, struct loc grid)
{
	assert(p);
	assert(c);
	assert(!square_monster(c, grid));
	assert(square_in_bounds_fully(c, grid));

	/* Save player location */
	p->mon.grid = grid;

	/* Mark cave grid */
	square_set_mon(c, grid, -1);

	/* Clear stair creation */
	p->upkeep->create_down_stair = false;
	p->upkeep->create_up_stair = false;
}

/*
 * Take care of bookkeeping after moving the player with monster_swap().
 *
 * \param p is the player that was moved.
 * \param eval_trap, if true, will cause evaluation (possibly affecting the
 * player) of the traps in the grid.
 * \param is_involuntary, if true, will do appropriate actions (flush the
 * command queue) for a move not expected by the player.
 */
void player_handle_post_move(struct player *p, bool eval_trap,
		bool is_involuntary)
{
	assert(p->cave);
	assert(player->cave);

	/* Handle store doors, or notice objects */
	if (square_isshop(cave, p->mon.grid)) {
		if (player_is_shapechanged(p)) {
			if (square_shopnum(cave, p->mon.grid) == f_info[FEAT_HOME].shopnum) {
				msg("There is a scream and the door slams shut!");
			}
			return;
		}
		disturb(p);
		if (is_involuntary) {
			cmdq_flush();
		}
		event_signal(EVENT_ENTER_STORE);
		event_remove_handler_type(EVENT_ENTER_STORE);
		event_signal(EVENT_USE_STORE);
		event_remove_handler_type(EVENT_USE_STORE);
		event_signal(EVENT_LEAVE_STORE);
		event_remove_handler_type(EVENT_LEAVE_STORE);
	} else {
		if (is_involuntary) {
			cmdq_flush();
		}
		square_know_pile(cave, p->mon.grid, object_not_in_container_predicate);
	}

	/* Discover invisible traps, set off visible ones */
	if (eval_trap && square_isplayertrap(cave, p->mon.grid)
			&& !square_isdisabledtrap(cave, p->mon.grid)) {
		hit_trap(p->mon.grid, 0);
	}

	/* Update view and search */
	update_view(cave, p);
}

/*
 * Something has happened to disturb the player.
 *
 * All disturbance cancels repeated commands, resting, and running.
 *
 * XXX-AS: Make callers either pass in a command
 * or call cmd_cancel_repeat inside the function calling this
 */
void disturb(struct player *p)
{
	/* Cancel repeated commands */
	cmd_cancel_repeat();
	//cmdq_flush();

	/* Cancel Resting */
	if (player_is_resting(p)) {
		player_resting_cancel(p, true);
		p->upkeep->redraw |= PR_STATE;
	}

	/* Cancel running */
	if (p->upkeep->running) {
		p->upkeep->running = 0;
		mem_free(p->upkeep->steps);
		p->upkeep->steps = NULL;

		/* Cancel queued commands */
		cmdq_flush();

		/* Check for new panel if appropriate */
		event_signal(EVENT_PLAYERMOVED);
		p->upkeep->update |= PU_TORCH;

		/* Mark the whole map to be redrawn */
		event_signal_point(EVENT_MAP, -1, -1);
	}

	/* Flush input */
	event_signal(EVENT_INPUT_FLUSH);
}

static bool player_can_search(struct player *p)
{
	if (p->searched_this_turn) return false;
	if (p->mon.m_timed[TMD_BLIND]) return false;
	if (p->mon.m_timed[TMD_CONFUSED]) return false;
	if (p->mon.m_timed[TMD_PHOENIX]) return false;
	if (p->mon.m_timed[TMD_PARALYZED]) return false;
	if (player_timed_grade_eq(p, TMD_STUN, "Knocked Out")) return false;
	if (p->mon.m_timed[TMD_PHOENIX]) return false;
	if (no_light(p)) return false;

	return true;
}

/**
 * Search for traps or secret doors
 */
void search(struct player *p)
{
	if (!player_can_search(p)) return;

	struct loc grid;
	int basepower = p->mon.state.skills[SKILL_SEARCH];// - cave->depth / 4;
	int toroll = basepower /*MAX(cave->depth / 4, basepower)*/ + p->search_turn + 25;
	int roll1 = randint0(toroll) + p->search_turn; // L: higher rolls more likely as you keep searching
	int roll2 = randint0(toroll);
	int detectpower = MIN(roll1, roll2); // - cave->depth / 4;
	int rad;

	if (x_in_y(25, p->search_turn))	++p->search_turn;
	p->searched_this_turn = true;

	if (detectpower < 0) return;

	rad = detectpower / 25;

	/* Search the nearby grids, which are always in bounds */
	// L: add less nearby grids for better searchers, which are not always in bounds
	for (grid.y = (p->mon.grid.y - rad); grid.y <= (p->mon.grid.y + rad); grid.y++) {
		for (grid.x = (p->mon.grid.x - rad); grid.x <= (p->mon.grid.x + rad); grid.x++) {
			int dist;
			struct object *obj;
			struct monster *mon = square_monster(cave, grid);
			//struct feature_kind *featr;
			int currpower;
			struct feature *feat;
			const char *pref, *name;

			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isview(cave, grid)) continue;

			dist = distance(p->mon.grid, grid);

			if (dist > detectpower / 25) continue;

			currpower = detectpower - dist * 10;

			for (feat = square_feat(cave, grid); feat; feat = feat->next) {
				if (feat_is_hidden(p, grid, feat->kind->fidx) && randint0(currpower < cave->depth)) {
					square_memorize_feat_real(p, cave, grid, feat->kind->fidx);

					name = feat->kind->name;
					pref = feat->kind->look_prefix;

					msg("You have discovered %s%s.", name, pref);
				}
			}

			// L: reveal anything hidden
			/*if (tf_has(featr->flags, TF_HIDDEN) && square_ismemorybad(cave, grid) &&
					randint0(currpower) > cave->depth) {
				square_true_memorize(cave, grid);
				msg("You have discovered %s%s",
					square_apparent_look_prefix(p->cave, grid),
					square_apparent_name(p->cave, grid));

				if (OPT(p, disturb_secret)) {
					disturb(p);
				}
			}
			}*/

			/* L: find invisible monsters
			   invisible monsters percieved will get spotted and will be visible until
			   they teleport or move out of range*/
			if (mon && monster_is_invisible(mon) &&
					currpower > randint0(mon->race->level)) {
				char mdesc[128];

				mflag_on(mon->mflag, MFLAG_SPOTTED);
				mflag_on(mon->mflag, MFLAG_KNOWN);

				update_mon(mon, cave, false);

				rf_on(get_lore(mon->race)->flags, RF_INVISIBLE);

				monster_desc(mdesc, sizeof(mdesc), mon, MDESC_SHOW | MDESC_OBJE | MDESC_IND_VIS);
				msg("You have spotted %s", mdesc);
			}

			/* Traps on chests */
			for (obj = square_object(cave, grid); obj; obj = obj->next) {
				if (!obj->known || ignore_item_ok(p, obj)
						|| !is_trapped_chest(obj)) {
					continue;
				}

				if (obj->known->pval != obj->pval) {
					msg("You have discovered a trap on the chest!");
					obj->known->pval = obj->pval;
					disturb(p);
				}
			}
		}
	}

	++p->search_turn;
	p->searched_this_turn = true;
}

/**
 * L: upkeep at start of player's turn
 * is done then rather than when xp is gained to avoid, say,
 * messages while the map is being drawn
 */
void player_start_turn(struct player *p)
{
	int i;

	for (i = 1; i < cave_monster_max(cave); ++i) {
		struct monster *mon = cave_monster(cave, i);

		if (mon) {
			update_mon_state(mon);
		}
	}

	if (p->xp_this_turn) {
		check_learn_powers(p, p->xp_this_turn);
		check_player_monster(p, false);
		player_learn_spell_xp(p, false, p->xp_this_turn);

		p->xp_this_turn = 0;
	}

	if (p->mon.m_timed[TMD_PHOENIX]) {
		p->mon.m_timed[TMD_PHOENIX]--;
		if (!p->mon.m_timed[TMD_PHOENIX]) {
			bool id;
			effect_simple(EF_REBIRTH, source_player(), "0d0", 0, 0, 0, 0, 0, &id);
		}
	}

	if (player_can_search(p)) {
		search(p);
	}
}

bool player_is_invisible(struct player *p)
{
	return monster_is_invisible(&p->mon);
}



#ifdef OBJ_SAVELOAD_DEBUG
static char obj_save_log_file_path[1024] = "";
static char obj_load_log_file_path[1024] = "";
#endif


void init_obj_log_file(bool save)
{
#ifdef OBJ_SAVELOAD_DEBUG
	ang_file *file = NULL;
	if (save) {
		path_build(obj_save_log_file_path, sizeof obj_save_log_file_path, ANGBAND_DIR_USER, "objectsave.log");
		file = file_open(obj_save_log_file_path, MODE_WRITE, FTYPE_TEXT);
		assert(file);
		file_close(file);
	}
	if (!save) {
		path_build(obj_load_log_file_path, sizeof obj_load_log_file_path, ANGBAND_DIR_USER, "objectload.log");
		file = file_open(obj_load_log_file_path, MODE_WRITE, FTYPE_TEXT);
		assert(file);
		file_close(file);
	}
#endif
}


void describe_saveload(const char *msg, bool save)
{
#ifdef OBJ_SAVELOAD_DEBUG
	const char *path = save ? obj_save_log_file_path : obj_load_log_file_path;
	ang_file *file = file_open(path, MODE_APPEND, FTYPE_TEXT);
	assert(file);
	assert(msg);
	assert(file_putf(file, "%s\n", msg));
	assert(file_close(file));
#endif
}

void describe_object_saveload(const struct object *obj, const char *source, bool save)
{
#ifdef OBJ_SAVELOAD_DEBUG
	char desc[80] = "";
	const char *act = save ? "saving" : "loading";

	if (!obj) return;
	strnfmt(desc, sizeof desc, "%s object %i (%s) in %s", act, obj->oidx, obj->kind->name, source);

	//if (my_stristr(source, "objects_aux")) plog(desc);

	describe_saveload(desc, save);
#endif
}

