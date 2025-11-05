/**
 * Calculations for monster state
 */

#include "angband.h"
#include "effects.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "obj-properties.h"
#include "obj-tval.h"
#include "object.h"
#include "obj-curse.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-util.h"
#include "monster.h"
#include "mon-attack.h"
#include "mon-spell.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-enum.h"
#include "player-properties.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "player.h"
#include "project.h"
#include "z-util.h"



struct mon_player_match of_matches[] = {
	{ RF_PASS_WEB, OF_PASS_WEB },
	{ RF_INVISIBLE, OF_INVISIBILITY },
	{ RF_HI_REGEN, OF_HI_REGEN },
	{ RF_PASS_TREE, OF_PASS_TREE },
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
	ATK_SPCL_TYP_TOUCH,
	ATK_SPCL_TYP_CHAIN
};


struct attack_special_type {
	int index;
	int dam_stat;
	const char *title;
	const char *msg;
	const char *fmsg;
	int dice;
	int eq_slot;
} atk_spcl_types[] = {
	{ ATK_SPCL_TYP_NONE, STAT_NONE, NULL, NULL, NULL, 0, EQUIP_NONE },
	{ ATK_SPCL_TYP_PUNCH, STAT_STR, "punch", "punches {target}", "punch {target}", 2, EQUIP_WEAPON },
	{ ATK_SPCL_TYP_KICK, STAT_STR, "kick", "kicks {target}", "kick {target}", 2, EQUIP_BOOTS },
	{ ATK_SPCL_TYP_TOUCH, STAT_STR, "touch", "touches {target}", "touch {target}", 1, EQUIP_WEAPON },
	{ ATK_SPCL_TYP_CHAIN, STAT_DEX, "enchain", "enchains {target}", "enchain {target}", 1, EQUIP_BODY_ARMOR }
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

	int crit_chance;

	int auto_freq;

	const char *msg;
	char title[32];

	const struct monster_blow *mon_blow;
	const struct object *obj;
	int special_type;

	struct effect *extra;
};


int mon_lev(const struct monster *mon)
{
	if (mon->player) return mon->player->lev;
	return mon->race->level;
}


static int skill_stepdown(const struct monster *mon, int skill)
{
	int lev = mon_lev(mon);
	int diff;

	lev = MAX(lev / 2 + 25, lev);

	if (skill <= lev) return skill;

	diff = skill - lev;
	diff = diff / 2;

	return lev + diff;
}

void race_skill(const struct monster *mon, int which, int *base, int *xtra)
{
	const struct monster_race *mr = mon->race;
	*base += mr->skills[which] / 10;
	*xtra += mr->skills[which];
}

void class_skill(const struct monster *mon, int which, int *base, int *xtra)
{
	struct player *p = mon->player;
	if (p) {
		*base += player_class_c_skill(p, which);
		*xtra += player_class_x_skill(p, which);
	}
}

void tome_skill(const struct monster *mon, int which, int *base, int *xtra)
{
	struct player *p = mon->player;
	if (p) {
		*base += p->extra_skills[which];
	}
}

static int stat_bonus_index(const struct player_state *state, int stat1, int stat2, char *buf, size_t bufsize)
{
	int sum = 0, div = 0;

	assert(stat1 != STAT_NONE);

	if (buf) buf[0] = '\0';

	sum += state->stat_ind[stat1];
	++div;
	if (buf) my_strcat(buf, stat_idx_to_name(stat1), bufsize);

	if (stat2 != STAT_NONE && (state->stat_ind[stat1] < state->stat_ind[stat2])) {
		sum += state->stat_ind[stat2];
		++div;
		if (buf) {
			my_strcat(buf, " and ", bufsize);
			my_strcat(buf, stat_idx_to_name(stat2), bufsize);
		}
	}

	return sum / div;
}

int stat_skill_bonus(const struct monster *mon, const struct player_state *state, int which, int curr, char *buf, size_t bufsize)
{
	const struct magic_realm *r = mon->player && mon->player->realm ? mon->player->realm : realms;
	int stat1, stat2;
	int sum = 0, div = 0;
	int result = 0;

	bool use_stat2;

	if (buf) buf[0] = '\0';

	skill_stat(r, state->stat_ind, which, &stat1, &stat2);

	use_stat2 = (stat2 != STAT_NONE) && ((stat1 == STAT_NONE) || (state->stat_ind[stat1] < state->stat_ind[stat2]));

	if (stat1 != STAT_NONE) {
		sum += state->stat_ind[stat1];
		++div;
		if (buf) {
			my_strcat(buf, stat_idx_to_name(stat1), bufsize);
		}
	}
	if (buf && stat1 != STAT_NONE && use_stat2) {
		my_strcat(buf, " and ", bufsize);
	}
	if (use_stat2) {
		sum += state->stat_ind[stat2];
		++div;
		if (buf) {
			my_strcat(buf, stat_idx_to_name(stat2), bufsize);
		}
	}

	if (div <= 0) return 0;

	result += MAX(curr, 0) * adj_stat_skill_percent(sum / div, which) / 100;
	result += adj_stat_skill_flat(sum / div, which);

	return result;
}

static int mon_skill(const struct monster *mon, const struct player_state *state, int skill)
{
	int base = 0, xtra = 0, result;

	race_skill(mon, skill, &base, &xtra);
	class_skill(mon, skill, &base, &xtra);
	tome_skill(mon, skill, &base, &xtra);

	result = base + xtra * mon_lev(mon) / 50;

	result += stat_skill_bonus(mon, state, skill, result, NULL, 0);

	/*if (stat_ind >= 0) {
		result += MAX(result, 0) * adj_stat_skill_percent(stat_ind, skill) / 100;
		result += adj_stat_skill_flat(stat_ind, skill);
	}*/

	return skill_stepdown(mon, result);
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


int mon_class_power(const struct monster *mon, int power)
{
	if (!mon->player) return 0;

	int base = player_class_power(mon->player, power);
	int lev = mon_lev(mon);
	int missing_base = 100 - base;
	int stepdown;

	if (base < 0) return base;

	missing_base *= 25 - lev;

	stepdown = 100 - missing_base;
	return MAX(0, MIN(base, stepdown));

	if (mon->player) return player_class_power(mon->player, power);
	return 0;
}

int calc_mon_race_power(const struct monster_race *mr, int power)
{
	int scale = mr->powers[power];

	if (scale <= 0) return scale;

	int normal = scale * mr->level / 100;
	int special = scale * mr->level / 50 + scale * scale / 50 - 100 * 100 / 50;

	// a low-level monster with slow scaling doesn't get the power at all;
	return MAX(0, MIN(normal, special));
}

int mon_race_power(const struct monster *mon, int power)
{
	return calc_mon_race_power(mon->race, power);
}

int mon_tome_power(const struct monster *mon, int power)
{
	if (mon->player) {
		return (mon->player->extra_powers[power] + 1) / 2;
	}
	return 0;
}

static int mon_power(const struct monster *mon, int power)
{
	if (power <= PP_NONE || power >= PP_MAX) return 0;

	struct player_ability *abil = lookup_player_ability(power, PY_ABIL_POWER);
	assert(abil);

	int scale = mon_class_power(mon, power) + mon_race_power(mon, power);
	int lev = mon_lev(mon);

	int xtra = mon_tome_power(mon, power);

	if (scale <= 0) return scale + xtra;
	int result = scale * lev / 50 + xtra;

	return result;
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
	int i, j;
	int extra_blows = 0, extra_shots = 0, extra_might = 0, extra_moves = 0;
	int curr_light = 0;
	int arm_wgt = 0;
	//struct element_info race_elem_info[ELEM_MAX] = { 0 };
	struct monster_race *mrace = mon->race;
	bitflag f[OF_SIZE];

	verify_mon_ownership(mon);

	memset(state, 0, sizeof *state);

	get_mon_ac(mon, state);
	state->to_a += adj_dex_ta(stat_bonus_index(state, STAT_DEX, STAT_INT, NULL, 0));
	state->speed = mon->race->speed;

	pf_wipe(state->pflags);
	of_wipe(state->flags);

	pf_union(state->pflags, mrace->base->pflags);
	of_union(state->flags, mrace->base->oflags);


	mon_stat_calc(mon, state);


	for (i = PP_NONE + 1; i < PP_MAX; ++i) {
		state->powers[i] = mon_power(mon, i);
	}

	for (i = 0; i < SKILL_MAX; i++) {
		state->skills[i] = mon_skill(mon, state, i);
		/*int stat_ind = mon_skill_stat_ind(mon, state, i);

		if (i == SKILL_HEALTH) state->skills[i] = mrace->avg_hp;
		else state->skills[i] = (mrace->skills[i] + 66) * mrace->level / 66;
		state->skills[i] = mon_lev(mon) + 10;*/
	}

	for (i = 0; i < ELEM_MAX; ++i) {
		state->el_info[i].res_level = mrace->el_info[i].res_level;
	}
	//memcpy(state->el_info, mrace->el_info, sizeof *race_elem_info * ELEM_MAX);

	/*for (i = 0; i < ELEM_MAX; i++) {
		state->el_info[i].res_level = race_elem_info[i].res_level;
	}*/


	state->el_info[ELEM_HOLY_FIRE].res_level = state->el_info[ELEM_HOLY_ORB].res_level * 2 + state->el_info[ELEM_FIRE].res_level;
	state->el_info[ELEM_HELLFIRE].res_level = state->el_info[ELEM_FIRE].res_level + (pf_has(state->pflags, PF_EVIL) ? 0 : -1);


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


	for (i = 0; i < mon->body.count; ++i) {
		struct object *obj = slot_object(mon, i);
		int dig, index = 0, light_amt;
		struct curse_data *curse;

		if (!obj) continue;

		if (tval_is_armor(obj)) {
			arm_wgt = MAX(object_weight_one(obj), arm_wgt);
		}

		while (obj) {
			object_flags(obj, f);
			of_union(state->flags, f);

			state->stat_add[STAT_STR] += obj->modifiers[OBJ_MOD_STR];
			state->stat_add[STAT_INT] += obj->modifiers[OBJ_MOD_INT];
			state->stat_add[STAT_WIS] += obj->modifiers[OBJ_MOD_WIS];
			state->stat_add[STAT_DEX] += obj->modifiers[OBJ_MOD_DEX];
			state->stat_add[STAT_CON] += obj->modifiers[OBJ_MOD_CON];
			state->skills[SKILL_STEALTH] += obj->modifiers[OBJ_MOD_STEALTH];
			state->skills[SKILL_SEARCH] += (obj->modifiers[OBJ_MOD_SEARCH] * 5);
			state->see_infra += obj->modifiers[OBJ_MOD_INFRA];
			if (tval_is_digger(obj)) {
				if (of_has(obj->flags, OF_DIG_1)) {
					dig = 1;
				} else if (of_has(obj->flags, OF_DIG_2)) {
					dig = 2;
				} else if (of_has(obj->flags, OF_DIG_3)) {
					dig = 3;
				}
			}
			
			dig += obj->modifiers[OBJ_MOD_TUNNEL];
			state->skills[SKILL_DIGGING] += (dig * 20);
			state->speed += obj->modifiers[OBJ_MOD_SPEED];
			state->dam_red += obj->modifiers[OBJ_MOD_DAM_RED];
			extra_blows += obj->modifiers[OBJ_MOD_BLOWS] * 100;
			extra_shots += obj->modifiers[OBJ_MOD_SHOTS];
			extra_might += obj->modifiers[OBJ_MOD_MIGHT];
			extra_moves += obj->modifiers[OBJ_MOD_MOVES];


			if (of_has(obj->flags, OF_LIGHT_3)) {
				light_amt = 3;
			} else if (of_has(obj->flags, OF_LIGHT_2)) {
				light_amt = 2;
			} else {
				light_amt = 0;
			}

			light_amt += obj->modifiers[OBJ_MOD_LIGHT];

			if (tval_is_light(obj) && !of_has(obj->flags, OF_NO_FUEL) &&
					obj->timeout == 0) {
				light_amt = 0;
			}

			curr_light += light_amt;


			for (j = 0; j < ELEM_MAX; ++j) {
				state->el_info[j].res_level += obj->el_info[j].res_level;
			}

			if (!slot_type_is(mon, i, EQUIP_WEAPON)
					&& !slot_type_is(mon, i, EQUIP_BOW)) {
				state->to_h += obj->to_h;
				state->to_d += obj->to_d;
			}

			curse = obj->curses;

			/* Move to any unprocessed curse object */
			if (curse) {
				index++;
				obj = NULL;
				while (index < z_info->curse_max) {
					if (curse[index].power) {
						obj = curses[index].obj;
						break;
					} else {
						index++;
					}
				}
			} else {
				obj = NULL;
			}
		}
	}

	if (rf_has(mon->race->flags, RF_NEVER_MOVE)) {
		extra_moves -= 25;
	}

	curr_light -= get_power_scale_state(state, PP_UNLIGHT, UNLIGHT_MAX_POWER);
	curr_light += get_power_scale_state(state, PP_GLOW, UNLIGHT_MAX_POWER);

	if (mon->state.cur_light != curr_light) {
		player->upkeep->update |= PU_UPDATE_VIEW;
	}

	state->cur_light = curr_light;

	unarmoured_ac_bonus(mon, state, arm_wgt);
	unarmoured_speed_bonus(mon, state, arm_wgt);

	calc_glow(mon, state);
	calc_unlight(mon, state);

	calc_running(mon, state);

	for (i = 0; i < TMD_MAX; ++i) {
		if (mon->m_timed[i] && timed_effects[i].oflag_dup != OF_NONE
				&& i != TMD_TRAPSAFE) {
			of_on(f, timed_effects[i].oflag_dup);
		}
	}

	if (mon->m_timed[TMD_STUN]) {
		int penalty = monster_effect_level(mon, TMD_STUN);

		state->to_h -= penalty * 5;
		state->to_d -= penalty * 5;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -penalty, 10, 0);
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
		int p_berserk = get_power_scale_state(state, PP_BERSERK, 150);
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

	if (of_has(state->flags, OF_AFRAID)) {
		state->to_h -= 20;
		state->to_a += 8;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}

	if (state->skills[SKILL_DIGGING] < 1) state->skills[SKILL_DIGGING] = 1;
	if (state->skills[SKILL_STEALTH] > 150) state->skills[SKILL_STEALTH] = 150;
	if (state->skills[SKILL_HEALTH] < 3) state->skills[SKILL_HEALTH] = 3;


	/* Analyze flags - check for fear */
	if (of_has(state->flags, OF_AFRAID)) {
		state->to_h -= 20;
		state->to_a += 8;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}


	mflag_on(mon->mflag, MFLAG_UPDATE_ATTACKS);

	verify_mon_ownership(mon);
}








static void effect_add_value(struct effect *ef, random_value rv)
{
	if (!ef->dice) ef->dice = dice_new();

	dice_parse_random_value(ef->dice, rv);
}


typedef void (*emb_atk_mod_fn)(const struct monster *, struct embryo_attack *);

static void emb_atk_mod_death_touch(const struct monster *mon, struct embryo_attack *emb)
{
	int div;
	struct effect *ef;
	random_value rv = { 0, 0, 0, 0 };

	if (!mon_has_power(mon, PP_DEATH_TOUCH)) return;
	if (emb->obj) return;

	div = emb->mon_blow ? 2 : 1;
	rv.sides = get_power_scale(mon, PP_DEATH_TOUCH, 50 / div);
	rv.dice = 1;

	ef = mem_zalloc(sizeof *ef);

	ef->index = EF_HIT;
	ef->subtype = PROJ_NETHER;
	effect_add_value(ef, rv);

	ef->next = emb->extra;
	emb->extra = ef;
}

static struct effect *breath_bite_ef(int innate, const struct monster *mon)
{
	const struct monster_spell *spell = monster_spell_by_index(innate);
	struct effect *ef_new;
	const struct effect *ef_src;
	random_value rv;
	int power_mod = 10 + get_power_scale(mon, PP_BREATH_BITE, 40);
	int prev_cmc;

	ef_src = spell->effect;
	while (ef_src && ef_src->index != EF_BREATH) {
		ef_src = ef_src->next;
	}

	if (!ef_src) return NULL;
	assert(effect_valid(ef_src));

	ef_new = mem_zalloc(sizeof *ef_new);
	memcpy(ef_new, ef_src, sizeof *ef_new);
	ef_new->msg = ef_src->msg ? string_make(ef_src->msg) : NULL;
	ef_new->monster = NULL;
	ef_new->next = NULL;
	ef_new->dice = NULL;

	prev_cmc = cave->mon_current;
	cave->mon_current = mon->midx;

	dice_random_value(ef_src->dice, &rv);

	rv.base *= power_mod;
	rv.base += 100 - 1;
	rv.base /= 100;

	effect_add_value(ef_new, rv);

	cave->mon_current = prev_cmc;

	return ef_new;
}

static void emb_atk_mod_breath_bite(const struct monster *mon, struct embryo_attack *emb)
{
	struct effect *result = NULL, *ef_temp;
	int num_breaths = 0, innate;
	struct monster_race *mr = mon->race;

	if (!mon_has_power(mon, PP_BREATH_BITE)) return;
	if (!emb->mon_blow) return;
	if (!streq(emb->mon_blow->method->name, "BITE")) return;

	for (innate = 0; innate < RSF_MAX; ++innate) {
		if (!spell_is_castable_innately(mr, innate)) continue;
		if (!rsf_has(mr->spell_flags, innate)) continue;

		ef_temp = breath_bite_ef(innate, mon);

		if (ef_temp) {
			++num_breaths;
			ef_temp->next = result;
			result = ef_temp;
		}
	}

	if (num_breaths > 1) {
		ef_temp = mem_zalloc(sizeof *ef_temp);
		ef_temp->index = EF_RANDOM;
		random_value rv = { 0, 0, 0, 0 };

		rv.dice = num_breaths;
		rv.sides = 1;

		effect_add_value(ef_temp, rv);

		ef_temp->next = result;
		result = ef_temp;
	}

	if (emb->extra) {
		ef_temp = emb->extra;
		while (ef_temp->next) ef_temp = ef_temp->next;

		ef_temp->next = result;
	}
	else {
		emb->extra = result;
	}
}

emb_atk_mod_fn mod_fns[] = {
	emb_atk_mod_death_touch,
	emb_atk_mod_breath_bite
};


static void calc_emb_crit(const struct monster *mon, struct embryo_attack *emb)
{
	int chance = 5;
	const struct object *obj = emb->obj;

	if (obj) {
		chance += z_info->m_crit_chance_weight_scl * obj->weight / 100;
	}

	chance += z_info->m_crit_chance_toh_skill_scl * mon->state.skills[emb->skill] / 100;

	chance += get_power_scale(mon, PP_CRITICAL_HITS, 15);

	emb->crit_chance = chance;
}

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

	blows = skill * base /* * emb->num */ / div;

	emb->blows = MAX(blows / 2 + 100, blows);

	//emb->blows = MAX(blows + 50 * emb->num, blows / 2 + 100 * emb->num);
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

	emb->sides += get_power_scale(mon, PP_UNARMED_STRIKE, 10) * factor / 100;
	
	if (emb->acc_stat == PP_UNARMED_STRIKE && mon_power_minimum(mon, PP_UNARMED_STRIKE, dexmin)) {
		emb->acc_stat = STAT_DEX;
	}
}



static struct embryo_attack *get_weapon_attack(const struct monster *mon, const struct object *weap)
{
	assert(weap);
	assert(weap->kind);
	assert(weap->kind->base);

	if (weap->tval == TV_SHIELD) return NULL;

	bool p = mon->player ? true : false;

	struct embryo_attack *emb = mem_zalloc(sizeof *emb);

	uint16_t od_mode;

	emb->obj = weap;

	emb->skill = SKILL_TO_HIT_MELEE;

	emb->acc_stat = STAT_NONE;
	emb->dam_stat = STAT_STR;

	emb->dice = weap->dd;
	emb->sides = weap->ds;

	emb->to_h = object_to_hit(weap) * BTH_PLUS_ADJ;
	emb->sides += object_to_dam(weap);

	emb->to_d = 0;

	emb->msg = p ? "hit {target}" : "hits {target}";
	od_mode = ODESC_SINGULAR | ODESC_TERSE | ODESC_LOWERCASE;
	object_desc(emb->title, sizeof emb->title, weap, od_mode, player);

	emb->extra = NULL;

	emb->num = 1;

	emb->dam_type = weap->kind->base->proj_type;

	emb->range = weap->kind->base->tval == TV_POLEARM ? 2 : 1;

	return emb;
}

static struct effect *get_timed_effect(int lev, int timed)
{
	struct effect *new = NULL;
	random_value rv = { 0, 0, 0, 0 };

	if (timed >= TMD_MAX || timed < 0) return new;

	new = mem_zalloc(sizeof *new);

	new->index = EF_OTHER_TIMED_INC;
	new->subtype = timed;

	rv.base = 10;
	rv.dice = 1;
	rv.sides = lev;

	effect_add_value(new, rv);

	return new;
}

static struct embryo_attack *get_natural_attack(const struct monster *mon, const struct monster_blow *blow)
{
	struct embryo_attack *emb = mem_zalloc(sizeof *emb);
	bool p = mon_is_player(mon);

	emb->mon_blow = blow;

	emb->skill = blow->method->skill;

	emb->acc_stat = STAT_NONE;
	emb->dam_stat = emb->skill == SKILL_SEARCH ? STAT_WIS : STAT_STR;

	emb->dice = blow->dice.dice;
	emb->sides = blow->dice.sides;
	emb->to_d = blow->dice.base;

	// make sure there's at least one die so that players can do damage via stat bonus to sides
	if (emb->dice * emb->sides == 0) {
		emb->dice = MAX(emb->dice, 1);
		emb->sides = 0;
	}

	emb->msg = p ? blow->method->fmessage : blow->method->messages->act_msg;
	strnfmt(emb->title, sizeof emb->title, "%s", blow->method->name);

	emb->extra = NULL;

	emb->num = 1;

	if (blow->effect->lash_type == -1) {
		emb->dam_type = blow->method->lash_type;
	}
	else {
		emb->dam_type = blow->effect->lash_type;
	}

	emb->range = monster_melee_attack_range(mon->race->level, blow);

	if (blow->effect->mtimed >= 0) {
		struct effect *ef = get_timed_effect(mon->race->level, blow->effect->mtimed);
		emb->extra = ef;
	}

	return emb;
}

static void get_chain_attack(const struct monster *mon, struct embryo_attack *emb)
{
	emb->obj = NULL;

	emb->skill = SKILL_TO_HIT_MELEE;

	emb->acc_stat = STAT_NONE;
	emb->dam_stat = STAT_DEX;

	emb->dice = 1;
	emb->sides = get_power_scale(mon, PP_ANIMATE_CHAINS, 5) + 5;

	emb->msg = mon_is_player(mon) ? "enchain {target}" : "enchains {target}";
	strnfmt(emb->title, sizeof emb->title, "enchain");

	emb->num = get_power_scale(mon, PP_ANIMATE_CHAINS, 5);
	emb->dam_type = PROJ_PIERCING;

	emb->range = get_power_scale(mon, PP_ANIMATE_CHAINS, 3) + 1;

	emb->auto_freq = get_power_scale(mon, PP_ANIMATE_CHAINS, 30) + 20;
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
	strnfmt(emb->title, sizeof emb->title, "%s", data->title);

	emb->extra = NULL;

	emb->num = 1;

	emb->range = 1;

	emb->dam_type = PROJ_BLUDGEONING;

	if (special == ATK_SPCL_TYP_CHAIN) get_chain_attack(mon, emb);

	return emb;
}



static void calc_emb_expertise(const struct monster *mon, struct embryo_attack *emb)
{
	int spec = attack_specialization_power(mon, emb->obj, emb->mon_blow);

	if (spec <= 0) return;

	emb->blows += spec;
	emb->to_d += spec / 15;
}



static void num_slots(const struct monster *mon, int empty[EQUIP_MAX], int total[EQUIP_MAX])
{
	int i;

	for (i = 0; i < mon->body.count; ++i) {
		struct equip_slot *slot = &mon->body.slots[i];

		if (slot != EQUIP_NONE) {
			++total[slot->type];
			if (!slot->obj) {
				++empty[slot->type];
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
			array[eq_slot] += mb->num;
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

	num = MIN(num, remaining_slots[slot]);

	remaining_slots[slot] -= num;

	if (num > 0) {
		new = get_special_attack(mon, source);
		if (new->num == 0) new->num = num;
	}

	return new;
}

static struct embryo_attack *get_unarmed(const struct monster *mon, int remaining_slots[EQUIP_MAX], bool has_attacks)
{
	int num_punches = get_power_scale(mon, PP_UNARMED_STRIKE, 3);
	int num_kicks = get_power_scale(mon, PP_UNARMED_STRIKE, 1);
	int num_touches = mon_has_power(mon, PP_DEATH_TOUCH) && num_punches <= 0 ? 1 : 0;
	int num_chains = get_power_scale(mon, PP_ANIMATE_CHAINS, 5) > 0 ? 1 : 0;
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
	new = add_unarmed(mon, ATK_SPCL_TYP_CHAIN, EQUIP_BODY_ARMOR, num_chains, remaining_slots);
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
		assert(mon->body.slots);
		const struct object *obj = mon->body.slots[i].obj;
		int slot = mon->body.slots[i].type;

		if (slot == EQUIP_WEAPON && obj) {
			new = get_weapon_attack(mon, obj);

			add_attack_to_end(&result, new);
		}
	}

	assert(blows);

	for (i = 0; i < z_info->mon_blows_max && blows[i].method; ++i) {
		const struct monster_blow *blow = &blows[i];
		int slot = blow->method->equip_slot;
		int num = blow->num;

		if (slot != EQUIP_NONE) {
			if (remaining_slots[slot] <= 0) {
				continue;
			}
			num = MIN(num, remaining_slots[slot]);
			remaining_slots[slot] -= num;
		}

		new = get_natural_attack(mon, blow);

		new->num = num;

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
	size_t siz;
	bool has_skill = emb->skill >= 0 && emb->skill < SKILL_MAX;

	if (emb->acc_stat >= 0 && emb->acc_stat < STAT_MAX) {
		int ind = mon->state.stat_ind[emb->acc_stat];
		emb->to_h += adj_dex_th(ind);
	}
	if (emb->dam_stat >= 0 && emb->dam_stat < STAT_MAX) {
		int ind = mon->state.stat_ind[emb->dam_stat];
		emb->sides += adj_str_td(ind);
	}

	main = mem_zalloc(sizeof *main);

	rv.base = emb->to_d + mon->state.to_d;
	rv.dice = emb->dice;
	rv.sides = emb->sides;
	rv.m_bonus = 0;

	if (has_skill) {
		rv.dice += get_skill_scale(mon, emb->skill, rv.dice * 3) / 2;
	}

	main->index = EF_HIT;
	main->subtype = emb->dam_type;
	assert(main->subtype >= 0 && main->subtype < PROJ_MAX);
	main->next = emb->extra;

	effect_add_value(main, rv);

	result = mem_zalloc(sizeof *result);

	result->ef = main;
	result->blows = emb->blows;
	result->obj = emb->obj;
	result->range = emb->range;
	result->to_hit = emb->to_h;
	result->num = emb->num;
	result->auto_freq = emb->auto_freq;
	result->crit_chance = emb->crit_chance;

	result->to_hit += mon->state.to_h;

	if (emb->skill >= 0 && emb->skill < SKILL_MAX) {
		result->to_hit += mon->state.skills[emb->skill];
	}

	siz = strlen(emb->msg) + 1U;
	result->message = mem_zalloc(siz);
	strnfmt(result->message, siz, "%s", emb->msg);
	my_struncap_full(result->message);

	siz = strlen(emb->title) + 1U;
	result->title = mem_zalloc(siz);
	strnfmt(result->title, siz, "%s", emb->title);
	my_struncap_full(result->title);

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

		calc_emb_crit(mon, curr);

		for (i = N_ELEMENTS(mod_fns) - 1; i >= 0; --i) {
			mod_fns[i](mon, curr);
		}

		calc_emb_expertise(mon, curr);

		hatch_attack_embryo(curr, mon);

		free_atk_embryo(curr);

		curr = next;
	}
}




static void free_attack(struct attack *atk)
{
	free_effect(atk->ef);
	string_free(atk->message);
	string_free(atk->title);
	mem_free(atk);
}

void free_mon_attacks(struct monster *mon)
{
	struct attack *atk, *nxt;

	assert(mon);
	atk = mon->atk;
	while (atk) {
		nxt = atk->next;
		free_attack(atk);
		atk = nxt;
	}

	mon->atk = NULL;
}


static void refresh_mon_attacks(struct monster *mon)
{
	free_mon_attacks(mon);
	get_mon_attacks(mon);
}


void update_mon_attacks(struct monster *mon)
{
	verify_mon_ownership(mon);
	if (mflag_has(mon->mflag, MFLAG_UPDATE_ATTACKS)) {
		refresh_mon_attacks(mon);
		mflag_off(mon->mflag, MFLAG_UPDATE_ATTACKS);
	}
	verify_mon_ownership(mon);
}


void update_mon_state(struct monster *mon)
{
	assert(mon);
	if (mflag_has(mon->mflag, MFLAG_UPDATE_STATE)) {
		assert(mon->race);
		calc_mon_bonuses(mon, &mon->state);
		mflag_off(mon->mflag, MFLAG_UPDATE_STATE);
	}
}

