/**
 * \file player.c
 * \brief Player implementation
 *
 * Copyright (c) 2011 elly+angband@leptoquark.net. See COPYING.
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

#include "effects.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-make.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-enum.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "randname.h"
#include "z-color.h"
#include "z-util.h"

/**
 * Pointer to the player struct
 */
struct player *player = NULL;

struct player_body *bodies;
struct player_race *races;
struct player_shape *shapes;
struct player_class *classes;
struct player_ability *player_abilities;
struct magic_realm *realms;
struct player_spell *spells;

/**
 * Base experience levels, may be adjusted up for race and/or class
 */
/*static uint64_t new_player_exp(int level, int xp_fact)
{
	int base = 150 - xp_fact / 2; // ten times
	int exp_scale, linear_scale, cost, i = 1;

	while (i - base < level * 10) {
		i += base;
		++exp_scale;
		base += 10;
	}

	linear_scale = (level * 10 - exp_scale) * base / 10;

	cost = exponentiate(10, exp_scale, 1);
	cost *= linear_scale;

	return cost;*/

	/*double base = 21.0 - my_sqrt((float)xp_fact);
	int quotient, remainder;
	uint64_t cost;

	base = MAX(base, 5.0);
	quotient = (int)(level / base);
	remainder = (int)(level - quotient * base);

	cost = (uint64_t)exponentiate(10, quotient, 1);
	cost *= (uint64_t)(remainder * (base - 1));

	return cost;*/

	/*int base = 20 - my_int_sqrt(xp_fact), i, mult;
	uint64_t cost;
	base = MAX(base, 5);
	cost = 1U;
	mult = level % base;

    for (i = 0; i < level / base; i++) cost *= 10U;

	cost *= MAX(mult * base, mult * base / 2 + base);

	return cost;*/

	/*int exp_num = level;
	int exp_denom = 2;
	double base = (double)xp_fact / 200.0 + 1.0;

	plog_fmt("exponentiating (%f)^(%i)", base, exp_num);

	return (int)exponentiate_dbl(base, exp_num, exp_denom);
}*/

const uint32_t player_exp[PY_MAX_LEVEL] =
{
	2,
	6,
	12,
	25,
	50,
	100,
	200,
	280,
	380,
	500,
	650,
	850,
	1100,
	1400,
	1800,
	2300,
	2900,
	3600,
	4400,
	5400,
	6800,
	8400,
	10200,
	12500,
	17500,
	25000,
	35000L,
	50000L,
	75000L,
	100000L,
	150000L,
	200000L,
	275000L,
	350000L,
	450000L,
	550000L,
	700000L,
	850000L,
	1000000L,
	1250000L,
	1500000L,
	1800000L,
	2100000L,
	2400000L,
	2700000L,
	3000000L,
	3500000L,
	4000000L,
	4500000L,
	5000000L,
	6400000L,
	8100000L,
	10000000L,
	12100000L,
	14400000L,
	16900000L,
	19600000L,
	22500000L,
	25600000L,
	28900000L,
	32400000L,
	36100000L,
	40000000L,
	44100000L,
	48400000L
};


static uint64_t player_exp_new_calc(int level_num, int level_denom)
{
	int num, div;
	int lev_remain = level_num / level_denom, ten_exp = 0, ten_exp_need;
	uint64_t result = 1, mod;
	double temp_result;
	bool two;

	for (ten_exp_need = 3; ; ten_exp_need++) {
		if (ten_exp_need > lev_remain) {
			div = ten_exp_need;
			num = ten_exp * div + lev_remain;

			div *= level_denom;
			num *= level_denom;

			num += level_num % level_denom;

			temp_result = exponentiate_dbl(10.0, num, div);

			if (temp_result > (double)UINT64_MAX) {
				plog_fmt("level %f too high to calculate", (float)level_num / (float)level_denom);
			}

			result = (uint64_t)(temp_result + 0.3);
			break;
		}
		
		ten_exp += 1;
		lev_remain -= ten_exp_need;
	}

	mod = 1U;
	two = false;
	while (mod * 50U < result) {
		if (two) {
			mod *= 2U;
		}
		else {
			mod *= 5U;
		}
		two = !two;
	}

	result = (result / mod) * mod;

	return result;
}


uint64_t player_exp_new(int level_num, int level_denom)
{
	static uint64_t whole_results[PY_MAX_LEVEL] = { 0 };
	static uint64_t prev_result = 0;
	static int prev_num = 0, prev_denom = 0;
	uint64_t result;

	if (level_denom == 1 && level_num <= PY_MAX_LEVEL && whole_results[level_num - 1] != 0) {
		return whole_results[level_num - 1];
	}

	if (prev_num == level_num && prev_denom == level_denom) {
		return prev_result;
	}

	result = player_exp_new_calc(level_num, level_denom);

	if (level_denom == 1 && level_num <= PY_MAX_LEVEL) {
		whole_results[level_num - 1] = result;
	}
	else {
		prev_num = level_num;
		prev_denom = level_denom;
		prev_result = result;
	}

	return result;
}


static const char *stat_name_list[] = {
	#define STAT(a) #a,
	#include "list-stats.h"
	#undef STAT
	"MAX",
    NULL
};

int stat_name_to_idx(const char *name)
{
    int i;
    for (i = 0; stat_name_list[i]; i++) {
        if (!my_stricmp(name, stat_name_list[i]))
            return i;
    }

    return -1;
}

const char *stat_idx_to_name(int type)
{
    assert(type >= 0);
    assert(type < STAT_MAX);

    return stat_name_list[type];
}

const struct magic_realm *lookup_realm(const char *name)
{
	struct magic_realm *realm = realms;
	while (realm) {
		if (!my_stricmp(name, realm->name)) {
			return realm;
		}
		realm = realm->next;
	}

	/* Fail horribly */
	quit_fmt("Failed to find %s magic realm", name);
	return realm;
}

bool player_stat_inc(struct player *p, int stat)
{
	int v = p->stat_cur[stat];

	if (v >= 18 + 100)
		return false;
	if (v < 18) {
		p->stat_cur[stat]++;
	} else if (v < 18 + 90) {
		int gain = (((18 + 100) - v) / 2 + 3) / 2;
		if (gain < 1)
			gain = 1;
		p->stat_cur[stat] += randint1(gain) + gain / 2;
		if (p->stat_cur[stat] > 18 + 99)
			p->stat_cur[stat] = 18 + 99;
	} else {
		p->stat_cur[stat] = 18 + 100;
	}

	/* L: impose maxima based on birth */
	p->stat_cur[stat] = MIN(p->stat_cur[stat], stat_max_max(p, stat));

	if (p->stat_cur[stat] > p->stat_max[stat]) {
		p->stat_max[stat] = p->stat_cur[stat];
	}
	
	p->upkeep->update |= PU_BONUS;
	return true;
}

bool player_stat_dec(struct player *p, int stat, bool permanent)
{
	int cur, max, res = false;

	cur = p->stat_cur[stat];
	max = p->stat_max[stat];

	if (cur > 18+10)
		cur -= 10;
	else if (cur > 18)
		cur = 18;
	else if (cur > 3)
		cur -= 1;

	res = (cur != p->stat_cur[stat]);

	if (permanent) {
		if (max > 18+10)
			max -= 10;
		else if (max > 18)
			max = 18;
		else if (max > 3)
			max -= 1;

		res = (max != p->stat_max[stat]);
	}

	if (res) {
		p->stat_cur[stat] = cur;
		p->stat_max[stat] = max;
		p->upkeep->update |= (PU_BONUS);
		p->upkeep->redraw |= (PR_STATS);
	}

	return res;
}

uint64_t player_exp_needed(struct player *p, int level)
{
	int num = level * p->mon.state.expfact;
	int denom = 100;

	return player_exp_new(num, denom);
}

bool player_at_max_level(struct player *p)
{
	if (p->lev >= PY_MAX_LEVEL) return true;
	
    if (p->lev >= (50 + adj_int_lev(p->mon.state.stat_ind[STAT_INT]))) return true;

	//if (player_exp[p->lev-1] > PY_MAX_EXP) return true;

	return false;
}

bool player_can_level_up(struct player *p)
{
	if (player_at_max_level(p)) return false;

	if (player_exp_needed(p, p->lev) > p->exp) {
		return false;
	}

    //if (p->exp < (player_exp[p->lev-1])) return false;

	return true;
}

static void adjust_level(struct player *p, bool verbose, bool levelup)
{
	bool doneone = false;

	if (p->exp > PY_MAX_EXP) {
		p->exp = PY_MAX_EXP;
	}

	if (p->max_exp > PY_MAX_EXP) {
		p->max_exp = PY_MAX_EXP;
	}

	if (p->exp > p->max_exp) {
		p->max_exp = p->exp;
	}

	p->upkeep->redraw |= PR_EXP;

	if (levelup) handle_stuff(p);

	/*while ((p->lev > 1) &&
		   (p->exp < player_exp[p->lev-2])) {
		p->lev--;
	}*/

	while (((levelup && !doneone) || p->lev < p->max_lev) && player_can_level_up(p)) {
		char buf[80];

		p->lev++;

		/* Save the highest level */
		/* L: and do stuff that happens on the first time reaching a level */
		while (p->lev > p->max_lev) {
			doneone = true;

			p->max_lev++;

			p->exp_frac = 0;

			int freq = p->max_lev < 20 ? 5 : p->max_lev < 36 ? 4 : 3;

			if (!(p->max_lev % freq)) {
				player_increase_stat(p);
			}
		}

		if (verbose) {
			/* Log level updates */
			strnfmt(buf, sizeof(buf), "Reached level %d", p->lev);
			history_add(p, buf, HIST_GAIN_LEVEL);

			/* Message */
			msgt(MSG_LEVEL, "Welcome to level %d.",	p->lev);
		}
	}

	p->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);
	p->upkeep->redraw |= (PR_LEV | PR_TITLE | PR_EXP | PR_STATS);
	
	if (levelup) handle_stuff(p);
}

void player_exp_gain(struct player *p, uint64_t amount, uint32_t fract)
{
	uint64_t tolev;
	uint64_t new_fract, extra_fract, new_amt;

	if (p->max_lev >= PY_MAX_LEVEL) tolev = PY_MAX_EXP;
	else tolev = player_exp_needed(p, p->max_lev);// player_exp[p->max_lev - 1];

	new_amt = amount * 100;
	new_amt /= p->mon.state.expfact;

	new_fract = fract * 100;
	new_fract /= p->mon.state.expfact;

	extra_fract = (amount * 100 - new_amt * p->mon.state.expfact) * UINT16_MAX;
	extra_fract /= p->mon.state.expfact;

	new_fract += extra_fract;
	new_fract += p->exp_frac;

	while (new_fract > UINT16_MAX) {
		++new_amt;
		new_fract -= UINT16_MAX;
	}

	if (new_amt > tolev - p->exp) {
		p->exp = tolev;
		p->exp_frac = new_fract;
	}
	else {
		p->exp += new_amt;
		p->exp_frac = new_fract;
	}

	p->xp_this_turn += new_amt;

	if (p->lev >= 10 && p->num_evol_choices > 0) {
		if (p->monster_xp < UINT64_MAX - new_amt) {
			p->monster_xp += new_amt;
		} else {
			p->monster_xp = UINT64_MAX;
		}
	}

	if (p->mon.m_timed[TMD_PHOENIX_CD]) {
		int dec = (new_amt + randint0(p->lev)) / p->lev;
		player_dec_timed(player, TMD_PHOENIX_CD, dec, true, true);
	}
	
	if (p->exp < p->max_exp) {
		p->max_exp = MIN(new_amt / 10 + p->max_exp, tolev);
	}

	adjust_level(p, true, false);
}

int player_min_xp_depth(struct player *p)
{
	int64_t eff_xp = p->max_exp;// * p->mon.state.expfact / 100;
	int i;

	for (i = 1; i < PY_MAX_LEVEL; ++i) {
		if (player_exp_new(i, 1) > eff_xp) break;
	}

	return i * 3 / 2;
}

void player_exp_lose(struct player *p, int64_t amount, bool permanent)
{
	if (p->exp < (unsigned)amount) {
		amount = p->exp;
	}
	p->exp -= amount;
	if (permanent) {
		p->max_exp -= amount;
	}
	adjust_level(p, true, false);
}

void player_level_up_one(struct player *p, bool verbose)
{
	adjust_level(p, verbose, true);
}

void check_level(struct player *p)
{
	adjust_level(p, false, false);
}

/**
 * Obtain object flags for the player
 */
void player_flags(struct player *p, bitflag f[OF_SIZE])
{
	/* Add racial flags */
	memcpy(f, p->race->flags, sizeof(p->race->flags));
	of_union(f, p->class->flags);

	/* Some classes become immune to fear at a certain plevel */
	if (player_has(p, PF_BRAVERY_30) && p->lev >= 30) {
		of_on(f, OF_PROT_FEAR);
	}
}


/**
 * Combine any flags due to timed effects on the player into those in f.
 *
 * Hack:  TMD_TRAPSAFE is excluded so a player's flags can be tested for
 * OF_TRAP_IMMUNE and know that did not come from a timed effect; that is
 * used for learning the trap immune rune when working with traps
 */
void player_flags_timed(struct player *p, bitflag f[OF_SIZE])
{
	int i;

	for (i = 0; i < TMD_MAX; ++i) {
		if (p->mon.m_timed[i] && timed_effects[i].oflag_dup != OF_NONE
				&& i != TMD_TRAPSAFE) {
			of_on(f, timed_effects[i].oflag_dup);
		}
	}
}


uint8_t player_hp_attr(struct player *p)
{
	uint8_t attr;
	
	if (p->mon.hp >= p->mon.maxhp)
		attr = COLOUR_L_GREEN;
	else if (p->mon.hp > (p->mon.maxhp * p->opts.hitpoint_warn) / 10)
		attr = COLOUR_YELLOW;
	else
		attr = COLOUR_RED;
	
	return attr;
}

uint8_t player_sp_attr(struct player *p)
{
	uint8_t attr;
	
	if (p->csp >= p->msp)
		attr = COLOUR_L_GREEN;
	else if (p->csp > (p->msp * p->opts.hitpoint_warn) / 10)
		attr = COLOUR_YELLOW;
	else
		attr = COLOUR_RED;
	
	return attr;
}

bool player_restore_mana(struct player *p, int amt) {
	int old_csp = p->csp;

	p->csp += amt;
	if (p->csp > p->msp) {
		p->csp = p->msp;
	}
	p->upkeep->redraw |= PR_MANA;

	msg("You feel some of your energies returning.");

	return p->csp != old_csp;
}

/**
 * Construct a random player name appropriate for the setting.
 *
 * \param buf is the buffer to contain the name.  Must have space for at
 * least buflen characters.
 * \param buflen is the maximum number of character that can be written to
 * buf.
 * \return the number of characters, excluding the terminating null, written
 * to the buffer
 */
size_t player_random_name(char *buf, size_t buflen)
{
	size_t result = randname_make(RANDNAME_TOLKIEN, 4, 8, buf, buflen,
		name_sections);

	my_strcap(buf);
	return result;
}

/**
 * Return a version of the player's name safe for use in filesystems.
 *
 * XXX This does not belong here.
 */
void player_safe_name(char *safe, size_t safelen, const char *name, bool strip_suffix)
{
	size_t i;
	size_t limit = 0;

	if (name) {
		char *suffix = find_roman_suffix_start(name);

		if (suffix) {
			limit = suffix - name - 1; /* -1 for preceding space */
		} else {
			limit = strlen(name);
		}
	}

	/* Limit to maximum size of safename buffer */
	limit = MIN(limit, safelen);

	for (i = 0; i < limit; i++) {
		char c = name[i];

		/* Convert all non-alphanumeric symbols */
		if (!isalpha((unsigned char)c) && !isdigit((unsigned char)c))
			c = '_';

		/* Build "base_name" */
		safe[i] = c;
	}

	/* Terminate */
	safe[i] = '\0';

	/* Require a "base" name */
	if (!safe[0]) {
		my_strcpy(safe, "PLAYER", safelen);
	}
}


static void cleanup_follower(struct follower *follow)
{
	struct object *obj;
	int i;
	struct monster *mon = follow->mon;

	for (obj = mon->gear; obj; obj = obj->next) {
		object_free(obj);
	}
	for (i = 0; i < mon->body.count; ++i) {
		obj = mon->body.slots[i].obj;
		if (obj) object_free(obj);
	}

	mon_disembody(mon);
	free_mon_attacks(mon);

	mem_free(mon);
	mem_free(follow);
}


/**
 * Release resources allocated for fields in the player structure.
 */
void player_cleanup_members(struct player *p)
{
	struct follower *curr, *next;

	/* Free the history */
	history_clear(p);

	/* Free the things that are always initialised */
	if (p->obj_k) {
		object_free(p->obj_k);
	}
	if (p->upkeep) {

		curr = p->upkeep->follow;
		while (curr) {
			next = curr->next;
			cleanup_follower(curr);
			curr = next;
		}

		mem_free(p->upkeep->quiver);
		mem_free(p->upkeep->inven);
		mem_free(p->upkeep->steps);
		mem_free(p->upkeep);
		p->upkeep = NULL;
	}

	/* Free the things that are only sometimes initialised */
	if (p->quests) {
		player_quests_free(p);
	}
	player_spells_free(p);
	if (p->mon.gear) {
		object_pile_free(NULL, NULL, p->mon.gear);
		object_pile_free(NULL, NULL, p->gear_k);
	}
	if (p->mon.body.slots) {
		for (int i = 0; i < p->mon.body.count; i++) {
			string_free(p->mon.body.slots[i].name);
		}
		mem_free(p->mon.body.slots);
		p->mon.body.slots = NULL;
	}
	string_free(p->mon.body.name);
	string_free(p->history);
	if (p->cave) {
		cave_free(p->cave);
		p->cave = NULL;
	}
	if (p->unlocked_classes) {
		mem_free(p->unlocked_classes);
		p->unlocked_classes = NULL;
	}
	if (p->unlocked_races) {
		mem_free(p->unlocked_races);
		p->unlocked_races = NULL;
	}
	if (p->unlocked_tomes) {
		mem_free(p->unlocked_tomes);
		p->unlocked_tomes = NULL;
	}
	if (p->extra_target) {
		mem_free(p->extra_target);
		p->extra_target = NULL;
	}
	if (p->extra_choice) {
		mem_free(p->extra_choice);
		p->extra_choice = NULL;
	}
	if (p->extra_learned) {
		mem_free(p->extra_learned);
		p->extra_learned = NULL;
	}
	if (p->learned_when) {
		mem_free(p->learned_when);
		p->learned_when = NULL;
	}
	if (p->evol_choices) {
		mem_free(p->evol_choices);
		p->evol_choices = NULL;
	}
	if (p->mon.atk) {
		free_mon_attacks(&p->mon);
	}
	mem_free(p->mon.race);
	if (p->mon.original_race) {
		mem_free(p->mon.original_race);
	}
}


/**
 * Initialise player struct
 */
static void init_player(void) {
	int i;
	/* Create the player array, initialised with 0 */
	player = mem_zalloc(sizeof *player);

	/* Allocate player sub-structs */
	player->upkeep = mem_zalloc(sizeof(struct player_upkeep));
	player->upkeep->inven = mem_zalloc((z_info->pack_size + 1) * sizeof(struct object *));
	player->upkeep->quiver = mem_zalloc(z_info->quiver_size * sizeof(struct object *));
	player->obj_k = object_new();
	player->obj_k->brands = mem_zalloc(z_info->brand_max * sizeof(bool));
	player->obj_k->slays = mem_zalloc(z_info->slay_max * sizeof(bool));
	player->obj_k->curses = mem_zalloc(z_info->curse_max *
									   sizeof(struct curse_data));

	player->unlocked_classes = mem_zalloc(z_info->c_max * sizeof(*player->unlocked_classes));
	player->unlocked_races = mem_zalloc(z_info->pr_max * sizeof(*player->unlocked_races));
	player->unlocked_tomes = mem_zalloc(z_info->learn_max * sizeof(*player->unlocked_tomes));

	player->extra_target = mem_zalloc(z_info->learn_max * sizeof *player->extra_target);
	player->extra_choice = mem_zalloc(z_info->learn_max * sizeof *player->extra_choice);
	player->extra_learned = mem_zalloc(z_info->learn_max * sizeof *player->extra_learned);
	player->learned_when = mem_zalloc(z_info->learn_max * sizeof *player->learned_when);

	assert(player->extra_learned);

	for (i = 0; i < z_info->learn_max; ++i) {
		player->extra_choice[i] = -1;
	}
	
	player->mon.midx = PLAYER_MON_MIDX;
	player->mon.player = player;

	options_init_defaults(&player->opts);

	mflag_on(player->mon.mflag, MFLAG_UPDATE_STATE);
	mflag_on(player->mon.mflag, MFLAG_UPDATE_ATTACKS);
}

/**
 * Free player struct
 */
static void cleanup_player(void) {
	if (!player) return;

	player_cleanup_members(player);

	/* Free the basic player struct */
	mem_free(player);
	player = NULL;
}

struct init_module player_module = {
	.name = "player",
	.init = init_player,
	.cleanup = cleanup_player
};
