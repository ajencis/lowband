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
	case PLAYER_FLAG_POWER:
	    {
			strnfmt(buf, sizeof(buf), "Power:  %s (level %i)", 
			    choices[oid].name, player->state.powers[choices[oid].index]);
			color = COLOUR_GREEN;
			break;
		}
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

	/* Redirect output to the screen */
	text_out_hook = text_out_to_screen;
	text_out_wrap = 60;
	text_out_indent = loc->col - 1;
	text_out_pad = 1;

	/* L: more info for powers */
	char extra[128];
	extra[0] = '\0';
	if (choices[oid].group == PLAYER_FLAG_POWER) {
		int cpower = player_class_power(player, choices[oid].index);
		int rpower = player->race->r_powers[choices[oid].index];
		int xpower = player->extra_powers[choices[oid].index] / 2;
		bool any = cpower || rpower || xpower;
		if (any) {
			my_strcat(extra, " You gain ", sizeof(extra));
			bool last = !rpower && !xpower;
			bool second_last = !rpower || !xpower;
			if (cpower) {
				my_strcat(extra, format("%i%% from your class", cpower), sizeof(extra));
				if (last) my_strcat(extra, ".", sizeof(extra));
				else if (second_last) my_strcat(extra, " and ", sizeof(extra));
				else my_strcat(extra, ", ", sizeof(extra));
			}
			last = !xpower;
			if (rpower) {
				my_strcat(extra, format("%i%% from your race", rpower), sizeof(extra));
				if (last) my_strcat(extra, ".", sizeof(extra));
				else my_strcat(extra, ", and ", sizeof(extra));
			}
			if (xpower) {
				my_strcat(extra, format("up to %i points from your learning.", xpower), sizeof(extra));
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
		"Race and class abilities (%c-%c, ESC=exit): ",
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

