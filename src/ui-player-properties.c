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
#include "player-calcs.h"
#include "player-properties.h"
#include "player-util.h"
#include "ui-input.h"
#include "ui-menu.h"
#include "ui-player-properties.h"

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


/**
 * Show ability long description when browsing
 */
static void view_ability_menu_browser(int oid, void *data, const region *loc)
{
	struct player_ability *choices = data;
	int monster_powers[PP_MAX] = { 0 };
	int monster_skills[SKILL_MAX] = { 0 };
	int race_skills[SKILL_MAX] = { 0 };
	int race_x_skills[SKILL_MAX] = { 0 };
	struct monster_race *mrace = lookup_player_monster(player);

	player_race_r_skill(player->race, mrace ? true : false, race_skills);
	player_race_x_skill(player->race, mrace ? true : false, race_x_skills);

	/* Redirect output to the screen */
	text_out_hook = text_out_to_screen;
	text_out_wrap = 60;
	text_out_indent = loc->col - 1;
	text_out_pad = 1;

	/* L: more info for powers and skills */
	char extra[128];
	extra[0] = '\0';
	if (mrace) {
		calc_monster_powers(mrace, monster_powers);
		calc_monster_skills(mrace, monster_skills);
	}
	if (choices[oid].group == PLAYER_FLAG_POWER || choices[oid].group == PLAYER_FLAG_SKILL) {
		int cbase, cxtra, rbase, rxtra, tome, stat;
		const char *stat_name = NULL;
		if (choices[oid].group == PLAYER_FLAG_POWER) {
			cbase = 0;
			cxtra = player_class_power(player, choices[oid].index);
			rbase = monster_powers[choices[oid].index];
			rxtra = player->race->r_powers[choices[oid].index];
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
	}


	clear_from(loc->row + loc->page_rows);
	Term_gotoxy(loc->col, loc->row + loc->page_rows);
	text_out_c(COLOUR_L_BLUE, "\n%s%s\n", (char *) choices[oid].desc, extra);

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
	menu_iter menu_f = { view_ability_tag, 0, view_ability_display, 0, 0 };
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

