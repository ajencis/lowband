/**
 * Calculations for monster state
 */

#include "angband.h"
#include "effects.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-util.h"
#include "object.h"
#include "obj-gear.h"
#include "obj-util.h"
#include "monster.h"
#include "mon-attack.h"
#include "player-calcs.h"
#include "player-util.h"
#include "player-attack.h"
#include "project.h"



struct mon_player_match of_matches[] = {
	{ RF_PASS_WEB, OF_PASS_WEB },
	{ RF_INVISIBLE, OF_INVISIBILITY },
	{ RF_HI_REGEN, OF_HI_REGEN },
	{ RF_NONE, -1 }
};

struct mon_player_match pf_matches[] = {
	{ RF_UNDEAD, PF_UNDEAD },
	{ RF_EVIL, PF_EVIL },
	{ RF_PASS_WALL, PF_PASS_WALL },
	{ RF_PHOENIX_RESURRECT, PF_PHOENIX_RESURRECT },
	{ RF_NONE, -1 }
};

enum attack_special_type_ind {
	ATK_SPCL_TYP_NONE = 0,
	ATK_SPCL_TYP_PUNCH,
	ATK_SPCL_TYP_KICK,
	ATK_SPCL_TYP_TOUCH
};


struct attack_special_type {
	int index;
	int dam_stat;
	const char *msg;
	const char *fmsg;
	int dice;
	int eq_slot;
} atk_spcl_types[] = {
	{ ATK_SPCL_TYP_NONE, STAT_NONE, NULL, NULL, 0, EQUIP_NONE },
	{ ATK_SPCL_TYP_PUNCH, STAT_STR, "hits", "hit", 2, EQUIP_WEAPON },
	{ ATK_SPCL_TYP_KICK, STAT_STR, "kicks", "kick", 2, EQUIP_BOOTS },
	{ ATK_SPCL_TYP_TOUCH, STAT_STR, "touches", "touch", 1, EQUIP_WEAPON }
};


struct embryo_attack {
	struct embryo_attack *next;

	int num;

	int dice;
	int sides;
	int to_d;
	int to_h;

	int dam_type;

	int range;

	int skill;
	int acc_stat;
	int dam_stat;

	int blows;

	const char *msg;

	const struct monster_blow *blow;
	const struct object *obj;
	int special_type;

	struct effect *extra;
};


static int mon_lev(const struct monster *mon)
{
	if (mon->player) return mon->player->lev;
	return mon->race->level;
}


static int mon_skill_stat_ind(const struct monster *mon, const struct player_state *state, int skill)
{
	if (mon->player) {
		return player_skill_stat_ind(mon->player, state, skill);
	}
	else {
		const struct magic_realm *r = realms;
		return skill_stat_ind(r, state->stat_ind, skill);
	}
}

static void race_skill(const struct monster *mon, int which, int *base, int *xtra)
{
	const struct monster_race *mr = mon->race;
	*base += mr->skills[which] * mr->level / 50;
	*xtra += mr->skills[which] / 66;
}

static void class_skill(const struct monster *mon, int which, int *base, int *xtra)
{
	struct player *p = mon->player;
	if (p) {
		*base += player_class_c_skill(p, which);
		*xtra += player_class_x_skill(p, which);
	}
}

static void tome_skill(const struct monster *mon, int which, int *base, int *xtra)
{
	struct player *p = mon->player;
	if (p) {
		*base += p->extra_skills[which];
	}
}

static int mon_skill(const struct monster *mon, const struct player_state *state, int skill)
{
	int stat_ind = mon_skill_stat_ind(mon, state, skill);
	int base = 0, xtra = 0, result;

	race_skill(mon, skill, &base, &xtra);
	class_skill(mon, skill, &base, &xtra);
	tome_skill(mon, skill, &base, &xtra);

	result = base + xtra * mon_lev(mon) / PY_MAX_LEVEL;

	if (stat_ind >= 0) {
		result += MAX(result, 0) * adj_stat_skill_percent(stat_ind, skill) / 100;
		result += adj_stat_skill_flat(stat_ind, skill);
	}

	return result;
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


static void mon_stat_calc(const struct monster *mon, struct player_state *state)
{
	int i;
	struct player *p = mon->player;

	for (i = 0; i < STAT_MAX; ++i) {
		int add = state->stat_add[i];
		int ind, use, top;

		if (p) {
			top = modify_stat_value(p->stat_max[i], add);
			use = modify_stat_value(p->stat_cur[i], add);
		}
		else {
			int base = 10;
			//int base = mon->race->level * 8 / 100 + 8;
			add += mon->race->stat_mod[i];

			top = modify_stat_value(base, add);
			use = modify_stat_value(base, add);
		}
		
		if (use <= 3) {
			ind = 0;
		} else if (use <= 18) {
			ind = (use - 3);
		} else if (use <= 18+219) {
			ind = (15 + (use - 18) / 10);
		} else {
			ind = (37);
		}

		state->stat_ind[i] = ind;
		state->stat_use[i] = use;
		state->stat_top[i] = top;
	}
}


int mon_power(const struct monster_race *mon, int power)
{
	int scale = mon->powers[power];

	if (scale <= 0) return scale;

	int normal = scale * mon->level / 100;
	int special = scale * mon->level / 50 + scale * scale / 50 - 100 * 100 / 50;

	// a low-level monster with slow scaling doesn't get the power at all;
	return MAX(0, MIN(normal, special));
}

static void get_mon_ac(struct monster *mon, struct player_state *state)
{
	int base_ac = mon->race->ac / 3;
	int base_to = mon->race->ac - base_ac;
	int ac = 0, to_a = 0;
	uint16_t i;
	struct object *obj;

	for (i = 0; i < mon->body.count; ++i) {
		obj = mon->body.slots[i].obj;
		if (obj) {
			ac += obj->ac;
			to_a += object_to_ac(obj);
		}
	}

	ac = MAX(base_ac, ac) + MIN(base_ac, ac) / 2;
	to_a = MAX(base_to, to_a) + MIN(base_to, to_a) / 2;

	state->ac = ac;
	state->to_a = to_a;
}


void calc_mon_bonuses(struct monster *mon, struct player_state *state)
{
	int i, extra_blows = 0;
	struct element_info race_elem_info[ELEM_MAX] = { 0 };
	struct monster_race *mrace = mon->race;

	memset(state, 0, sizeof *state);

	get_mon_ac(mon, state);
	state->speed = mon->race->speed;

	pf_wipe(state->pflags);
	of_wipe(state->flags);

	pf_union(state->pflags, mrace->base->pflags);
	of_union(state->flags, mrace->base->oflags);


	mon_stat_calc(mon, state);


	for (i = 0; i < PP_MAX; ++i) {
		state->powers[i] = mon_power(mrace, i);
	}

	for (i = 0; i < SKILL_MAX; i++) {
		state->skills[i] = mon_skill(mon, state, i);
		/*int stat_ind = mon_skill_stat_ind(mon, state, i);

		if (i == SKILL_HEALTH) state->skills[i] = mrace->avg_hp;
		else state->skills[i] = (mrace->skills[i] + 66) * mrace->level / 66;
		state->skills[i] = mon_lev(mon) + 10;*/
	}


	memcpy(race_elem_info, mrace->el_info, sizeof *race_elem_info * ELEM_MAX);
	for (i = 0; i < ELEM_MAX; i++) {
		state->el_info[i].res_level = race_elem_info[i].res_level;
	}
	

	state->el_info[ELEM_HOLY_FIRE].res_level = state->el_info[ELEM_HOLY_ORB].res_level * 2 + state->el_info[ELEM_FIRE].res_level;
	state->el_info[ELEM_HELLFIRE].res_level = state->el_info[ELEM_FIRE].res_level + pf_has(state->pflags, PF_EVIL) ? 0 : -1;


	for (i = 0; of_matches[i].mval != RF_NONE; ++i) {
		if (rf_has(mrace->flags, of_matches[i].mval)) {
			of_on(state->flags, of_matches[i].pval);
		}
	}

	for (i = 0; pf_matches[i].mval != RF_NONE; ++i) {
		if (rf_has(mrace->flags, pf_matches[i].mval)) {
			pf_on(state->pflags, pf_matches[i].pval);
		}
	}

		
	if (mon->m_timed[TMD_INVULN]) {
		state->to_a += 100;
	}
	if (mon->m_timed[TMD_BLESSED]) {
		state->to_a += 5;
		state->to_h += 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (mon->m_timed[TMD_SHIELD]) {
		state->to_a += 50;
	}
	if (mon->m_timed[TMD_STONESKIN]) {
		state->to_a += 40;
		state->speed -= 5;
	}
	if (mon->m_timed[TMD_HERO]) {
		state->to_h += 12;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (mon->m_timed[TMD_SHERO]) {
		state->skills[SKILL_TO_HIT_MELEE] += 75;
		state->to_a -= 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 10, 0);
	}
	if (mon->m_timed[TMD_FAST] || mon->m_timed[TMD_SPRINT]) {
		state->speed += 10;
	}
	if (mon->m_timed[TMD_SLOW]) {
		state->speed -= 10;
	}
	if (mon->m_timed[TMD_SINFRA]) {
		state->see_infra += 5;
	}
	if (mon->m_timed[TMD_TERROR]) {
		state->speed += 10;
	}
	for (i = 0; i < TMD_MAX; ++i) {
		if (mon->m_timed[i] && timed_effects[i].temp_resist != -1
				&& state->el_info[timed_effects[i].temp_resist].res_level
				< 2) {
			state->el_info[timed_effects[i].temp_resist].res_level++;
		}
	}
	if (mon->m_timed[TMD_CONFUSED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 4, 0);
	}
	if (mon->m_timed[TMD_AMNESIA]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (mon->m_timed[TMD_POISONED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}
	if (mon->m_timed[TMD_IMAGE]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (mon->m_timed[TMD_BLOODLUST]) {
		int p_berserk = get_power_scale_state(state, PP_BERSERK, 150, mon->race->level);
		int bonus = mon->m_timed[TMD_BLOODLUST] * (100 + p_berserk) / 100;

		state->to_d += bonus / 5 + 1;
		state->to_h += bonus * 2 / 3;
		extra_blows += bonus * 4;
		state->speed += bonus / 5 - 3;
		state->dam_red += bonus * (mon->maxhp + 100) / 1000;
		adjust_skill_scale(&state->skills[SKILL_STEALTH], -bonus, 5, 10);
		adjust_skill_scale(&state->skills[SKILL_SAVE], bonus, 20, 10);
	}
	if (mon->m_timed[TMD_STEALTH]) {
		state->skills[SKILL_STEALTH] += 10;
	}

	if (state->skills[SKILL_DIGGING] < 1) state->skills[SKILL_DIGGING] = 1;
	if (state->skills[SKILL_STEALTH] > 150) state->skills[SKILL_STEALTH] = 150;
	if (state->skills[SKILL_HEALTH] < 3) state->skills[SKILL_HEALTH] = 3;
}








static void effect_add_value(struct effect *ef, random_value rv)
{
	char dice_str[80];
	dice_t *dice;

	if (ef->dice) {
		dice = ef->dice;
	}
	else {
		dice = dice_new();
	}

	strnfmt(dice_str, sizeof dice_str, "%id%i", rv.dice, rv.sides);

	if (rv.base) {
		my_strcat(dice_str, format("%+i", rv.base), sizeof dice_str);
	}

	if (rv.m_bonus) {
		my_strcat(dice_str, format("M%i", rv.m_bonus), sizeof dice_str);
	}

	dice_parse_string(dice, dice_str);

	ef->dice = dice;
}


typedef void (*emb_atk_mod_fn)(const struct monster *, struct embryo_attack *);

static void emb_atk_mod_death_touch(const struct monster *mon, struct embryo_attack *emb)
{
	int div;
	struct effect *ef;
	random_value rv = { 0, 0, 0, 0 };

	if (!mon_has_power(mon, PP_DEATH_TOUCH)) return;
	if (emb->obj) return;

	div = emb->blow ? 2 : 1;
	rv.sides = get_mon_power_scale(mon, PP_DEATH_TOUCH, 50 / div);
	rv.dice = 1;

	ef = mem_zalloc(sizeof *ef);

	ef->index = EF_HIT;
	ef->subtype = PROJ_NETHER;
	effect_add_value(ef, rv);

	ef->next = emb->extra;
	emb->extra = ef;
}

emb_atk_mod_fn mod_fns[] = {
	emb_atk_mod_death_touch
};


static void calc_emb_blows(const struct monster *mon, struct embryo_attack *emb, int numblows)
{
	int wgt = emb->obj ? object_weight_one(emb->obj) : 0;
	int div = wgt * 2 + 100;
	bool has_acc = emb->acc_stat != STAT_NONE, has_dam = emb->dam_stat != STAT_NONE;

	int sdiv = 0, sind = 0;
	int base, skill, blows;

	if (has_dam) {
		sind += mon->state.stat_ind[emb->dam_stat];
		++sdiv;
	}
	if (has_acc) {
		sind += mon->state.stat_ind[emb->acc_stat];
		++sdiv;
	}

	if (sdiv > 0) {
		base = adj_stat_blow(sind / sdiv);
	}
	else {
		base = adj_stat_blow(AVG_STAT_IND);
	}

	skill = mon->state.skills[emb->skill];

	blows = skill * base * emb->num / div / numblows;

	emb->blows = MAX(blows + 50 * emb->num, blows / 2 + 100 * emb->num);
}

static int num_embryos(const struct embryo_attack *emb)
{
	int count = 0;
	const struct embryo_attack *curr;
	for (curr = emb; curr; curr = curr->next) {
		count += emb->num;
	}
	return count;
}



static void modify_unarmed_attack(struct embryo_attack *emb, const struct monster *mon)
{
	if (emb->obj) return;

	int factor = MIN(125 - (emb->dice * emb->sides * 2), 100);
	int dexmin = 75 - factor / 2;

	emb->sides += get_mon_power_scale(mon, PP_UNARMED_STRIKE, 10) * factor / 100;
	
	if (emb->acc_stat == PP_UNARMED_STRIKE && mon_power_minimum(mon, PP_UNARMED_STRIKE, dexmin)) {
		emb->acc_stat = STAT_DEX;
	}
}



static struct embryo_attack *get_weapon_attack(const struct monster *mon, const struct object *weap)
{
	assert(weap);

	if (weap->tval == TV_SHIELD) return NULL;

	bool p = mon->player ? true : false;

	struct embryo_attack *emb = mem_zalloc(sizeof *emb);

	emb->obj = weap;

	emb->skill = SKILL_TO_HIT_MELEE;

	emb->acc_stat = STAT_NONE;
	emb->dam_stat = STAT_STR;

	emb->dice = weap->dd;
	emb->sides = weap->ds;

	emb->to_h = object_to_hit(weap) * BTH_PLUS_ADJ;
	emb->sides += object_to_dam(weap);

	emb->to_d = 0;

	emb->msg = p ? "hit" : "hits";

	emb->extra = NULL;

	emb->num = 1;

	emb->dam_type = weap->kind->base->proj_type;

	emb->range = weap->kind->base->tval == TV_POLEARM ? 2 : 1;

	return emb;
}

static struct embryo_attack *get_natural_attack(const struct monster *mon, const struct monster_blow *blow)
{
	bool p = mon->player ? true : false;
	struct embryo_attack *emb = mem_zalloc(sizeof *emb);

	emb->blow = blow;

	emb->skill = SKILL_TO_HIT_MELEE;

	emb->acc_stat = STAT_NONE;
	emb->dam_stat = STAT_STR;

	emb->dice = blow->dice.dice;
	emb->sides = blow->dice.sides;
	emb->to_d = blow->dice.base;

	emb->msg = p ? blow->method->fmessage : blow->method->messages->act_msg;

	emb->extra = NULL;

	emb->num = 1;

	if (blow->effect->lash_type == -1) {
		emb->dam_type = blow->method->lash_type;
	}
	else {
		emb->dam_type = blow->effect->lash_type;
	}

	emb->range = monster_melee_attack_range(mon->race->level, blow);

	return emb;
}

static struct embryo_attack *get_special_attack(const struct monster *mon, int special)
{
	struct embryo_attack *emb = mem_zalloc(sizeof *emb);
	const struct attack_special_type *data = &atk_spcl_types[special];
	bool p = mon->player ? true : false;

	emb->obj = NULL;

	emb->skill = SKILL_TO_HIT_MELEE;

	emb->acc_stat = STAT_NONE;
	emb->dam_stat = data->dam_stat;

	emb->dice = data->dice;
	emb->sides = 1;
	
	emb->msg = p ? data->fmsg : data->msg;

	emb->extra = NULL;

	emb->num = 1;

	emb->dam_type = PROJ_BLUDGEONING;

	return emb;
}



static void num_slots(const struct monster *mon, int empty[EQUIP_MAX], int total[EQUIP_MAX])
{
	int i;

	for (i = 0; i < mon->body.count; ++i) {
		int slot = mon->body.slots[i].type;

		if (slot != EQUIP_NONE) {
			++total[i];
			if (!mon->body.slots[i].obj) {
				++empty[i];
			}
		}
	}
}

static void num_natural_attacks(const struct monster *mon, int array[EQUIP_MAX])
{
	int i;
	for (i = 0; i < z_info->mon_blows_max && mon->race->blow[i].method; ++i) {
		const struct monster_blow *mb = &mon->race->blow[i];
		int eq_slot = mb->method->equip_slot;

		if (eq_slot != EQUIP_NONE) {
			++array[eq_slot];
		}
	}
}

static void add_attack_to_end(struct embryo_attack **src, struct embryo_attack *new)
{
	struct embryo_attack **last = src;

	while (*last) {
		last = &((*last)->next);
	}

	*last = new;
}

static struct embryo_attack *add_unarmed(const struct monster *mon, int source, int slot, int num, int remaining_slots[EQUIP_MAX])
{
	struct embryo_attack *new = NULL;

	num = MIN(num, remaining_slots[EQUIP_MAX]);

	remaining_slots[EQUIP_MAX] -= num;

	if (num > 0) {
		new = get_special_attack(mon, source);
		new->num = num;
	}

	return new;
}

static struct embryo_attack *get_unarmed(const struct monster *mon, int remaining_slots[EQUIP_MAX], bool has_attacks)
{
	int num_punches = get_mon_power_scale(mon, PP_UNARMED_STRIKE, 3);
	int num_kicks = get_mon_power_scale(mon, PP_UNARMED_STRIKE, 1);
	int num_touches = mon_has_power(mon, PP_DEATH_TOUCH) && num_punches <= 0 ? 1 : 0;
	bool has_new_attacks = num_punches > 0 || num_kicks > 0 || num_touches > 0;
	struct embryo_attack *new, *result = NULL;

	if (!has_attacks && !has_new_attacks) {
		num_punches = 1;
	}

	new = add_unarmed(mon, ATK_SPCL_TYP_PUNCH, EQUIP_WEAPON, num_punches, remaining_slots);
	if (new) add_attack_to_end(&result, new);
	new = add_unarmed(mon, ATK_SPCL_TYP_KICK, EQUIP_BOOTS, num_kicks, remaining_slots);
	if (new) add_attack_to_end(&result, new);
	new = add_unarmed(mon, ATK_SPCL_TYP_TOUCH, EQUIP_WEAPON, num_touches, remaining_slots);
	if (new) add_attack_to_end(&result, new);

	return result;
}

static struct embryo_attack *init_mon_attacks(const struct monster *mon)
{
	int avail_slots[EQUIP_MAX] = { 0 }, total_slots[EQUIP_MAX] = { 0 };
	int num_naturals[EQUIP_MAX] = { 0 };
	int remaining_slots[EQUIP_MAX];
	struct embryo_attack *result = NULL, *new;
	const struct monster_blow *blows = mon->race->blow;
	int i;

	num_slots(mon, avail_slots, total_slots);
	num_natural_attacks(mon, num_naturals);

	for (i = 0; i < EQUIP_MAX; ++i) {
		if (total_slots[i] <= 0) {
			remaining_slots[i] = num_naturals[i];
		}
		else if (total_slots[i] < num_naturals[i]) {
			remaining_slots[i] = avail_slots[i] * num_naturals[i];
			avail_slots[i] /= total_slots[i];
		}
		else {
			remaining_slots[i] = avail_slots[i];
		}
	}

	for (i = 0; i < mon->body.count; ++i) {
		const struct object *obj = mon->body.slots[i].obj;
		int slot = mon->body.slots[i].type;

		if (slot == EQUIP_WEAPON && obj) {
			new = get_weapon_attack(mon, obj);

			add_attack_to_end(&result, new);
		}
	}

	for (i = 0; i < z_info->mon_blows_max && blows[i].method; ++i) {
		const struct monster_blow *blow = &blows[i];
		int slot = blow->method->equip_slot;

		if (slot != EQUIP_NONE) {
			if (remaining_slots[slot] <= 0) continue;
			--remaining_slots[slot];
		}

		new = get_natural_attack(mon, blow);

		add_attack_to_end(&result, new);
	}

	new = get_unarmed(mon, remaining_slots, result ? true : false);

	if (new) {
		add_attack_to_end(&result, new);
	}

	return result;
}

static void free_atk_embryo(struct embryo_attack *emb)
{
	mem_free(emb);
}



static void hatch_attack_embryo(struct embryo_attack *emb, struct monster *mon)
{
	struct effect *main;
	struct attack *result;
	random_value rv = { 0, 0, 0, 0 };

	if (emb->acc_stat >= 0 && emb->acc_stat < STAT_MAX) {
		int ind = mon->state.stat_ind[emb->acc_stat];
		emb->to_h += adj_dex_th(ind);
	}
	if (emb->dam_stat >= 0 && emb->dam_stat < STAT_MAX) {
		int ind = mon->state.stat_ind[emb->dam_stat];
		emb->sides += adj_str_td(ind);
	}

	main = mem_zalloc(sizeof *main);

	rv.base = emb->to_d;
	rv.dice = emb->dice;
	rv.sides = emb->sides;
	rv.m_bonus = 0;

	main->index = EF_HIT;
	main->subtype = emb->dam_type;
	assert(main->subtype >= 0 && main->subtype < PROJ_MAX);
	main->next = emb->extra;

	effect_add_value(main, rv);

	result = mem_zalloc(sizeof *result);

	result->ef = main;
	result->blows = emb->blows;
	result->message = emb->msg;
	result->obj = emb->obj;
	result->range = emb->range;
	result->to_hit = emb->to_h;
	if (emb->skill >= 0 && emb->skill < SKILL_MAX) {
		result->to_hit += mon->state.skills[emb->skill];
	}

	for (struct effect *ef = result->ef; ef; ef = ef->next) {
		assert(effect_valid(ef));
	}

	// add to end
	if (mon->atk) {
		struct attack *last = mon->atk;
		while (last->next) {
			last = last->next;
		}
		last->next = result;
	}
	else {
		mon->atk = result;
	}
}


static void get_mon_attacks(struct monster *mon)
{
	struct embryo_attack *emb = init_mon_attacks(mon), *curr, *next;
	int count = num_embryos(emb), i;

	curr = emb;
	while (curr) {
		next = curr->next;

		modify_unarmed_attack(curr, mon);

		calc_emb_blows(mon, curr, count);

		for (i = N_ELEMENTS(mod_fns) - 1; i >= 0; --i) {
			mod_fns[i](mon, curr);
		}

		hatch_attack_embryo(curr, mon);

		free_atk_embryo(curr);

		curr = next;
	}
}




static void free_attack(struct attack *atk)
{
	free_effect(atk->ef);
	mem_free(atk);
}

void free_mon_attacks(struct monster *mon)
{
	struct attack *atk, *nxt;

	atk = mon->atk;
	while (atk) {
		nxt = atk->next;
		free_attack(atk);
		atk = nxt;
	}

	mon->atk = NULL;
}


void update_mon_attacks(struct monster *mon)
{
	free_mon_attacks(mon);
	get_mon_attacks(mon);
}


void update_mon_state(struct monster *mon)
{
	if (mflag_has(mon->mflag, MFLAG_UPDATE_STATE)) {
		calc_mon_bonuses(mon, &mon->state);
		mflag_off(mon->mflag, MFLAG_UPDATE_STATE);
	}

	if (mflag_has(mon->mflag, MFLAG_UPDATE_ATTACKS) && mon_is_player(mon)) {
		update_mon_attacks(mon);
		mflag_off(mon->mflag, MFLAG_UPDATE_ATTACKS);
	}
}

