/**
 * \file ui-player-properties.c 
 * \brief UI for class and race abilities
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
#include "game-input.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-util.h"
#include "player-calcs.h"
#include "player-properties.h"
#include "player-spell.h"
#include "player-util.h"
#include "ui-input.h"
#include "ui-knowledge.h"
#include "ui-menu.h"
#include "ui-player-properties.h"
#include "ui-target.h"



static void add_scaling_desc(char *buf, const char *name, int base, int scale, int lev, int numleft, size_t bufsize)
{
	if (!base && !scale) return;
	if (base) {
		my_strcat(buf, format("%i", base), bufsize);
	}
	if (base && scale) {
		my_strcat(buf, " + ", bufsize);
	}
	if (scale) {
		my_strcat(buf, format("%i%% of your level (%i)", scale * 2, lev * scale / 50), bufsize);
	}
	my_strcat(buf, " from your ", bufsize);
	my_strcat(buf, name, bufsize);
	if (numleft > 2) {
		my_strcat(buf, ", ", bufsize);
	}
	else if (numleft == 2) {
		my_strcat(buf, ", and ", bufsize);
	}
	else {
		my_strcat(buf, ". ", bufsize);
	}
}

static void ability_desc(struct player *p, const struct player_ability *ability, char *buf, size_t bufsize, bool player_has, int group)
{
	int monster_powers[PP_MAX] = { 0 };
	int monster_skills[SKILL_MAX] = { 0 };
	//int race_skills[SKILL_MAX] = { 0 };
	//int race_x_skills[SKILL_MAX] = { 0 };
	struct monster_race *mrace = lookup_player_monster(p);

	// L: hack for hypothetical players
	bool hypothetical = p != player;
	const char *verb = hypothetical ? "would gain" : "gain";

	assert(bufsize > 0);

	//player_race_r_skill(p->race, mrace ? true : false, race_skills);
	//player_race_x_skill(p->race, mrace ? true : false, race_x_skills);

	memset(buf, '\0', bufsize * sizeof *buf);

	if (group == PLAYER_FLAG_POWER || group == PLAYER_FLAG_SKILL) {
		if (player_has) {
			my_strcat(buf, "You", bufsize);
		} else {
			my_strcat(buf, "User", bufsize);
		}
	}
	my_strcat(buf, ability->desc, bufsize);
	my_strcat(buf, "\n", bufsize);

	if (mrace) {
		calc_monster_powers(mrace, monster_powers, player->mon.state.powers);
		calc_monster_skills(mrace, monster_skills);
	}
	if (group == PLAYER_FLAG_POWER || group == PLAYER_FLAG_SKILL) {
		int cbase = 0, cxtra = 0, rbase = 0, rxtra = 0, tbase = 0, txtra = 0, stat = 0;
		char stat_name[80];
		if (group == PLAYER_FLAG_POWER) {
			cxtra = mon_class_power(&p->mon, ability->index);
			rxtra = mon_race_power(&p->mon, ability->index);
			tbase = mon_tome_power(&p->mon, ability->index);
		}
		else {
			int result;
			race_skill(&p->mon, ability->index, &rbase, &rxtra);
			class_skill(&p->mon, ability->index, &cbase, &cxtra);
			tome_skill(&p->mon, ability->index, &tbase, &txtra);

			result = (rxtra + cxtra + txtra) * mon_lev(&p->mon) / 50 + rbase + cbase + tbase;
			stat = stat_skill_bonus(&p->mon, &p->mon.state, ability->index, result, stat_name, sizeof stat_name);
		}

		int numleft = ((rxtra || rbase) ? 1 : 0) +
				((cxtra || cbase) ? 1 : 0) +
				((txtra || tbase) ? 1 : 0) +
				(stat ? 1 : 0);

		if (numleft > 0) {
			my_strcat(buf, format("You %s ", verb), bufsize);
			if (cbase || cxtra) {
 				add_scaling_desc(buf, "class", cbase, cxtra, p->lev, numleft, bufsize);
				--numleft;
			}
			if (rbase || rxtra) {
				add_scaling_desc(buf, "race", rbase, rxtra, p->lev, numleft, bufsize);
				--numleft;
			}
			if (tbase || txtra) {
				add_scaling_desc(buf, "learning", tbase, txtra, p->lev, numleft, bufsize);
				--numleft;
			}
			if (stat) {
				add_scaling_desc(buf, stat_name, stat, 0, p->lev, numleft, bufsize);
				--numleft;
			}
			my_strcat(buf, "\n", bufsize);
		}
	}
}

static bool ability_parent_desc(const struct player_ability *abil, char *buf, size_t bufsize)
{
	int i;
	int parent_num = 0;
	int numleft;

	memset(buf, '\0', bufsize * sizeof *buf);

	for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
		if (abil->parent[i]) {
			++parent_num;
		}
	}

	if (parent_num == 0) {
		return false;
	}

	numleft = parent_num;

	for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
		if (abil->parent[i]) {
			const char *label, *name;

			if (parent_num == 1) label = "Its parent is ";
			else if (numleft == parent_num) label = "Its parents are ";
			else if (numleft > 1) label = ", ";
			else if (parent_num == 2) label = " and ";
			else label = ", and ";

			name = abil->parent[i]->name;

			my_strcat(buf, label, bufsize);
			my_strcat(buf, name, bufsize);

			--numleft;
		}
	}

	strncat(buf, ".\n", bufsize);

	return true;
}


/**
 * ------------------------------------------------------------------------
 * Code for viewing race and class abilities
 * ------------------------------------------------------------------------ */

static char view_ability_tag(struct menu *menu, int oid)
{
	return all_letters_nohjkl[oid];
}

/**
 * Display an entry on the gain ability menu
 */
static void view_ability_display(struct menu *menu, int oid, bool cursor,
	int row, int col, int width)
{
	char buf[80];
	uint8_t color;
	const struct player_ability *choices = menu->menu_data;

	switch (choices[oid].group) {
	case PLAYER_FLAG_SPECIAL:
		{
			strnfmt(buf, sizeof(buf), "Specialty Ability: %s",
				choices[oid].name);
			color = COLOUR_GREEN;
			break;
		}
	case PLAYER_FLAG_CLASS:
		{
			strnfmt(buf, sizeof(buf), "Class:  %s",
				choices[oid].name);
			color = COLOUR_UMBER;
			break;
		}
	case PLAYER_FLAG_RACE:
		{
			strnfmt(buf, sizeof(buf), "Racial: %s",
				choices[oid].name);
			color = COLOUR_ORANGE;
			break;
		}
	case PLAYER_FLAG_POWER:
		{
			int curr = player->mon.state.powers[choices[oid].index];
			strnfmt(buf, sizeof(buf), "Power:  %s (level %i)", 
				choices[oid].name, curr);
			color = curr > 0 ? COLOUR_GREEN : COLOUR_RED;
			break;
		}
	case PLAYER_FLAG_SKILL:
		{
			strnfmt(buf, sizeof(buf), "Skill:  %s (level %i)",
				choices[oid].name, player->mon.state.skills[choices[oid].index]);
			color = COLOUR_L_BLUE;
			break;
		}
	default:
		{
			my_strcpy(buf, "Mysterious", sizeof(buf));
			color = COLOUR_PURPLE;
		}
	}

	/* Print it */
	c_put_str(cursor ? COLOUR_WHITE : color, buf, row, col);
}


/**
 * Show ability long description when browsing
 */
static void view_ability_menu_browser(int oid, void *data, const region *loc)
{
	const struct player_ability *choices = data;
	char buf[256] = "";

	/* Redirect output to the screen */
	text_out_hook = text_out_to_screen;
	text_out_wrap = 60;
	text_out_indent = loc->col - 1;
	text_out_pad = 1;

	ability_desc(player, &choices[oid], buf, sizeof(buf), true, choices[oid].group);

	clear_from(loc->row + loc->page_rows);
	Term_gotoxy(loc->col, loc->row + loc->page_rows);

	text_out_c(COLOUR_L_BLUE, "%s", buf);

	/* XXX */
	text_out_pad = 0;
	text_out_indent = 0;
	text_out_wrap = 0;
}

/**
 * Display list available specialties.
 */
void textui_view_ability_menu(struct player_ability *ability_list,
							  int num_abilities)
{
	struct menu menu;
	menu_iter menu_f = { view_ability_tag, NULL, view_ability_display, NULL, NULL, NULL };
	region loc = { 0, 0, 70, -99 };
	char buf[80];

	/* Save the screen and clear it */
	screen_save();

	/* Prompt choices */
	strnfmt(buf, sizeof(buf),
		"Abilities, powers, and skills (%c-%c, ESC=exit): ",
		all_letters_nohjkl[0], all_letters_nohjkl[num_abilities - 1]);

	/* Set up the menu */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.header = buf;
	menu_setpriv(&menu, num_abilities, ability_list);
	loc.page_rows = num_abilities + 1;
	menu.flags = MN_DBL_TAP;
	menu.browse_hook = view_ability_menu_browser;
	region_erase_bordered(&loc);
	menu_layout(&menu, &loc);

	menu_select(&menu, 0, false);

	/* Load screen */
	screen_load();

	return;
}






/**
 * L: Menu for learning powers / skills
 */
struct ability_learn_menu_mode_data {
	int index;
	const char *name;
} ability_learn_menu_modes[] = {
	{ AL_MODE_KNOWN, "Known Abilities" },
	{ AL_MODE_ALL, "All Abilities" },
};

struct ability_learn_menu_data {
	struct player *p;
	int mode;
	int *max_learnable;
	int *extra_max_learnable;
	int *valid;
	uint16_t *temp_target;
	//uint16_t *max_result;
	bool birth;
	int points;
	const struct monster_race *end_monster;
};

enum ability_learn_menu_data_column_locations {
	ALMC_NAME = 0,
	ALMC_COST = ALMC_NAME + 35,
	ALMC_COST_DIFF = ALMC_COST + 6,
	ALMC_CURR = ALMC_COST_DIFF + 6,
	ALMC_LRND = ALMC_CURR + 6,
	ALMC_TRGT = ALMC_LRND + 6,
	ALMC_COST_RESULT = ALMC_COST_DIFF + 5,
	ALMC_MAX = ALMC_TRGT + 5,
};


static const struct player_ability *ability_by_tome_id(int tome_id)
{
	const struct player_ability *pa;

	for (pa = player_abilities; pa; pa = pa->next) {
		if (pa->learn_index == tome_id) {
			return pa;
		}
	}

	return NULL;
}


static void get_max_learnable(struct menu *m, struct player *p)
{
	struct ability_learn_menu_data *data = menu_priv(m);

	tome_max_learnable_extra(data->p, data->max_learnable, data->extra_max_learnable);
}


static const struct player_ability *abil_parent(const struct player_ability *abil, struct player *p)
{
	const struct player_ability *bestparent = NULL, *currparent;
	int bestknown = -1, currknown, i;

	for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
		currparent = abil->parent[i];
		if (currparent) {
			if (currparent->type == PY_ABIL_POWER) currknown = p->mon.state.powers[currparent->index];
			else if (currparent->type == PY_ABIL_SKILL) currknown = p->mon.state.skills[currparent->index];
			else continue;

			if (currknown > bestknown || !bestparent) {
				bestparent = currparent;
				bestknown = currknown;
			}
		}
	}

	return bestparent;
}

/**
 * how many sequential parents a tome has
 */
static int tome_depth(const struct player_ability *abil)
{
	int depth = 0;
	const struct player_ability *next;
	for (next = abil_parent(abil, player); next; next = abil_parent(next, player)) {
		++depth;
		assert(next != abil);
	}
	return depth;
}

/**
 * if it's beyond what is learnable it's not valid
 * if it's less than what is learned so far it's not valid
 * however it's valid if it's beyond what is learnable but isn't more than
 * what has been leanred so far
 */
static bool extra_target_valid(struct player *p, int *max_learnable, int oid, int target)
{
	if (target > max_learnable[oid] && target > p->extra_target[oid]) {
		return false;
	}

	if (target < p->extra_target[oid]) {
		return false;
	}

	return true;
}


static void ability_learn_valid_refresh(struct menu *menu)
{
	int oid;
	bool changed = true;
	struct ability_learn_menu_data *data = menu_priv(menu);
	const struct player_ability *abil;

	for (oid = 0; oid < z_info->learn_max; ++oid) {
		data->valid[oid] = MN_ROW_SKIP;
		abil = ability_by_tome_id(oid);

		assert(abil);

		if (abil->type == PY_ABIL_SKILL) {
			data->valid[oid] = MN_ROW_VALID;
			continue;
		}
		else if (abil->type != PY_ABIL_POWER) {
			data->valid[oid] = MN_ROW_SKIP;
			continue;
		}

		if (!ability_satisfies_all_prereqs(abil, data->p)) {
			data->valid[oid] = MN_ROW_SKIP;
		}
		else if (data->birth) {
			data->valid[oid] = MN_ROW_VALID;
		}
		else if (data->max_learnable[oid] > 0) {
			data->valid[oid] = MN_ROW_VALID;
		}

		if (data->valid[oid] == MN_ROW_SKIP &&
				(data->p->extra_powers[abil->index] > 0 || data->p->mon.state.powers[abil->index] > 0)) {
			data->valid[oid] = MN_ROW_INVALID;
		}
	}

	while (changed) {
		changed = false;

		for (oid = 0; oid < z_info->learn_max; ++oid) {
			abil = ability_by_tome_id(oid);

			if (data->valid[oid] != MN_ROW_SKIP) {
				int i;
				for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
					const struct player_ability *parent = abil->parent[i];
					if (parent && data->valid[parent->learn_index] == MN_ROW_SKIP) {
						data->valid[parent->learn_index] = MN_ROW_INVALID;
						changed = true;
					}
				}
			}
		}
	}
}



static char ability_learn_get_tag(struct menu *menu, int oid)
{
	int num_tags = 0, i;
	int cursor = menu_oid_to_cursor(menu, oid);

	for (i = 0; all_letters_nohjkl[i]; ++i) {
		++num_tags;
	}

	if (cursor < menu->top) {
		return '\0';
	}
	if (cursor > menu->top + num_tags) {
		return '\0';
	}

	i = cursor % num_tags;

	return all_letters_nohjkl[i];
}

static int ability_learn_valid_mode(struct menu *menu, int oid, int mode)
{
	struct ability_learn_menu_data *data = menu_priv(menu);

	return data->valid[oid];
	const struct player_ability *abil = ability_by_tome_id(oid);
	assert(abil);

	if (oid < 0 || oid >= z_info->learn_max) return MN_ROW_SKIP;

	if (!ability_satisfies_all_prereqs(abil, data->p)) {
		return MN_ROW_SKIP;
	}

	if (abil->type == PY_ABIL_POWER) {
		if (data->p->extra_powers[abil->index] <= 0 &&
				data->p->mon.state.powers[abil->index] <= 0) {
			// if it's the full menu show all powers
			// if it's the partial menu only show learned powers
			if (mode == AL_MODE_ALL) {
				return data->max_learnable[oid] > 0 || data->birth ? MN_ROW_VALID : MN_ROW_INVALID;
			}
			return MN_ROW_SKIP;
		}
		return MN_ROW_VALID;
	}
	else {
		//if (mode != AL_MODE_SKILLS) return MN_ROW_SKIP;
		return MN_ROW_VALID;
	}

	return MN_ROW_SKIP;
}

static int ability_learn_valid(struct menu *menu, int oid)
{
	struct ability_learn_menu_data *data = menu_priv(menu);

	return ability_learn_valid_mode(menu, oid, data->mode);
}

static void ability_learn_display(struct menu *m, int oid, bool cursor,
	int row, int col, int wid)
{
	char name[32];
	int total_level, learn_level, learn_target, cost, cost_inc, next_level;
	uint8_t tl_attr, ll_attr, lt_attr, name_attr, cost_attr, cost_inc_attr;
	struct ability_learn_menu_data *data = menu_priv(m);
	const struct player_ability *abil = ability_by_tome_id(oid);
	bool targ_valid, next_targ_valid;
	int name_indent = tome_depth(abil) * 1;
	bool power = abil->type == PY_ABIL_POWER;

	if (oid < 0 || oid >= z_info->learn_max) return;

	if (power) {
		total_level = data->p->mon.state.powers[abil->index];
		learn_level = data->p->extra_powers[abil->index];
	}
	else {
		total_level = data->p->mon.state.skills[abil->index];
		learn_level = data->p->extra_skills[abil->index];
	}
	strcpy(name, abil->name);

	learn_target = data->temp_target[oid];

	next_level = tome_next_increment(data->p, abil, learn_target);
	targ_valid = extra_target_valid(data->p, data->max_learnable, oid, learn_target);
	next_targ_valid = learn_target >= LEARN_MAX ? false : extra_target_valid(data->p, data->max_learnable, oid, next_level);

	cost = player_bonus_to_cost(learn_target, abil, data->p);
	cost_inc = cost - player_bonus_to_cost(data->p->extra_target[oid], abil, data->p);
	
	if (cursor) {
		tl_attr = COLOUR_WHITE;
		ll_attr = learn_level >= learn_target ? COLOUR_L_BLUE : COLOUR_L_GREEN;
		lt_attr = next_targ_valid ? COLOUR_L_GREEN : COLOUR_L_BLUE;
		if (!targ_valid) lt_attr = COLOUR_L_RED;
		name_attr = COLOUR_WHITE;
		cost_attr = COLOUR_L_BLUE;
		cost_inc_attr = COLOUR_L_UMBER;
	}
	else {
		tl_attr = COLOUR_L_BLUE;
		ll_attr = learn_level >= learn_target ? COLOUR_BLUE : COLOUR_GREEN;
		lt_attr = next_targ_valid ? COLOUR_GREEN : COLOUR_BLUE;
		if (!targ_valid) lt_attr = COLOUR_RED;
		name_attr = COLOUR_L_BLUE;
		cost_attr = COLOUR_BLUE;
		cost_inc_attr = COLOUR_UMBER;
	}

	c_prt(name_attr, name, row, col + ALMC_NAME + name_indent);
	if (cost > 0) c_prt(cost_attr, format("%3i", cost), row, col + ALMC_COST);
	if (cost_inc > 0) c_prt(cost_inc_attr, format("%+3i", -cost_inc), row, col + ALMC_COST_DIFF);
	c_prt(tl_attr, format("%3i", total_level), row, col + ALMC_CURR);
	c_prt(ll_attr, format("%3i", learn_level), row, col + ALMC_LRND);
	c_prt(lt_attr, format("%3i", learn_target), row, col + ALMC_TRGT);
}

/**
 * if the mode doesn't have any valid selections skip it when changing modes
 */
static bool mode_is_valid(struct menu *m, int mode)
{
	int i;

	for (i = 0; i < m->count; ++i) {
		if (ability_learn_valid_mode(m, i, mode) == MN_ROW_VALID) {
			return true;
		}
	}

	return false;
}

static int get_next_mode(struct menu *m, int mode)
{
	int result = mode;
	do {
		result = (result + 1) % AL_MODE_MAX;
	} while (!mode_is_valid(m, result) && result != mode);
	return result;
}

/**
 * change the mode of the menu, involves recalculating the menu size
 */
static void ability_learn_set_mode(struct menu *m, int new_mode)
{
	struct ability_learn_menu_data *data = menu_priv(m);
	struct ability_learn_menu_mode_data mode, next_mode;
	size_t title_size = 80;
	char *new_title = mem_zalloc(title_size * sizeof(char));
	size_t width = MAX(0, MIN(Term->wid - 15, ALMC_MAX + 3));
	region loc = { 15, 2, width, 30 };

	if (data->birth) {
		loc.col = 0;
		loc.row = 9;
		clear_from(loc.row);
	}

	data->mode = new_mode;

	mode = ability_learn_menu_modes[data->mode];
	next_mode = ability_learn_menu_modes[get_next_mode(m, data->mode)];
	assert(mode.index == data->mode);

	strnfmt(new_title, title_size, "Learning %s (/=%s)", mode.name, next_mode.name);

	if (m->title) mem_free((char *)m->title);

	m->title = new_title;

	ability_learn_valid_refresh(m);
	get_menu_filter(m);

	loc.page_rows = menu_count(m) + 3;
	loc.page_rows = MIN(loc.page_rows, Term->hgt - loc.row - 8);
	menu_layout(m, &loc);
	//menu_refresh(m, true);
	menu_move_cursor_to(m, 0);
}

static void refresh_hypothetical_player(struct menu *m)
{
	struct ability_learn_menu_data *data = menu_priv(m);
	struct player *hypo = data->p;
	struct player_ability *abil;

	if (data->p == player) {
		return;
	}

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->learn_index < 0) continue;
		else if (abil->type == PY_ABIL_SKILL) {
			hypo->extra_skills[abil->index] = data->temp_target[abil->learn_index];
		}
		else if (abil->type == PY_ABIL_POWER) {
			hypo->extra_powers[abil->index] = data->temp_target[abil->learn_index];
		}
	}

	calc_bonuses(hypo, &hypo->mon, &hypo->mon.state, false, false);

	ability_learn_valid_refresh(m);

	data->points = hypo->mon.state.extra_points_max;
}

static void on_change_target(struct menu *m)
{
	struct ability_learn_menu_data *data = menu_priv(m);

	refresh_hypothetical_player(m);
	get_max_learnable(m, data->p);
}

static bool ability_learn_handler(struct menu *m, const ui_event *e, int oid)
{
	struct ability_learn_menu_data *data = menu_priv(m);
	const struct player_ability *abil = ability_by_tome_id(oid);

	if ((e->type == EVT_KBRD && e->key.code == '+') ||
			(e->type == EVT_MOVE && target_dir(e->key) == 6)) {
		data->temp_target[oid] = tome_next_increment(data->p, abil, data->temp_target[oid]);
		on_change_target(m);
		return true;
	}
	else if ((e->type == EVT_KBRD && e->key.code == '-') ||
			(e->type == EVT_MOVE && target_dir(e->key) == 4)) {
		data->temp_target[oid] = tome_prev_increment(data->p, abil, data->temp_target[oid]);
		on_change_target(m);
		return true;
	}
	else if (e->type == EVT_KBRD && e->key.code == '/') {
		ability_learn_set_mode(m, get_next_mode(m, data->mode));
		return true;
	}
	else if (data->birth) {
		if (e->type == EVT_KBRD && e->key.code == KTRL('X')) {
			quit(NULL);
		}
	}

	return false;
}

static int ability_learn_comp_base(const struct player_ability *abil1, const struct player_ability *abil2)
{
	bool ispower1 = abil1->type == PY_ABIL_POWER;
	bool ispower2 = abil2->type == PY_ABIL_POWER;

	if (ispower1 && !ispower2) return 1;
	if (!ispower1 && ispower2) return -1;

	return strcmp(abil1->name, abil2->name);
}

/**
 * sorta abilities by their ultimate parent first, then penultimate, etc
 */
static int ability_learn_comp(int oid1, int oid2)
{
	const struct player_ability *abil1 = ability_by_tome_id(oid1);
	const struct player_ability *abil2 = ability_by_tome_id(oid2);

	int depth1 = tome_depth(abil1), depth2 = tome_depth(abil2), currdepth;

	for (currdepth = 0; currdepth <= MIN(depth1, depth2); ++currdepth) {
		const struct player_ability *depth_parent1 = abil1, *depth_parent2 = abil2;
		int i, result;
		for (i = 0; i < depth1 - currdepth; ++i) {
			depth_parent1 = abil_parent(depth_parent1, player);
			assert(depth_parent1);
		}
		for (i = 0; i < depth2 - currdepth; ++i) {
			depth_parent2 = abil_parent(depth_parent2, player);
			assert(depth_parent2);
		}

		result = ability_learn_comp_base(depth_parent1, depth_parent2);

		if (result) return result;
	}

	if (depth1 > depth2) return 1;
	if (depth2 > depth1) return -1;

	return 0;
}


static const menu_iter ability_learn_menu_iter = { 
	ability_learn_get_tag,
	ability_learn_valid,
	ability_learn_display,
	ability_learn_handler,
	NULL,
	ability_learn_comp
};

static int ability_learn_browse_desc(const struct player_ability *abil, struct player *p, bool known, int col, int row)
{
	assert(abil);

	int group, c_x, c_y;
	char desc_buf[512];
	char parent_buf[512];

	if (abil->type == PY_ABIL_POWER) group = PLAYER_FLAG_POWER;
	else if (abil->type == PY_ABIL_SKILL) group = PLAYER_FLAG_SKILL;
	else return row;

	ability_desc(p, abil, desc_buf, sizeof desc_buf, known, group);
	ability_parent_desc(abil, parent_buf, sizeof parent_buf);

	Term_gotoxy(col, row);

	text_out_c(COLOUR_WHITE, "%s%s", desc_buf, parent_buf);

	if (Term_locate(&c_x, &c_y)) {
		return row + 1;
	}

	return c_y + 1;
}

/**
 * show the points at the bottom, give a description of the current ability
 */
static void ability_learn_browse(int oid, void *db, const region *loc)
{
	struct ability_learn_menu_data *data = db;

	int points_left = data->points - data->p->mon.state.extra_points_used;
	int more_points_used = calc_extra_points_array(data->p, data->temp_target) - data->p->mon.state.extra_points_used;
	//int row = loc->row + loc->page_rows, col = loc->col - loc->width;
	//row = 20, col = 15;
	uint8_t more_pts_attr = more_points_used > points_left ? COLOUR_L_RED : COLOUR_L_GREEN;
	uint8_t curr_points_attr = points_left > 0 ? COLOUR_L_GREEN : COLOUR_L_RED;
	int i;
	int row = loc->row + loc->page_rows;
	const struct player_ability *abil = ability_by_tome_id(oid);
	const char *pts_str = "Available Points: ";
	bool known;

	text_out_hook = text_out_to_screen;
	text_out_wrap = loc->col + loc->width;
	text_out_indent = loc->col - 1;
	text_out_pad = 1;

	clear_from(row);
	++row;

	Term_gotoxy(loc->col, row);
	for (i = 0; i < loc->width; ++i) {
		Term_addch(COLOUR_WHITE, '=');
	}
	++row;

	Term_gotoxy(loc->col, row);
	text_out_c(COLOUR_WHITE, pts_str);
	text_out_c(curr_points_attr, "%2i", points_left);
	if (more_points_used > 0) {
		Term_gotoxy(loc->col + ALMC_COST_DIFF + 4, row);
		text_out_c(COLOUR_L_UMBER, "%+2i", -more_points_used);
		text_out_c(COLOUR_WHITE, " = ");
		text_out_c(more_pts_attr, "%i", points_left - more_points_used);
	}

	assert(abil);

	known = data->mode == AL_MODE_KNOWN;
	row += 2;
	Term_gotoxy(loc->col + ALMC_NAME, row);
	ability_learn_browse_desc(abil, data->p, known, loc->col + ALMC_NAME, row);
	
	text_out_wrap = 0;
	text_out_indent = 0;
	text_out_pad = 0;
}

static struct menu *ability_learn_menu_new(struct player *p, ability_learn_mode mode, int *max_learn, bool birth)
{
	struct menu *m = menu_new(MN_SKIN_SCROLL, &ability_learn_menu_iter);
	struct ability_learn_menu_data *data = mem_zalloc(sizeof *data);
	//assert(max_learn);
	assert(p->extra_target);
	//size_t width = MAX(0, MIN(Term->wid - 5, ALMC_MAX + 1));
	
	// col, row, wid, page_rows
	//region loc = { /*0 - width*/ 15, 5, width, 15 };

	data->mode = mode;
	data->max_learnable = mem_zalloc(sizeof *data->max_learnable * z_info->learn_max);
	data->temp_target = mem_zalloc(sizeof *data->temp_target * z_info->learn_max);
	data->extra_max_learnable = mem_zalloc(sizeof *data->extra_max_learnable * z_info->learn_max);
	data->valid = mem_zalloc(sizeof *data->valid * z_info->learn_max);
	data->birth = birth;
	data->p = p;

	if (p->num_evol_choices > 0) {
		data->end_monster = p->evol_choices[p->num_evol_choices - 1];
	} else {
		data->end_monster = NULL;
	}

	if (max_learn) {
		memcpy(data->extra_max_learnable, max_learn, sizeof *data->max_learnable * z_info->learn_max);
	}
	memcpy(data->temp_target, p->extra_target, sizeof *data->temp_target * z_info->learn_max);

	menu_setpriv(m, z_info->learn_max, data);

	get_max_learnable(m, p);

	if (birth) {
		m->header = "   Name                               Cost  Diff  Max   Lrnd  Trgt";
	}
	else {
		m->header = "   Name                               Cost  Diff  Curr  Lrnd  Trgt";
	}
	m->selections = all_letters_nohjkl;
	m->cmd_keys = "/+-";
	m->browse_hook = ability_learn_browse;
	m->flags = MN_PVT_TAGS | MN_DBL_TAP;

	ability_learn_set_mode(m, mode);
	refresh_hypothetical_player(m);
	data->points = data->p->mon.state.extra_points_max;

	return m;
}

static void ability_learn_menu_destroy(struct menu *m)
{
	struct ability_learn_menu_data *data = menu_priv(m);

	//free_max_level_powers(data);
	mem_free(data->max_learnable);
	mem_free(data->temp_target);
	mem_free(data->extra_max_learnable);
	mem_free(data->valid);
	mem_free(data);
	mem_free((char *)m->title);

	menu_free(m);
}

/**
 * i'm not telling you what this function does
 * mind your own business
 */
static bool validate_result(struct player *p, struct menu *m, bool correct, int points)
{
	int i;
	struct ability_learn_menu_data *data = menu_priv(m);

	for (i = 0; i < m->count; ++i) {
		if (!extra_target_valid(data->p, data->max_learnable, i, data->temp_target[i])) {
			if (correct) data->temp_target[i] = p->extra_target[i];
			else return false;
		}
	}

	if (calc_extra_points_array(data->p, data->temp_target) > points) {
		if (correct) memcpy(data->temp_target, p->extra_target, sizeof *data->temp_target * z_info->learn_max);
		else return false;
	}

	return true;
}

/**
 * if we're in birth check if we're not using all our points
 */
static bool validate_birth(struct player *p, struct menu *m, int points)
{
	struct ability_learn_menu_data *data = menu_priv(m);

	if (!get_check("Use these targets? ")) {
		return false;
	}

	if (calc_extra_points_array(data->p, data->temp_target) < points) {
		if (!get_forced_check("Unused points will be lost permanently. Proceed? ")) {
			return false;
		}
	}

	return true;
}

/**
 * make a version of the player that (approximates what) they would be at the end of the game
 */
static struct player *hypothetical_player(const struct player *p)
{
	struct player *hypo = mem_zalloc(sizeof *hypo);
	struct monster_race *hypo_race = mem_zalloc(sizeof *hypo_race);
	int i;

	memcpy(hypo, p, sizeof *hypo);

	for (i = 0; i < STAT_MAX; ++i) {
		hypo->stat_cur[i] = stat_max_max(hypo, i);
	}

	if (hypo->num_evol_choices > 0) {
		memcpy(hypo_race, hypo->evol_choices[hypo->num_evol_choices - 1], sizeof *hypo_race);
		rearrange_monster(hypo_race, true);
		hypo->mon.race = hypo_race;
	}

	hypo->max_lev = PY_MAX_LEVEL;
	hypo->lev = PY_MAX_LEVEL;
	hypo->mon.player = hypo;

	return hypo;
}

static void free_hypothetical_player(struct player *hypo)
{
	if (hypo) {
		mem_free(hypo->mon.race);
		mem_free(hypo);
	}
}

bool textui_powers_learn(struct player *p, int *max_learn, bool birth)
{
	struct menu *m;
	struct ability_learn_menu_data *data;
	struct player *hypo = NULL;
	struct player *use;
	bool done = false;
	bool changed = false;
	bool go_back;
	int i;

	make_ability_subchoice(p);

	if (OPT(p, birth_level_one_learn)) {
		if (birth) {
			hypo = hypothetical_player(p);
		}
	}

	use = hypo ? hypo : p;

	m = ability_learn_menu_new(use, AL_MODE_KNOWN, max_learn, birth);
	data = menu_priv(m);

	assert(m);

	screen_save();

	while (!done) {
		ui_event evt;

		evt = menu_select(m, 0, false);
		go_back = evt.type == EVT_ESCAPE;

		if (go_back) {
			changed = false;
			for (i = 0; !changed && i < z_info->learn_max; ++i) {
				changed = p->extra_target[i] != data->temp_target[i];
			}
			done = !changed || get_check("Discard changes? ");
		}
		else {
			done = validate_result(use, m, false, data->points);
		
			if (!done) {
				if (get_check("Discard changes? ")) {
					validate_result(use, m, true, data->points);
				}
			}

			if (birth && !validate_birth(use, m, data->points)) {
				done = false;
			}
		}
	}

	if (!go_back) {
		for (i = 0; !changed && i < z_info->learn_max; ++i) {
			changed = p->extra_target[i] != data->temp_target[i];
		}

		if (changed && (birth || get_forced_check("Use these targets? "))) {
			memcpy(p->extra_target, data->temp_target, sizeof *p->extra_target * z_info->learn_max);
			p->upkeep->update |= PU_BONUS;
		}
	}

	screen_load();

	if (hypo) free_hypothetical_player(hypo);

	ability_learn_menu_destroy(m);

	make_ability_subchoice(p);

	return !go_back;
}




struct ability_subchoice_menu_data {
	const struct player_ability *abil;
};

static void ability_subchoice_browse(int oid, void *db, const region *loc)
{
	return;
}

static int ability_subchoice_valid(struct menu *menu, int oid)
{
	return MN_ROW_VALID;
}

static void ability_subchoice_display(struct menu *menu, int oid, bool cursor,
		int row, int col, int width)
{
	struct ability_subchoice_menu_data *data = menu_priv(menu);

	const char *base = ability_subchoice_name(oid, data->abil);
	char name[80];

	assert(base);

	strnfmt(name, sizeof name, "%s", base);
	my_strcap_full(name);

	Term_gotoxy(col, row);
	text_out_c(COLOUR_WHITE, "%s", name);
}

static bool ability_subchoice_handler(struct menu *m, const ui_event *e, int oid)
{
	struct ability_subchoice_menu_data *data = menu_priv(m);

	if (e->type == EVT_SELECT) {
		const char *name = ability_subchoice_name(oid, data->abil);
		const char *type = ability_subchoice_title(data->abil);
		return !get_check(format("Choose the %s %s? ", type, name));
	}

	return false;
}

static const menu_iter ability_subchoice_menu_iter = { 
	NULL,
	ability_subchoice_valid,
	ability_subchoice_display,
	ability_subchoice_handler,
	NULL,
	NULL
};

static struct menu *ability_subchoice_menu_new(struct player_ability *abil)
{
	struct menu *m;
	struct ability_subchoice_menu_data *data;
	char *title;
	int n_choices = ability_subchoice_choices(abil);
	const char *choice_name = ability_subchoice_title(abil);
	region loc = { 25, 1, 50, 30 };

	if (n_choices <= 0) return NULL;
	assert(choice_name);

	data = mem_zalloc(sizeof *data);
	data->abil = abil;

	title = mem_zalloc(80 * sizeof *title);
	strnfmt(title, 80, "%s: choose which %s?", abil->name, choice_name);

	loc.page_rows = n_choices + 3;

	m = menu_new(MN_SKIN_SCROLL, &ability_subchoice_menu_iter);
	m->title = title;
	m->browse_hook = ability_subchoice_browse;
	m->selections = all_letters_nohjkl;
	menu_setpriv(m, n_choices, data);
	menu_layout(m, &loc);

	return m;
}

static void ability_subchoice_menu_destroy(struct menu *m)
{
	struct ability_subchoice_menu_data *data = menu_priv(m);

	mem_free(data);

	string_free((char *)m->title);
	menu_free(m);
}

bool textui_ability_subchoice(struct player *p, struct player_ability *abil)
{
	if (p->extra_choice[abil->learn_index] >= 0) return false;

	struct menu *m = ability_subchoice_menu_new(abil);
	const ui_event evt = menu_select(m, 0, true);
	bool made_choice = false;

	if (evt.type == EVT_SELECT) {
		int selection = m->cursor;
		p->extra_choice[abil->learn_index] = selection;
		made_choice = true;
	}

	ability_subchoice_menu_destroy(m);

	return made_choice;
}





struct evolution_choice_menu_data {
	const struct evolution *choices;
	const struct monster_race *which;
	bool birth;
};

static void evolution_choice_browse(int oid, void *db, const region *loc)
{
	return;
}

static int evolution_choice_valid(struct menu *menu, int oid)
{
	return MN_ROW_VALID;
}

static void evolution_choice_display(struct menu *menu, int oid, bool cursor,
		int row, int col, int width)
{
	struct evolution_choice_menu_data *data = menu_priv(menu);
	char desc[80];
	int i;
	const struct evolution *curr = data->choices;
	uint8_t colour = cursor ? COLOUR_WHITE : COLOUR_L_BLUE;
	if (data->which && data->which->ridx == curr->race->ridx) {
		colour = cursor ? COLOUR_L_GREEN : COLOUR_GREEN;
	}

	assert(curr);

	assert(oid >= 0 && oid < menu->count);

	for (i = 0; i < oid; ++i) {
		assert(curr->next);
		curr = curr->next;
	}

	strnfmt(desc, sizeof desc, curr->race->name);

	my_strcap_full(desc);

	Term_gotoxy(col, row);
	text_out_c(colour, "%s", desc);
}

static bool evolution_choice_handler(struct menu *m, const ui_event *e, int oid)
{
	struct evolution_choice_menu_data *data = menu_priv(m);
	int i;

	if (e->type == EVT_SELECT) {
		const struct evolution *select = data->choices;
		for (i = 0; i < oid; ++i) {
			select = select->next;
		}
		data->which = select->race;
	}
	else if (data->birth) {
		if (e->key.code == KTRL('X')) {
			quit(NULL);
		}
	}
	return false;
}

static const menu_iter evolution_choice_menu_iter = { 
	NULL,
	evolution_choice_valid,
	evolution_choice_display,
	evolution_choice_handler,
	NULL,
	NULL
};

static struct menu *evolution_choice_menu_new(const struct evolution *curr, bool birth)
{
	struct menu *m;
	struct evolution_choice_menu_data *data;
	int num_choices = 0;
	const struct evolution *evol;
	region loc = { 25, 1, 50, 30 };

	for (evol = curr; evol; evol = evol->next) {
		++num_choices;
	}

	if (num_choices <= 1) return NULL;

	m = menu_new(MN_SKIN_SCROLL, &evolution_choice_menu_iter);
	data = mem_zalloc(sizeof *data);

	data->choices = curr;
	data->which = NULL;
	data->birth = birth;

	loc.page_rows = MAX(num_choices + 3, 25);

	m->title = "Evolve into which monster?";
	m->header = "  Name";
	m->browse_hook = evolution_choice_browse;
	m->selections = all_letters_nohjkl;

	menu_setpriv(m, num_choices, data);
	menu_layout(m, &loc);

	return m;
}

static void evolution_choice_menu_free(struct menu *m)
{
	struct evolution_choice_menu_data *data = menu_priv(m);

	mem_free(data);
	menu_free(m);
}

const struct monster_race *evolution_choice_menu_select(const struct evolution *evol, bool birth)
{
	struct menu *m;
	struct evolution_choice_menu_data *data;
	const struct monster_race *result;

	if (!evol) return NULL;
	if (!evol->next) return evol->race;

	m = evolution_choice_menu_new(evol, birth);

	assert(m);

	menu_select(m, 0, true);

	data = menu_priv(m);
	result = data->which;

	evolution_choice_menu_free(m);

	return result;
}


