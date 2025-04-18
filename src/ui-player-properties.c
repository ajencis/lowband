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
#include "player-calcs.h"
#include "player-properties.h"
#include "player-util.h"
#include "ui-input.h"
#include "ui-knowledge.h"
#include "ui-menu.h"
#include "ui-player-properties.h"
#include "ui-target.h"



static void add_scaling_desc(char *buf, const char *name, int base, int scale, int numleft, size_t bufsize)
{
	if (!base && !scale) return;
	if (base) {
		my_strcat(buf, format("%i", base), bufsize);
	}
	if (base && scale) {
		my_strcat(buf, " + ", bufsize);
	}
	if (scale) {
		my_strcat(buf, format("%i%% of your level", scale), bufsize);
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
		my_strcat(buf, ".", bufsize);
	}
}

static void ability_desc(struct player *p, const struct player_ability *ability, char *buf, size_t bufsize, bool player_has, int group)
{
	int monster_powers[PP_MAX] = { 0 };
	int monster_skills[SKILL_MAX] = { 0 };
	int race_skills[SKILL_MAX] = { 0 };
	int race_x_skills[SKILL_MAX] = { 0 };
	struct monster_race *mrace = lookup_player_monster(player);

	assert(bufsize > 0);

	player_race_r_skill(player->race, mrace ? true : false, race_skills);
	player_race_x_skill(player->race, mrace ? true : false, race_x_skills);

	buf[0] = '\0';

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
		calc_monster_powers(mrace, monster_powers, &player->state);
		calc_monster_skills(mrace, monster_skills);
	}
	if (group == PLAYER_FLAG_POWER || group == PLAYER_FLAG_SKILL) {
		int cbase, cxtra, rbase, rxtra, tome, stat;
		char stat_name[80];
		if (group == PLAYER_FLAG_POWER) {
			cbase = 0;
			cxtra = player_class_power(player, ability->index);
			rbase = monster_powers[ability->index];
			rxtra = player_race_power(player, ability->index);
			tome = player->extra_powers[ability->index] / 2;
			stat = 0;
		}
		else {
			int stat1, stat2;
			player_skill_stats(player, &player->state, ability->index, &stat1, &stat2);
			cbase = player_class_c_skill(player, ability->index);
			cxtra = player_class_x_skill(player, ability->index) * 100 / 10;
			rbase = race_skills[ability->index] + monster_skills[ability->index];
			rxtra = race_x_skills[ability->index] * 100 / 10;
			tome = player->extra_skills[ability->index];
			if (stat1 != STAT_NONE) {
				int ind = player_skill_stat_ind(player, &player->state, ability->index);
				int curr;
				stat = adj_stat_skill_flat(ind, ability->index);
				curr = cbase + rbase + (cxtra + rxtra) * player->lev / 100 + tome;
				curr = MAX(curr, 0);
				stat += curr * adj_stat_skill_percent(ind, ability->index) / 100;
				if (stat2 == STAT_NONE) {
					strnfmt(stat_name, sizeof(stat_name), stat_idx_to_name(stat1));
				} else {
					strnfmt(stat_name, sizeof(stat_name), "%s and %s",
						stat_idx_to_name(stat1), stat_idx_to_name(stat2));
				}
			} else {
				stat = 0;
			}
		}
		int numleft = ((rxtra || rbase) ? 1 : 0) +
				((cxtra || cbase) ? 1 : 0) +
				(tome ? 1 : 0) +
				(stat ? 1 : 0);
		if (numleft > 0) {
			my_strcat(buf, " You gain ", bufsize);
			if (cbase || cxtra) {
 				add_scaling_desc(buf, "class", cbase, cxtra, numleft, bufsize);
				--numleft;
			}
			if (rbase || rxtra) {
				add_scaling_desc(buf, "race", rbase, rxtra, numleft, bufsize);
				--numleft;
			}
			if (tome) {
				add_scaling_desc(buf, "learning", tome, 0, numleft, bufsize);
				--numleft;
			}
			if (stat) {
				add_scaling_desc(buf, stat_name, stat, 0, numleft, bufsize);
				--numleft;
			}
		}
	}
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
	struct player_ability *choices = menu->menu_data;

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
			strnfmt(buf, sizeof(buf), "Power:  %s (level %i)", 
				choices[oid].name, player->state.powers[choices[oid].index]);
			color = COLOUR_GREEN;
			break;
		}
	case PLAYER_FLAG_SKILL:
		{
			strnfmt(buf, sizeof(buf), "Skill:  %s (level %i)",
				choices[oid].name, player->state.skills[choices[oid].index]);
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
	struct player_ability *choices = data;
	char buf[256];
	/*int monster_powers[PP_MAX] = { 0 };
	int monster_skills[SKILL_MAX] = { 0 };
	int race_skills[SKILL_MAX] = { 0 };
	int race_x_skills[SKILL_MAX] = { 0 };
	struct monster_race *mrace = lookup_player_monster(player);

	player_race_r_skill(player->race, mrace ? true : false, race_skills);
	player_race_x_skill(player->race, mrace ? true : false, race_x_skills);*/

	/* Redirect output to the screen */
	text_out_hook = text_out_to_screen;
	text_out_wrap = 60;
	text_out_indent = loc->col - 1;
	text_out_pad = 1;

	/* L: more info for powers and skills */
	/*char extra[128];
	extra[0] = '\0';
	if (mrace) {
		calc_monster_powers(mrace, monster_powers, &player->state);
		calc_monster_skills(mrace, monster_skills);
	}
	if (choices[oid].group == PLAYER_FLAG_POWER || choices[oid].group == PLAYER_FLAG_SKILL) {
		int cbase, cxtra, rbase, rxtra, tome, stat;
		const char *stat_name = NULL;
		if (choices[oid].group == PLAYER_FLAG_POWER) {
			cbase = 0;
			cxtra = player_class_power(player, choices[oid].index);
			rbase = monster_powers[choices[oid].index];
			rxtra = player_race_power(player, choices[oid].index);
			tome = player->extra_powers[choices[oid].index] / 2;
			stat = 0;
		}
		else {
			int whichstat = player_skill_stat(player, choices[oid].index);
			cbase = player_class_c_skill(player, choices[oid].index);
			cxtra = player_class_x_skill(player, choices[oid].index) * 100 / 10;
			rbase = race_skills[choices[oid].index] + monster_skills[choices[oid].index];
			rxtra = race_x_skills[choices[oid].index] * 100 / 10;
			tome = player->extra_skills[choices[oid].index];
			if (whichstat != -1) {
				int ind = player->state.stat_ind[whichstat];
				int curr;
				stat = adj_stat_skill_flat(ind, choices[oid].index);
				curr = cbase + rbase + (cxtra + rxtra) * player->lev / 100 + tome;
				curr = MAX(curr, 0);
				stat += curr * adj_stat_skill_percent(ind, choices[oid].index) / 100;
				stat_name = stat_idx_to_name(whichstat);
			} else {
				stat = 0;
			}
		}
		int numleft = ((rxtra || rbase) ? 1 : 0) +
				((cxtra || cbase) ? 1 : 0) +
				(tome ? 1 : 0) +
				(stat ? 1 : 0);
		if (numleft > 0) {
			my_strcat(extra, " You gain ", sizeof(extra));
			if (cbase || cxtra) {
 				add_scaling_desc(extra, "class", cbase, cxtra, numleft, sizeof(extra));
				--numleft;
			}
			if (rbase || rxtra) {
				add_scaling_desc(extra, "race", rbase, rxtra, numleft, sizeof(extra));
				--numleft;
			}
			if (tome) {
				add_scaling_desc(extra, "learning", tome, 0, numleft, sizeof(extra));
				--numleft;
			}
			if (stat && stat_name) {
				add_scaling_desc(extra, stat_name, stat, 0, numleft, sizeof(extra));
				--numleft;
			}
		}
	}*/

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
	{ AL_MODE_SKILLS, "Skills" },
	{ AL_MODE_POWERS, "Powers" },
	{ AL_MODE_ALL_POWERS, "All Powers" },
};

struct ability_learn_menu_data {
	int mode;
	int max_learnable[TOME_MAX];
	uint16_t temp_target[TOME_MAX];
};

enum ability_learn_menu_data_column_locations {
	ALMC_NAME = 0,
	ALMC_COST = ALMC_NAME + 25,
	ALMC_COST_DIFF = ALMC_COST + 6,
	ALMC_CURR = ALMC_COST_DIFF + 6,
	ALMC_LRND = ALMC_CURR + 6,
	ALMC_TRGT = ALMC_LRND + 6,
	ALMC_COST_RESULT = ALMC_COST_DIFF + 5,
	ALMC_MAX = ALMC_TRGT + 5
};


static struct player_ability *ability_by_tome_id(int tome_id)
{
	struct player_ability *pa;

	const char *type = tome_id < PP_MAX ? "power" : "skill";
	int abil_id = tome_id < PP_MAX ? tome_id : tome_id - PP_MAX;

	for (pa = player_abilities; pa; pa = pa->next) {
		if (abil_id == pa->index && streq(pa->type, type)) {
			return pa;
		}
	}

	return NULL;
}

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


static int ability_learn_valid_mode(struct menu *menu, int oid, int mode)
{
	struct ability_learn_menu_data *data = menu_priv(menu);

	if (oid <= TOME_NONE || oid >= TOME_MAX) return MN_ROW_SKIP;

	if (oid < PP_MAX) {
		if (mode != AL_MODE_POWERS && mode != AL_MODE_ALL_POWERS) return MN_ROW_SKIP;
		if (player->extra_powers[oid] <= 0 &&
				player->state.powers[oid] <= 0) {
			// if it's the full menu show all powers
			// if it's the partial menu only show learned powers
			if (mode == AL_MODE_ALL_POWERS) {
				return data->max_learnable[oid] > 0 ? MN_ROW_VALID : MN_ROW_INVALID;
			}
			return MN_ROW_SKIP;
		}
		return MN_ROW_VALID;
	}
	else {
		if (mode != AL_MODE_SKILLS) return MN_ROW_SKIP;
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
	int total_level, learn_level, learn_target, cost, cost_inc;
	uint8_t tl_attr, ll_attr, lt_attr, name_attr, cost_attr, cost_inc_attr;
	struct ability_learn_menu_data *data = menu_priv(m);
	bool targ_valid;

	if (oid <= TOME_NONE || oid >= TOME_MAX) return;

	if (oid < PP_MAX) {
		assert(oid > PP_NONE);
		strcpy(name, lookup_power_name(oid));
		if (oid < MS_MAX) {
			strcat(name, " Magic");
		}
		total_level = player->state.powers[oid];
		learn_level = player->extra_powers[oid];
	}
	else {
		int skill = oid - PP_MAX;
		strcpy(name, skill_index_to_name(skill));
		my_strcap_full(name);
		total_level = player->state.skills[skill];
		learn_level = player->extra_skills[skill];
	}
	learn_target = data->temp_target[oid];

	targ_valid = extra_target_valid(player, data->max_learnable, oid, learn_target);

	cost = player_bonus_to_cost(learn_target, oid, player);
	cost_inc = cost - player_bonus_to_cost(player->extra_target[oid], oid, player);

	//points_valid = calc_extra_points_array(player, data->temp_target) <= (uint16_t)player->state.extra_points_max;
	
	if (cursor) {
		tl_attr = COLOUR_WHITE;
		ll_attr = learn_level >= learn_target ? COLOUR_L_BLUE : COLOUR_L_GREEN;
		lt_attr = targ_valid ? COLOUR_L_GREEN : COLOUR_L_RED;
		name_attr = COLOUR_WHITE;
		cost_attr = COLOUR_L_BLUE;
		cost_inc_attr = COLOUR_L_UMBER;
	}
	else {
		tl_attr = COLOUR_L_BLUE;
		ll_attr = learn_level >= learn_target ? COLOUR_BLUE : COLOUR_GREEN;
		lt_attr = targ_valid ? COLOUR_GREEN : COLOUR_RED;
		name_attr = COLOUR_L_BLUE;
		cost_attr = COLOUR_BLUE;
		cost_inc_attr = COLOUR_UMBER;
	}

	c_prt(name_attr, name, row, col + ALMC_NAME);
	if (cost > 0) c_prt(cost_attr, format("%3i", cost), row, col + ALMC_COST);
	if (cost_inc > 0) c_prt(cost_inc_attr, format("%+2i", -cost_inc), row, col + ALMC_COST_DIFF);
	c_prt(tl_attr, format("%3i", total_level), row, col + ALMC_CURR);
	c_prt(ll_attr, format("%3i", learn_level), row, col + ALMC_LRND);
	c_prt(lt_attr, format("%3i", learn_target), row, col + ALMC_TRGT);
}

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

static void ability_learn_set_mode(struct menu *m, int new_mode)
{
	struct ability_learn_menu_data *data = menu_priv(m);
	struct ability_learn_menu_mode_data mode, next_mode;
	size_t title_size = 80;
	char *new_title = mem_zalloc(title_size * sizeof(char));
	size_t width = MAX(0, MIN(Term->wid - 15, ALMC_MAX + 3));
	region loc = { 15, 2, width, 25 };

	data->mode = new_mode;

	mode = ability_learn_menu_modes[data->mode];
	next_mode = ability_learn_menu_modes[get_next_mode(m, data->mode)];
	assert(mode.index == data->mode);

	strnfmt(new_title, title_size, "Learning %s (/=%s)", mode.name, next_mode.name);

	if (m->title) mem_free((char *)m->title);

	m->title = new_title;
	get_menu_filter(m);
	loc.page_rows = menu_count(m) + 3;
	loc.page_rows = MIN(loc.page_rows, 25);
	menu_layout(m, &loc);
	//menu_refresh(m, true);
	menu_move_cursor_to(m, 0);
}

static bool ability_learn_handler(struct menu *m, const ui_event *e, int oid)
{
	struct ability_learn_menu_data *data = menu_priv(m);

	if (e->type == EVT_SELECT) {
		return true;
	}
	else if ((e->type == EVT_KBRD && e->key.code == '+') ||
			(e->type == EVT_MOVE && target_dir(e->key) == 6)) {
		data->temp_target[oid] = tome_next_increment(player, oid, data->temp_target[oid]);
		return true;
	}
	else if ((e->type == EVT_KBRD && e->key.code == '-') ||
			(e->type == EVT_MOVE && target_dir(e->key) == 4)) {
		data->temp_target[oid] = tome_prev_increment(player, oid, data->temp_target[oid]);
		return true;
	}
	else if (e->type == EVT_KBRD) {
		if (e->key.code == '/') {
			ability_learn_set_mode(m, get_next_mode(m, data->mode));
			return true;
		}
	}

	return false;
}

static int ability_learn_comp(int tome1, int tome2)
{
	assert(tome1 > TOME_NONE && tome2 < TOME_MAX);
	assert(tome2 > TOME_NONE && tome1 < TOME_MAX);

	if (tome1 < PP_MAX && tome2 >= PP_MAX) return -1;
	if (tome2 < PP_MAX && tome1 >= PP_MAX) return 1;

	if (tome1 < PP_MAX) {
		assert(tome1 > PP_NONE && tome2 > PP_NONE);
		const char *name1 = lookup_power_name(tome1);
		const char *name2 = lookup_power_name(tome2);

		return strcmp(name1, name2);
	}

	else {
		const char *name1 = skill_index_to_name(tome1 - PP_MAX);
		const char *name2 = skill_index_to_name(tome2 - PP_MAX);

		return strcmp(name1, name2);
	}
}


static const menu_iter ability_learn_menu_iter = { 
	NULL,
	ability_learn_valid,
	ability_learn_display,
	ability_learn_handler,
	NULL,
	ability_learn_comp
};

static void ability_learn_browse(int oid, void *db, const region *loc)
{
	struct ability_learn_menu_data *data = db;

	int points_left = player->state.extra_points_max - player->state.extra_points_used;
	int more_points_used = calc_extra_points_array(player, data->temp_target) - player->state.extra_points_used;
	//int row = loc->row + loc->page_rows, col = loc->col - loc->width;
	//row = 20, col = 15;
	uint8_t more_pts_attr = more_points_used > points_left ? COLOUR_L_RED : COLOUR_L_GREEN;
	uint8_t curr_points_attr = points_left > 0 ? COLOUR_L_GREEN : COLOUR_L_RED;
	int i;
	int row = loc->row + loc->page_rows;
	const struct player_ability *abil = ability_by_tome_id(oid);
	const char *pts_str = "Available Points: ";

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

	if (abil) {
		int group = oid < PP_MAX ? PLAYER_FLAG_POWER : PLAYER_FLAG_SKILL;
		char buf[256];
		
		ability_desc(player, abil, buf, sizeof(buf), data->mode != AL_MODE_ALL_POWERS, group);
		row += 2;
		Term_gotoxy(loc->col, row);
		text_out_c(COLOUR_WHITE, "%s", buf);
	}
	else {
		plog_fmt("can't find abil for oid %i", oid);
	}
	
	text_out_wrap = 0;
	text_out_indent = 0;
	text_out_pad = 0;
}

static struct menu *ability_learn_menu_new(struct player *p, ability_learn_mode mode, int max_learn[TOME_MAX])
{
	struct menu *m = menu_new(MN_SKIN_SCROLL, &ability_learn_menu_iter);
	struct ability_learn_menu_data *data = mem_zalloc(sizeof *data);
	//size_t width = MAX(0, MIN(Term->wid - 5, ALMC_MAX + 1));
	
	// col, row, wid, page_rows
	//region loc = { /*0 - width*/ 15, 5, width, 15 };

	data->mode = mode;
	memcpy(data->temp_target, p->extra_target, sizeof(data->temp_target));
	memcpy(data->max_learnable, max_learn, sizeof(data->max_learnable));

	//menu_layout(m, &loc);

	menu_setpriv(m, TOME_MAX - 1, data);

	m->header = "   Name                     Cost  Diff  Curr  Lrnd  Trgt";
	m->selections = all_letters_nohjkl;
	m->cmd_keys = "/+-";
	m->browse_hook = ability_learn_browse;

	ability_learn_set_mode(m, mode);

	return m;
}

static void ability_learn_menu_destroy(struct menu *m)
{
	struct ability_learn_menu_data *data = menu_priv(m);

	mem_free(data);
	mem_free((char *)m->title);
	menu_free(m);
}

void textui_powers_learn(struct player *p, int max_learn[TOME_MAX])
{
	struct menu *m = ability_learn_menu_new(p, AL_MODE_SKILLS, max_learn);
	struct ability_learn_menu_data *data = menu_priv(m);
	bool done = false;
	bool changed = false;
	int i;

	assert(m);

	screen_save();

	while (!done) {
		menu_select(m, 0, false);

		done = true;
		for (i = TOME_NONE + 1; i < m->count; ++i) {
			if (!extra_target_valid(p, data->max_learnable, i, data->temp_target[i])) {
				done = false;
				data->temp_target[i] = p->extra_target[i];
			}
		}

		for (i = 0; !changed && i < TOME_MAX; ++i) {
			changed = p->extra_target[i] != data->temp_target[i];
		}

		if (changed && (calc_extra_points_array(p, data->temp_target) > (uint16_t)p->state.extra_points_max)) {
			if (get_forced_check("Discard changes? ")) {
				memcpy(data->temp_target, p->extra_target, sizeof(data->temp_target));
				changed = false;
			}
			else {
				done = false;
			}
		}
	}

	if (changed && get_forced_check("Use these targets? ")) {
		memcpy(p->extra_target, data->temp_target, sizeof(p->extra_target));
		p->upkeep->update |= PU_BONUS;
	}

	screen_load();

	ability_learn_menu_destroy(m);
}
