/**
 * Calculations for monster state
 */

#include "angband.h"
#include "mon-calcs.h"
#include "object.h"
#include "obj-util.h"
#include "monster.h"
#include "player-calcs.h"
#include "player-util.h"



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
	int base_ac = mon->race->ac / 3, base_to = mon->race->ac - base_ac;
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


	for (i = 0; i < PP_MAX; ++i) {
		state->powers[i] = mon_power(mrace, i);
	}
	for (i = 0; i < SKILL_MAX; i++) {
		if (i == SKILL_HEALTH) state->skills[i] = mrace->avg_hp;
		else state->skills[i] = mrace->skills[i] * (mrace->level + 66) / 66;
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
}

void update_mon_state(struct monster *mon)
{
	if (mflag_has(mon->mflag, MFLAG_UPDATE)) {
		calc_mon_bonuses(mon, &mon->state);
		mflag_off(mon->mflag, MFLAG_UPDATE);
	}
}


