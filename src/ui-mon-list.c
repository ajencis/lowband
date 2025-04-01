/**
 * \file ui-mon-list.c
 * \brief Monster list UI.
 *
 * Copyright (c) 1997-2007 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2013 Ben Semmler
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

#include "cmd-core.h"
#include "mon-desc.h"
#include "mon-list.h"
#include "mon-lore.h"
#include "mon-util.h"
#include "player-timed.h"
#include "ui-menu.h"
#include "ui-mon-list.h"
#include "ui-output.h"
#include "ui-prefs.h"
#include "ui-term.h"

/**
 * Format a section of the monster list: a header followed by monster list
 * entry rows.
 *
 * This function will process each entry for the given section. It will display:
 * - monster char;
 * - number of monsters;
 * - monster name (truncated, if needed to fit the line);
 * - whether or not the monster is asleep (and how many if in a group);
 * - monster distance from the player (aligned to the right side of the list).
 * By passing in a NULL textblock, the maximum line width of the section can be
 * found.
 *
 * \param list is the monster list to format.
 * \param tb is the textblock to produce or NULL if only the dimensions need to
 * be calculated.
 * \param section is the section of the monster list to format.
 * \param lines_to_display are the number of entries to display (not including
 * the header).
 * \param max_width is the maximum line width.
 * \param prefix is the beginning of the header; the remainder is appended with
 * the number of monsters.
 * \param show_others is used to append "other monsters" to the header,
 * after the number of monsters.
 * \param max_width_result is returned with the width needed to format the list
 * without truncation.
 */
static void monster_list_format_section(const monster_list_t *list, textblock *tb, monster_list_section_t section, int lines_to_display, int max_width, const char *prefix, bool show_others, size_t *max_width_result)
{
	int remaining_monster_total = 0;
	int line_count = 0;
	int index;
	int total;
	char line_buffer[200];
	const char *punctuation = (lines_to_display == 0) ? "." : ":";
	const char *others = (show_others) ? "other " : "";
	size_t max_line_length = 0;

	if (list == NULL || list->entries == NULL)
		return;

	total = list->distinct_entries;

	if (list->total_monsters[section] == 0) {
		max_line_length = strnfmt(line_buffer, sizeof(line_buffer),
								  "%s no monsters.\n", prefix);

		if (tb != NULL)
			textblock_append(tb, "%s", line_buffer);

		/* Force a minimum width so that the prompt doesn't get cut off. */
		if (max_width_result != NULL)
			*max_width_result = MAX(max_line_length, 40);

		return;
	}

	max_line_length = strnfmt(line_buffer, sizeof(line_buffer),
							  "%s %d %smonster%s%s\n",
							  prefix,
							  list->total_monsters[section],
							  others,
							  PLURAL(list->total_monsters[section]),
							  punctuation);

	if (tb != NULL)
		textblock_append(tb, "%s", line_buffer);

	for (index = 0; index < total && line_count < lines_to_display; index++) {
		char asleep[20] = { '\0' };
		char location[20] = { '\0' };
		uint8_t line_attr;
		size_t full_width;
		size_t name_width;
		uint16_t count_in_section = 0;
		uint16_t asleep_in_section = 0;

		line_buffer[0] = '\0';

		if (list->entries[index].count[section] == 0)
			continue;

		/* Only display directions for the case of a single monster. */
		if (list->entries[index].count[section] == 1) {
			const char *direction1 =
				(list->entries[index].dy[section] <= 0)	? "N" : "S";
			const char *direction2 =
				(list->entries[index].dx[section] <= 0) ? "W" : "E";
			strnfmt(location, sizeof(location), " %d %s %d %s",
					abs(list->entries[index].dy[section]), direction1,
					abs(list->entries[index].dx[section]), direction2);
		}

		/* Get width available for monster name and sleep tag: 2 for char and
		 * space; location includes padding; last -1 for some reason? */
		full_width = max_width - 2 - utf8_strlen(location) - 1;

		asleep_in_section = list->entries[index].asleep[section];
		count_in_section = list->entries[index].count[section];

		if (asleep_in_section > 0 && count_in_section > 1)
			strnfmt(asleep, sizeof(asleep), " (%d asleep)", asleep_in_section);
		else if (asleep_in_section == 1 && count_in_section == 1)
			strnfmt(asleep, sizeof(asleep), " (asleep)");

		/* Clip the monster name to fit, and append the sleep tag. */
		name_width = MIN(full_width - utf8_strlen(asleep), sizeof(line_buffer));
		get_mon_name(line_buffer, sizeof(line_buffer),
					 list->entries[index].race,
					 list->entries[index].count[section]);
		utf8_clipto(line_buffer, name_width);
		my_strcat(line_buffer, asleep, sizeof(line_buffer));

		/* Calculate the width of the line for dynamic sizing; use a fixed max
		 * width for location and monster char. */
		max_line_length = MAX(max_line_length,
							  utf8_strlen(line_buffer) + 12 + 2);

		/* textblock_append_pict will safely add the monster symbol,
		 * regardless of ASCII/graphics mode. */
		if (tb != NULL && tile_width == 1 && tile_height == 1) {
			textblock_append_pict(tb, list->entries[index].attr, monster_x_char[list->entries[index].race->ridx]);
			textblock_append(tb, " ");
		}

		/* Add the left-aligned and padded monster name which will align the
		 * location to the right. */
		if (tb != NULL) {
			/* Hack - Because monster race strings are UTF8, we have to add
			 * additional padding for any raw bytes that might be consolidated
			 * into one displayed character. */
			full_width += strlen(line_buffer) - utf8_strlen(line_buffer);
			line_attr = monster_list_entry_line_color(&list->entries[index]);
			textblock_append_c(tb, line_attr, "%-*s%s\n",
				(int) full_width, line_buffer, location);
		}

		line_count++;
	}

	/* Don't worry about the "...others" line, since it's probably shorter
	 * than what's already printed. */
	if (max_width_result != NULL)
		*max_width_result = max_line_length;

	/* Bail since we don't have enough room to display the remaining count or
	 * since we've displayed them all. */
	if (lines_to_display <= 0 ||
		lines_to_display >= list->total_entries[section])
		return;

	/* Sum the remaining monsters; start where we left off in the above loop. */
	while (index < total) {
		remaining_monster_total += list->entries[index].count[section];
		index++;
	}

	if (tb != NULL)
		textblock_append(tb, "%6s...and %d others.\n", " ",
						 remaining_monster_total);
}

/**
 * Allow the standard list formatted to be bypassed for special cases.
 *
 * Returning true will bypass any other formatteding in
 * monster_list_format_textblock().
 *
 * \param list is the monster list to format.
 * \param tb is the textblock to produce or NULL if only the dimensions need to
 * be calculated.
 * \param max_lines is the maximum number of lines that can be displayed.
 * \param max_width is the maximum line width that can be displayed.
 * \param max_height_result is returned with the number of lines needed to
 * format the list without truncation.
 * \param max_width_result is returned with the width needed to format the list
 * without truncation.
 * \return true if further formatting should be bypassed.
 */
static bool monster_list_format_special(const monster_list_t *list, textblock *tb, int max_lines, int max_width, size_t *max_height_result, size_t *max_width_result)
{
	if (player->timed[TMD_IMAGE] > 0) {
		/* Hack - message needs newline to calculate width properly. */
		const char *message = "Your hallucinations are too wild to see things clearly.\n";

		if (max_height_result != NULL)
			*max_height_result = 1;

		if (max_width_result != NULL)
			*max_width_result = strlen(message);

		if (tb != NULL)
			textblock_append_c(tb, COLOUR_ORANGE, "%s", message);

		return true;
	}

	return false;
}

/**
 * Format the entire monster list with the given parameters.
 *
 * This function can be used to calculate the preferred dimensions for the list
 * by passing in a NULL textblock. The LOS section of the list will always be
 * shown, while the other section will be added conditionally. Also, this
 * function calls monster_list_format_special() first; if that function returns
 * true, it will bypass normal list formatting.
 *
 * \param list is the monster list to format.
 * \param tb is the textblock to produce or NULL if only the dimensions need to
 * be calculated.
 * \param max_lines is the maximum number of lines that can be displayed.
 * \param max_width is the maximum line width that can be displayed.
 * \param max_height_result is returned with the number of lines needed to
 * format the list without truncation.
 * \param max_width_result is returned with the width needed to format the list
 * without truncation.
 */
static void monster_list_format_textblock(const monster_list_t *list, textblock *tb, int max_lines, int max_width, size_t *max_height_result, size_t *max_width_result)
{
	int header_lines = 1;
	int lines_remaining;
	int los_lines_to_display;
	int esp_lines_to_display;
	size_t max_los_line = 0;
	size_t max_esp_line = 0;

	if (list == NULL || list->entries == NULL)
		return;

	if (monster_list_format_special(list, tb, max_lines, max_width,
									max_height_result, max_width_result))
		return;

	los_lines_to_display = list->total_entries[MONSTER_LIST_SECTION_LOS];
	esp_lines_to_display = list->total_entries[MONSTER_LIST_SECTION_ESP];

	if (list->total_entries[MONSTER_LIST_SECTION_ESP] > 0)
		header_lines += 2;

	if (max_height_result != NULL)
		*max_height_result = header_lines + los_lines_to_display +
			esp_lines_to_display;

	lines_remaining = max_lines - header_lines -
		list->total_entries[MONSTER_LIST_SECTION_LOS];

	/* Remove ESP lines as needed. */
	if (lines_remaining < list->total_entries[MONSTER_LIST_SECTION_ESP])
		esp_lines_to_display = MAX(lines_remaining - 1, 0);

	/* If we don't even have enough room for the ESP header, start removing
	 * LOS lines, leaving one for the "...others". */
	if (lines_remaining < 0)
		los_lines_to_display = list->total_entries[MONSTER_LIST_SECTION_LOS] -
			abs(lines_remaining) - 1;

	/* Display only headers if we don't have enough space. */
	if (header_lines >= max_lines) {
		los_lines_to_display = 0;
		esp_lines_to_display = 0;
	}

	monster_list_format_section(list, tb, MONSTER_LIST_SECTION_LOS,
								los_lines_to_display, max_width,
								"You can see", false, &max_los_line);

	if (list->total_entries[MONSTER_LIST_SECTION_ESP] > 0) {
		bool show_others = list->total_monsters[MONSTER_LIST_SECTION_LOS] > 0;

		if (tb != NULL)
			textblock_append(tb, "\n");

		monster_list_format_section(list, tb, MONSTER_LIST_SECTION_ESP,
									esp_lines_to_display, max_width,
									"You are aware of", show_others,
									&max_esp_line);
	}

	if (max_width_result != NULL)
		*max_width_result = MAX(max_los_line, max_esp_line);
}

/**
 * Get correct monster glyphs.
 */
static void monster_list_get_glyphs(monster_list_t *list)
{
	int i;

	/* Run through all monsters in the list. */
	for (i = 0; i < (int)list->entries_size; i++) {
		monster_list_entry_t *entry = &list->entries[i];
		if (entry->race == NULL)
			continue;

		/* If no monster attribute use the standard UI picture. */
		if (!entry->attr)
			entry->attr = monster_x_attr[entry->race->ridx];
	}
}

/**
 * Display the monster list statically. This will force the list to be
 * displayed to the provided dimensions. Contents will be adjusted accordingly.
 *
 * In order to support more efficient monster flicker animations, this function
 * uses a shared list object so that it's not constantly allocating and freeing
 * the list.
 *
 * \param height is the height of the list.
 * \param width is the width of the list.
 */
void monster_list_show_subwindow(int height, int width)
{
	textblock *tb;
	monster_list_t *list;
	int i;

	if (height < 1 || width < 1)
		return;

	tb = textblock_new();
	list = monster_list_shared_instance();

	/* Force an update if detected monsters */
	for (i = 1; i < cave_monster_max(cave); i++) {
		if (mflag_has(cave_monster(cave, i)->mflag, MFLAG_MARK)) {
			list->creation_turn = -1;
			break;
		}
	}

	monster_list_reset(list);
	monster_list_collect(list);
	monster_list_get_glyphs(list);
	monster_list_sort(list, monster_list_standard_compare);

	/* Draw the list to exactly fit the subwindow. */
	monster_list_format_textblock(list, tb, height, width, NULL, NULL);
	textui_textblock_place(tb, SCREEN_REGION, NULL);

	textblock_free(tb);
}

/**
 * Display the monster list interactively. This will dynamically size the list
 * for the best appearance. This should only be used in the main term.
 *
 * \param height is the height limit for the list.
 * \param width is the width limit for the list.
 */
void monster_list_show_interactive(int height, int width)
{
	textblock *tb;
	monster_list_t *list;
	size_t max_width = 0, max_height = 0;
	int safe_height, safe_width;
	region r;
	int sort_exp = 0;
	struct keypress ch;

	if (height < 1 || width < 1)
		return;

	// Repeat
	do {

		tb = textblock_new();
		list = monster_list_new();

		monster_list_collect(list);
		monster_list_get_glyphs(list);

		monster_list_sort(list, sort_exp ? monster_list_compare_exp : monster_list_standard_compare);

		/* Figure out optimal display rect. Large numbers are passed as the height
		 * and width limit so that we can calculate the maximum number of rows and
		 * columns to display the list nicely. We then adjust those values as
		 * needed to fit in the main term. Height is adjusted to account for the
		 * texblock prompt. The list is positioned on the right side of the term
		 * underneath the status line.
		 */
		monster_list_format_textblock(list, NULL, 1000, 1000, &max_height,
									  &max_width);
		safe_height = MIN(height - 3, (int) max_height + 3);
		safe_width = MIN(width - 40, (int) max_width);
		r.col = -safe_width;
		r.row = 1;
		r.width = safe_width;
		r.page_rows = safe_height;

		/*
		 * Actually draw the list. We pass in max_height to the format function so
		 * that all lines will be appended to the textblock. The textblock itself
		 * will handle fitting it into the region. However, we have to pass
		 * safe_width so that the format function will pad the lines properly so
		 * that the location string is aligned to the right edge of the list.
		 */
		monster_list_format_textblock(list, tb, (int) max_height, safe_width, NULL,
									  NULL);
		region_erase_bordered(&r);

		char buf[300];

		if (sort_exp) {
			my_strcpy(buf, "Press 'x' to turn OFF 'sort by exp'", sizeof(buf));
		}
		else {
			my_strcpy(buf, "Press 'x' to turn ON 'sort by exp'", sizeof(buf));
		}

		ch = textui_textblock_show(tb, r, buf);

		// Toggle sort
		sort_exp = !sort_exp;

		textblock_free(tb);
		monster_list_free(list);
	}
	while (ch.code == 'x');
}

/**
 * Force an update to the monster list subwindow.
 *
 * There are conditions that monster_list_reset() can't catch, so we set the
 * turn an invalid value to force the list to update.
 */
void monster_list_force_subwindow_update(void)
{
	monster_list_t *list = monster_list_shared_instance();
	list->creation_turn = -1;
}





/**
 * Diplomacy menu
 */


/**
 * Diplomacy menu data struct
 */
struct diplomacy_menu_data {
	int *commands;
	int num_commands;

	bool browse;
	bool show_description;

	int selected_command;
};

static void diplomacy_menu_display(struct menu *m, int oid, bool cursor,
		int row, int col, int wid)
{
	//plog("entering dmd");
	struct diplomacy_menu_data *d = menu_priv(m);

	int command = d->commands[oid];
	const char *name;
	int attr = d->selected_command == command ? COLOUR_L_BLUE : COLOUR_WHITE;

	switch (command) {
		case CMD_DIP_HIRE:
			name = "Hire";
			break;
		case CMD_DIP_GIFT:
			name = "Gift";
			break;
		case CMD_DIP_LEARN:
			name = "Learn";
			break;
		default:
			name = "ERROR";
	}


	c_prt(attr, name, row, col);
	//plog("done dmd");
}

static bool diplomacy_menu_handler(struct menu *m, const ui_event *e, int oid)
{
	//plog("entering gsmh");
	struct diplomacy_menu_data *d = menu_priv(m);

	if (e->type == EVT_SELECT) {
		d->selected_command = d->commands[oid];
		return d->browse ? true : false;
	}
	else if (e->type == EVT_KBRD) {
		if (e->key.code == '?') {
			d->show_description = !d->show_description;
		}
	}

	return false;
}

static void diplomacy_menu_browser(int oid, void *data, const region *loc)
{
	//struct diplomacy_menu_data *d = data;
	//int command = d->commands[oid];

	/*if (d->show_description) {
		// Redirect output to the screen 
		text_out_hook = text_out_to_screen;
		text_out_wrap = SCREEN_WID - 5;
		text_out_indent = loc->col - 1;
		text_out_pad = 1;

		Term_gotoxy(loc->col, loc->row + loc->page_rows);
		// Spell description 
		text_out("\n%s", spell->text);

		// To summarize average damage, count the damaging effects 
		int num_damaging = 0;
		int num_schools = 0;
		int i;

		ref_spell = spell;

		for (struct effect *e = spell->effect; e != NULL; e = effect_next(e)) {
			if (effect_damages(e)) {
				num_damaging++;
			}
		}
		// Now enumerate the effects' damage and type if not forgotten 
		if (num_damaging > 0 &&
				(player->player_spell_flags[spell_index] & PY_SPELL_WORKED) &&
				!(player->player_spell_flags[spell_index] & PY_SPELL_FORGOTTEN)) {
			dice_t *shared_dice = NULL;
			i = 0;

			text_out("  Inflicts an average of");
			for (struct effect *e = spell->effect; e != NULL; e = effect_next(e)) {
				if (e->index == EF_SET_VALUE) {
					shared_dice = e->dice;
				} else if (e->index == EF_CLEAR_VALUE) {
					shared_dice = NULL;
				}
				if (effect_damages(e)) {
					if (num_damaging > 2 && i > 0) {
						text_out(",");
					}
					if (num_damaging > 1 && i == num_damaging - 1) {
						text_out(" and");
					}
					text_out_c(COLOUR_L_GREEN, " %d", effect_avg_damage(e, shared_dice));
					const char *projection = effect_projection(e);
					if (strlen(projection) > 0) {
						text_out(" %s", projection);
					}
					i++;
				}
			}
			text_out(" damage.");
		}

		ref_spell = NULL;

		for (i = 0; i < MAX_SPELL_SCHOOLS; i++) {
			if (spell->school[i]) ++num_schools;
		}

		if (num_schools > 0) {
			int numremaining = num_schools;
			text_out("\nIt is a");
			for (i = 0; i < MAX_SPELL_SCHOOLS; i++) {
				if (spell->school[i]) {
					--numremaining;
					const char *name = school_idx_to_name(spell->school[i]);
					bool add_n = (numremaining + 1 == num_schools) && is_a_vowel(name[0]);
					text_out("%s %s", add_n ? "n" : "", name);
					if (num_schools > 2 && numremaining >= 1) {
						text_out(",");
					}
					if (numremaining == 1) {
						text_out(" and");
					}
				}
			}
			
			text_out(" spell.");
		}

		else {
			text_out("\nIt is an universal spell.");
		}
		
		text_out("\n\n");

		// XXX 
		text_out_pad = 0;
		text_out_indent = 0;
	}*/
}

static const menu_iter diplomacy_menu_iter = {
	NULL,	/* get_tag = NULL, just use lowercase selections */
	NULL,
	diplomacy_menu_display,
	diplomacy_menu_handler,
	NULL	/* no resize hook */
};


static struct menu *diplomacy_menu_new(struct player *p, struct monster *mon,
		bool show_description)
{
	struct menu *m = menu_new(MN_SKIN_SCROLL, &diplomacy_menu_iter);
	struct diplomacy_menu_data *d = mem_alloc(sizeof *d);
	size_t width = MAX(0, MIN(Term->wid - 15, 80));

	region loc = { 0 - width, 1, width, -99 };

	/* comands are static */
	d->num_commands = 0;
	d->commands = mem_zalloc(2 * sizeof(*d->commands));

	if (mon->reaction >= MON_REACT_FRIENDLY) {
		d->commands[d->num_commands] = CMD_DIP_HIRE;
		++d->num_commands;
	}
	if (distance(mon->grid, player->grid) <= 2) {
		d->commands[d->num_commands] = CMD_DIP_GIFT;
		++d->num_commands;
	}
	if (mon->reaction >= MON_REACT_ALLY && player_can_learn_from_monster(p, mon)) {
		d->commands[d->num_commands] = CMD_DIP_LEARN;
		++d->num_commands;
	}

	// no legitimate commands
	if (d->num_commands <= 0) {
		mem_free(d->commands);
		mem_free(d);
		menu_free(m);
		return NULL;
	}

	/* Copy across private data */
	d->selected_command = -1;
	d->browse = false;
	d->show_description = show_description;

	menu_setpriv(m, d->num_commands, d);

	/* Set flags */
	m->header = "Action";
	m->flags = MN_CASELESS_TAGS;
	m->selections = all_letters_nohjkl;
	m->browse_hook = diplomacy_menu_browser;
	m->cmd_keys = "?";

	/* Set size */
	loc.page_rows = d->num_commands + 1;
	menu_layout(m, &loc);

	//plog("done gsmn");
	return m;
}

static void diplomacy_menu_destroy(struct menu *m)
{
	struct diplomacy_menu_data *d = menu_priv(m);
	mem_free(d->commands);
	mem_free(d);
	menu_free(m);
}

static int diplomacy_menu_select(struct menu *m)
{
	//plog("entering gsms");
	struct diplomacy_menu_data *d = menu_priv(m);
	char buf[80];

	screen_save();
	region_erase_bordered(&m->active);

	/* Format, capitalise and display */
	strnfmt(buf, sizeof buf, "Take which action?");
	my_strcap(buf);
	prt(buf, 0, 0);

	menu_select(m, 0, true);
	screen_load();

	//plog("done gsms");
	return d->selected_command;
}

int textui_do_diplomacy(struct player *p, struct monster *mon, const char *error)
{
	struct menu *m;

	//handle_stuff(p);

	m = diplomacy_menu_new(p, mon, false);
	if (m) {
		int command = diplomacy_menu_select(m);
		diplomacy_menu_destroy(m);
		return command;
	} else if (error) {
		msg("%s", error);
	}

	return -1;
}