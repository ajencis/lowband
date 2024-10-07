/**
 * \file ui-spell.c
 * \brief Spell UI handing
 *
 * Copyright (c) 2010 Andi Sidwell
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
#include "cmds.h"
#include "cmd-core.h"
#include "effects.h"
#include "effects-info.h"
#include "game-input.h"
#include "init.h"
#include "monster.h"
#include "mon-spell.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-util.h"
#include "ui-menu.h"
#include "ui-output.h"
#include "ui-spell.h"


/**
 * Spell menu data struct
 */
struct spell_menu_data {
	int *spells;
	int n_spells;

	bool browse;
	bool (*is_valid)(const struct player *p, int spell_index);
	bool show_description;

	int selected_spell;
};


/**
 * Is item oid valid?
 */
static int spell_menu_valid(struct menu *m, int oid)
{
	struct spell_menu_data *d = menu_priv(m);
	int *splls = d->spells;

	return d->is_valid(player, splls[oid]);
}

/**
 * Display a row of the spell menu
 */
static void spell_menu_display(struct menu *m, int oid, bool cursor,
		int row, int col, int wid)
{
	struct spell_menu_data *d = menu_priv(m);
	int spell_index = d->spells[oid];
	const struct class_spell *spell = spell_by_index(player, spell_index);

	char help[30];
	char out[80];

	int attr;
	const char *illegible = NULL;
	const char *comment = "";
	size_t u8len;

	if (!spell) return;

	if (spell->slevel >= 99) {
		illegible = "(illegible)";
		attr = COLOUR_L_DARK;
	} else if (player->spell_flags[spell_index] & PY_SPELL_FORGOTTEN) {
		comment = " forgotten";
		attr = COLOUR_YELLOW;
	} else if (player->spell_flags[spell_index] & PY_SPELL_LEARNED) {
		if (player->spell_flags[spell_index] & PY_SPELL_WORKED) {
			/* Get extra info */
			get_spell_info(spell_index, help, sizeof(help));
			comment = help;
			attr = COLOUR_WHITE;
			//if (player->state.powers[spell->school]) attr = COLOUR_L_WHITE;
		} else {
			comment = " untried";
			attr = COLOUR_L_GREEN;
		}
	} else if (spell->slevel <= (player->lev + caster_level_bonus(player, spell))) {
		comment = " unknown";
		attr = COLOUR_L_BLUE;
	} else {
		comment = " difficult";
		attr = COLOUR_RED;
	}

	/* Dump the spell --(-- */
	u8len = utf8_strlen(spell->name);
	if (u8len < 30) {
		strnfmt(out, sizeof(out), "%s%*s", spell->name,
			(int)(30 - u8len), " ");
	} else {
		char *name_copy = string_make(spell->name);

		if (u8len > 30) {
			utf8_clipto(name_copy, 30);
		}
		my_strcpy(out, name_copy, sizeof(out));
		string_free(name_copy);
	}
	my_strcat(out, format("%2d %4d %3d%%%s", spell->slevel, spell->smana,
		spell_chance(spell_index), comment), sizeof(out));
	c_prt(attr, illegible ? illegible : out, row, col);
}

/**
 * Handle an event on a menu row.
 */
static bool spell_menu_handler(struct menu *m, const ui_event *e, int oid)
{
	struct spell_menu_data *d = menu_priv(m);

	if (e->type == EVT_SELECT) {
		d->selected_spell = d->spells[oid];
		return d->browse ? true : false;
	}
	else if (e->type == EVT_KBRD) {
		if (e->key.code == '?') {
			d->show_description = !d->show_description;
		}
	}

	return false;
}

/**
 * Show spell long description when browsing
 */
static void spell_menu_browser(int oid, void *data, const region *loc)
{
	struct spell_menu_data *d = data;
	int spell_index = d->spells[oid];
	const struct class_spell *spell = spell_by_index(player, spell_index);

	if (d->show_description) {
		/* Redirect output to the screen */
		text_out_hook = text_out_to_screen;
		text_out_wrap = 0;
		text_out_indent = loc->col - 1;
		text_out_pad = 1;

		Term_gotoxy(loc->col, loc->row + loc->page_rows);
		/* Spell description */
		text_out("\n%s", spell->text);

		/* To summarize average damage, count the damaging effects */
		int num_damaging = 0;
		for (struct effect *e = spell->effect; e != NULL; e = effect_next(e)) {
			if (effect_damages(e)) {
				num_damaging++;
			}
		}
		/* Now enumerate the effects' damage and type if not forgotten */
		if (num_damaging > 0
			&& (player->spell_flags[spell_index] & PY_SPELL_WORKED)
			&& !(player->spell_flags[spell_index] & PY_SPELL_FORGOTTEN)) {
			dice_t *shared_dice = NULL;
			int i = 0;

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
		text_out("\n\n");

		/* XXX */
		text_out_pad = 0;
		text_out_indent = 0;
	}
}

static const menu_iter spell_menu_iter = {
	NULL,	/* get_tag = NULL, just use lowercase selections */
	spell_menu_valid,
	spell_menu_display,
	spell_menu_handler,
	NULL	/* no resize hook */
};

/**
 * Create and initialise a spell menu, given an object and a validity hook
 */
static struct menu *spell_menu_new(const struct object *obj,
		bool (*is_valid)(const struct player *p, int spell_index),
		bool show_description)
{
	struct menu *m = menu_new(MN_SKIN_SCROLL, &spell_menu_iter);
	struct spell_menu_data *d = mem_alloc(sizeof *d);
	size_t width = MAX(0, MIN(Term->wid - 15, 80));

	region loc = { 0 - width, 1, width, -99 };

	/* collect spells from object */
	d->n_spells = spell_collect_from_book(player, obj, &d->spells);
	if (d->n_spells == 0 || !spell_okay_list(player, is_valid, d->spells, d->n_spells)) {
		mem_free(m);
		mem_free(d->spells);
		mem_free(d);
		return NULL;
	}

	/* Copy across private data */
	d->is_valid = is_valid;
	d->selected_spell = -1;
	d->browse = false;
	d->show_description = show_description;

	menu_setpriv(m, d->n_spells, d);

	/* Set flags */
	m->header = "Name                             Lv Mana Fail Info";
	m->flags = MN_CASELESS_TAGS;
	m->selections = all_letters_nohjkl;
	m->browse_hook = spell_menu_browser;
	m->cmd_keys = "?";

	/* Set size */
	loc.page_rows = d->n_spells + 1;
	menu_layout(m, &loc);

	return m;
}

/**
 * Clean up a spell menu instance
 */
static void spell_menu_destroy(struct menu *m)
{
	struct spell_menu_data *d = menu_priv(m);
	mem_free(d->spells);
	mem_free(d);
	mem_free(m);
}

/**
 * Run the spell menu to select a spell.
 */
static int spell_menu_select(struct menu *m, const char *noun, const char *verb)
{
	struct spell_menu_data *d = menu_priv(m);
	char buf[80];

	screen_save();
	region_erase_bordered(&m->active);

	/* Format, capitalise and display */
	strnfmt(buf, sizeof buf, "%s which %s? ('?' to toggle description)",
			verb, noun);
	my_strcap(buf);
	prt(buf, 0, 0);

	menu_select(m, 0, true);
	screen_load();

	return d->selected_spell;
}

/**
 * Run the spell menu, without selections.
 */
static void spell_menu_browse(struct menu *m, const char *noun)
{
	struct spell_menu_data *d = menu_priv(m);

	screen_save();

	region_erase_bordered(&m->active);
	prt(format("Browsing %ss. ('?' to toggle description)", noun), 0, 0);

	d->browse = true;
	menu_select(m, 0, true);

	screen_load();
}

/**
 * Browse a given book.
 */
void textui_book_browse(const struct object *obj)
{
	struct menu *m;
	const char *noun = player_object_to_book(player, obj)->realm->spell_noun;

	m = spell_menu_new(obj, spell_okay_to_browse, true);
	if (m) {
		spell_menu_browse(m, noun);
		spell_menu_destroy(m);
	} else {
		msg("You cannot browse that.");
	}
}

/**
 * Browse the given book.
 */
void textui_spell_browse(void)
{
	struct object *obj;

	if (!get_item(&obj, "Browse which book? ",
				  "You have no books that you can read.",
				  CMD_BROWSE_SPELL, obj_can_browse,
				  (USE_INVEN | USE_FLOOR | IS_HARMLESS)))
		return;

	/* Track the object kind */
	track_object(player->upkeep, obj);
	handle_stuff(player);

	textui_book_browse(obj);
}

/**
 * Get a spell from specified book.
 */
int textui_get_spell_from_book(struct player *p, const char *verb,
	struct object *book, const char *error,
	bool (*spell_filter)(const struct player *p, int spell_index))
{
	const char *noun = player_object_to_book(p, book)->realm->spell_noun;
	struct menu *m;

	track_object(p->upkeep, book);
	handle_stuff(p);

	m = spell_menu_new(book, spell_filter, false);
	if (m) {
		int spell_index = spell_menu_select(m, noun, verb);
		spell_menu_destroy(m);
		return spell_index;
	} else if (error) {
		msg("%s", error);
	}

	return -1;
}

/**
 * Get a spell from the player.
 */
int textui_get_spell(struct player *p, const char *verb,
		item_tester book_filter, cmd_code cmd, const char *book_error,
		bool (*spell_filter)(const struct player *p, int spell_index),
		const char *spell_error, struct object **rtn_book)
{
	char prompt[1024];
	struct object *book;

	/* Create prompt */
	strnfmt(prompt, sizeof prompt, "%s which book?", verb);
	my_strcap(prompt);

	if (!get_item(&book, prompt, book_error,
				  cmd, book_filter, (USE_INVEN | USE_FLOOR)))
		return -1;

	if (rtn_book) {
		*rtn_book = book;
	}

	return textui_get_spell_from_book(p, verb, book, spell_error,
		spell_filter);
}







/**
 * Innate menu data struct
 */
struct innate_menu_data {
	int *innates;
	int n_innates;

	bool browse;
	bool (*is_valid)(const struct player *p, int spell_index);
	bool show_description;

	int selected_innate;
};

static int innate_menu_valid(struct menu *m, int oid)
{
	struct innate_menu_data *d = menu_priv(m);
	int *innates = d->innates;

	return d->is_valid(player, innates[oid]);
}

static void innate_menu_display(struct menu *m, int oid, bool cursor,
		int row, int col, int wid)
{
	struct innate_menu_data *d = menu_priv(m);
	int innate_index = d->innates[oid];
	const struct monster_spell *innate = monster_spell_by_index(innate_index);
	const struct monster_race *mrace = lookup_player_monster(player);

	char out[80];
	char name[80];
	get_mon_spell_name(name, sizeof(name), innate_index, mrace);
	int mana = innate_spell_mana(mrace);

	int attr = COLOUR_WHITE;
	size_t u8len;

	if (!innate) return;

	/* Dump the spell --(-- */
	u8len = utf8_strlen(name);
	if (u8len < 30) {
		strnfmt(out, sizeof(out), "%s%*s", name,
			(int)(30 - u8len), " ");
	} else {
		char *name_copy = string_make(name);

		if (u8len > 30) {
			utf8_clipto(name_copy, 30);
		}
		my_strcpy(out, name_copy, sizeof(out));
		string_free(name_copy);
	}
	my_strcat(out, format("%i", mana), sizeof(out));
	c_prt(attr, out, row, col);
}

static bool innate_menu_handler(struct menu *m, const ui_event *e, int oid)
{
	struct innate_menu_data *d = menu_priv(m);

	if (e->type == EVT_SELECT) {
		d->selected_innate = d->innates[oid];
		return d->browse ? true : false;
	}
	else if (e->type == EVT_KBRD) {
		if (e->key.code == '?') {
			d->show_description = !d->show_description;
		}
	}

	return false;
}

static void innate_menu_browser(int oid, void *data, const region *loc)
{
	/*struct innate_menu_data *d = data;
	int innate_index = d->innates[oid];
	const struct monster_spell *innate = monster_spell_by_index(innate_index);*/

	/*if (d->show_description) {
		// Redirect output to the screen 
		text_out_hook = text_out_to_screen;
		text_out_wrap = 0;
		text_out_indent = loc->col - 1;
		text_out_pad = 1;

		Term_gotoxy(loc->col, loc->row + loc->page_rows);
		// Spell description 
		text_out("\n%s", spell->text);

		// To summarize average damage, count the damaging effects 
		int num_damaging = 0;
		for (struct effect *e = spell->effect; e != NULL; e = effect_next(e)) {
			if (effect_damages(e)) {
				num_damaging++;
			}
		}
		// Now enumerate the effects' damage and type if not forgotten 
		if (num_damaging > 0
			&& (player->spell_flags[spell_index] & PY_SPELL_WORKED)
			&& !(player->spell_flags[spell_index] & PY_SPELL_FORGOTTEN)) {
			dice_t *shared_dice = NULL;
			int i = 0;

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
		
		text_out("\n\n");

		// XXX 
		text_out_pad = 0;
		text_out_indent = 0;
	}*/
}

static const menu_iter innate_menu_iter = {
	NULL,	/* get_tag = NULL, just use lowercase selections */
	innate_menu_valid,
	innate_menu_display,
	innate_menu_handler,
	NULL	/* no resize hook */
};

static struct menu *innate_menu_new(const struct monster_race *monr,
		bool (*is_valid)(const struct player *p, int innate_index),
		bool show_description)
{
	struct menu *m = menu_new(MN_SKIN_SCROLL, &innate_menu_iter);
	struct innate_menu_data *d = mem_alloc(sizeof *d);
	size_t width = MAX(0, MIN(Term->wid - 15, 80));

	int max_innates = 25;

	region loc = { 0 - width, 1, width, -99 };

	int i;

	/* collect spells from monster */
	d->n_innates = 0;
	d->innates = mem_zalloc(max_innates * sizeof(int));
	for (i = 0; i < RSF_MAX; i++) {
		if (!mon_spell_is_innate(i)) continue;
		if (!rsf_has(monr->spell_flags, i)) continue;
		d->innates[d->n_innates] = i;
		d->n_innates++;
		if (d->n_innates >= max_innates) break;
	}

	if (d->n_innates == 0) { //|| !innate_okay_list(player, is_valid, d->spells, d->n_spells)) {
		mem_free(m);
		mem_free(d->innates);
		mem_free(d);
		return NULL;
	}

	/* Copy across private data */
	d->is_valid = is_valid;
	d->selected_innate = -1;
	d->browse = false;
	d->show_description = show_description;

	menu_setpriv(m, d->n_innates, d);

	/* Set flags */
	m->header = "Name                             Lv";
	m->flags = MN_CASELESS_TAGS;
	m->selections = all_letters_nohjkl;
	m->browse_hook = innate_menu_browser;
	m->cmd_keys = "?";

	/* Set size */
	loc.page_rows = d->n_innates + 1;
	menu_layout(m, &loc);

	return m;
}

static void innate_menu_destroy(struct menu *m)
{
	struct innate_menu_data *d = menu_priv(m);
	mem_free(d->innates);
	mem_free(d);
	mem_free(m);
}

static int innate_menu_select(struct menu *m)
{
	struct innate_menu_data *d = menu_priv(m);
	char buf[80];

	screen_save();
	region_erase_bordered(&m->active);

	/* Format, capitalise and display */
	strnfmt(buf, sizeof buf, "Evoke which innate power?");
	my_strcap(buf);
	prt(buf, 0, 0);

	menu_select(m, 0, true);
	screen_load();

	return d->selected_innate;
}

int textui_get_innate(struct player *p,
	struct monster_race *monr, const char *error,
	bool (*innate_filter)(const struct player *p, int innate_index))
{
	struct menu *m;

	handle_stuff(p);

	m = innate_menu_new(monr, innate_filter, false);
	if (m) {
		int innate_index = innate_menu_select(m);
		innate_menu_destroy(m);
		return innate_index;
	} else if (error) {
		msg("%s", error);
	}

	return -1;
}







/**
 * Innate menu data struct
 */
struct gener_spell_menu_data {
	const struct player_spell **spells;
	int n_splls;

	bool browse;
	int (*is_valid)(const struct player *p, int spell_index);
	bool show_description;

	int selected_spell;
};

static int gener_spell_menu_valid(struct menu *m, int oid)
{
	//plog("entering gsmv");
	struct gener_spell_menu_data *d = menu_priv(m);

	return d->is_valid(player, d->spells[oid]->sidx);
}

static void gener_spell_menu_display(struct menu *m, int oid, bool cursor,
		int row, int col, int wid)
{
	//plog("entering gsmd");
	struct gener_spell_menu_data *d = menu_priv(m);
	const struct player_spell *spell = d->spells[oid];
	int spell_index = spell->sidx;

	assert(spell);

	if (!spell) return;

	char out[80];
	char name[80];
	const char *comment = "";
	char desc[30];
	my_strcpy(name, spell->name, sizeof(name));
	int level = gener_spell_power(player, spell);
	int mana = player_spell_mana(spell);
	int fail = player_spell_fail(spell);
	fail = MIN(100, fail);

	int attr = COLOUR_WHITE;
	size_t u8len;

	if (player->player_spell_flags[spell_index] & PY_SPELL_FORGOTTEN) {
		comment = " forgotten";
		attr = COLOUR_YELLOW;
	} else if (player->player_spell_flags[spell_index] & PY_SPELL_LEARNED) {
		if (player->player_spell_flags[spell_index] & PY_SPELL_WORKED) {
			/* Get extra info */
			get_player_spell_info(spell_index, desc, sizeof(desc));
			comment = desc;
			attr = COLOUR_WHITE;
		} else {
			comment = " untried";
			attr = COLOUR_L_GREEN;
		}
	} else if (level > 0) {
		comment = " unknown";
		attr = COLOUR_L_BLUE;
	} else {
		comment = " difficult";
		attr = COLOUR_RED;
	}

	/* Dump the spell --(-- */
	u8len = utf8_strlen(name);
	if (u8len < 30) {
		strnfmt(out, sizeof(out), "%s%*s", name,
			(int)(30 - u8len), " ");
	} else {
		char *name_copy = string_make(name);

		if (u8len > 30) {
			utf8_clipto(name_copy, 30);
		}
		my_strcpy(out, name_copy, sizeof(out));
		string_free(name_copy);
	}

	my_strcat(out, format("%3i %4i %3i%%%s", level, mana, fail, comment), sizeof(out));
	c_prt(attr, out, row, col);
	//plog("done gsmd");
}

static bool gener_spell_menu_handler(struct menu *m, const ui_event *e, int oid)
{
	//plog("entering gsmh");
	struct gener_spell_menu_data *d = menu_priv(m);

	if (e->type == EVT_SELECT) {
		d->selected_spell = d->spells[oid]->sidx;
		return d->browse ? true : false;
	}
	else if (e->type == EVT_KBRD) {
		if (e->key.code == '?') {
			d->show_description = !d->show_description;
		}
	}

	return false;
}

static void gener_spell_menu_browser(int oid, void *data, const region *loc)
{
	struct gener_spell_menu_data *d = data;
	const struct player_spell *spell = d->spells[oid];
	int spell_index = spell->sidx;

	if (d->show_description) {
		// Redirect output to the screen 
		text_out_hook = text_out_to_screen;
		text_out_wrap = 0;
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
	}
}

static const menu_iter gener_spell_menu_iter = {
	NULL,	/* get_tag = NULL, just use lowercase selections */
	gener_spell_menu_valid,
	gener_spell_menu_display,
	gener_spell_menu_handler,
	NULL	/* no resize hook */
};

static int spell_compare_name(const void *a, const void *b)
{
	assert(a && b);
	int i;
	const struct player_spell *ps1 = *((struct player_spell **)a);
	const struct player_spell *ps2 = *((struct player_spell **)b);
	assert(ps1 && ps2);

	const char *name1 = ps1->name;
	const char *name2 = ps2->name;

	i = 0;
	while (name1[i] || name2[i]) {
		if (!name1[i]) return -1;
		if (!name2[i]) return 1;
		if (name1[i] < name2[i]) return -1;
		if (name2[i] < name1[i]) return 1;
	}

	return 0;
}

static int spell_compare_level(const void *a, const void *b)
{
	assert(a && b);
	const struct player_spell *ps1 = *((struct player_spell **)a);
	const struct player_spell *ps2 = *((struct player_spell **)b);
	assert(ps1 && ps2);

	int lev1 = gener_spell_power(player, ps1);
	int lev2 = gener_spell_power(player, ps2);

	if (lev1 > lev2) return -1;
	if (lev2 > lev1) return 1;

	return 0;
}

static int spell_compare_standard(const void *a, const void *b)
{
	int result = spell_compare_level(a, b);
	return result ? result : spell_compare_name(a, b);
}

static struct menu *gener_spell_menu_new(struct player *p, 
		int (*is_valid)(const struct player *p, int spell_index),
		bool show_description)
{
	//plog("entering gsmn");
	struct menu *m = menu_new(MN_SKIN_SCROLL, &gener_spell_menu_iter);
	struct gener_spell_menu_data *d = mem_alloc(sizeof *d);
	size_t width = MAX(0, MIN(Term->wid - 15, 80));
	bool repeat;
	int i;

	int max_splls = 25;
	struct object *spellbook;

	region loc = { 0 - width, 1, width, -99 };

	/* collect spells from books */
	d->n_splls = 0;
	d->spells = mem_zalloc(max_splls * sizeof(struct player_spell *));
	for (spellbook = p->gear; spellbook && d->n_splls < max_splls; spellbook = spellbook->next) {
		const struct player_spell *spell = spellbook->kind->spell;
		repeat = false;
		if (spell) {
			for (i = 0; i < d->n_splls; i++) {
				if (spell->sidx == d->spells[i]->sidx) repeat = true;
			}
			if (!repeat && (is_valid(player, spell->sidx) != 2)) {
				d->spells[d->n_splls] = spell;
				d->n_splls++;
			}
		}
	}

	if (d->n_splls == 0) {
		mem_free(m);
		mem_free(d->spells);
		mem_free(d);
		return NULL;
	}

	sort(d->spells, d->n_splls, sizeof(d->spells[0]), spell_compare_standard);

	/* Copy across private data */
	d->is_valid = is_valid;
	d->selected_spell = -1;
	d->browse = false;
	d->show_description = show_description;

	menu_setpriv(m, d->n_splls, d);

	/* Set flags */
	m->header = "Name                             Lvl Mana Fail Info";
	m->flags = MN_CASELESS_TAGS;
	m->selections = all_letters_nohjkl;
	m->browse_hook = gener_spell_menu_browser;
	m->cmd_keys = "?";

	/* Set size */
	loc.page_rows = d->n_splls + 1;
	menu_layout(m, &loc);

	//plog("done gsmn");
	return m;
}

static void gener_spell_menu_destroy(struct menu *m)
{
	struct gener_spell_menu_data *d = menu_priv(m);
	mem_free(d->spells);
	mem_free(d);
	mem_free(m);
}

static int gener_spell_menu_select(struct menu *m)
{
	//plog("entering gsms");
	struct gener_spell_menu_data *d = menu_priv(m);
	char buf[80];

	screen_save();
	region_erase_bordered(&m->active);

	/* Format, capitalise and display */
	strnfmt(buf, sizeof buf, "Cast which spell?");
	my_strcap(buf);
	prt(buf, 0, 0);

	menu_select(m, 0, true);
	screen_load();

	//plog("done gsms");
	return d->selected_spell;
}

/**
 * Run the spell menu, without selections.
 */
static void gener_spell_menu_browse(struct menu *m)
{
	struct gener_spell_menu_data *d = menu_priv(m);

	screen_save();

	region_erase_bordered(&m->active);
	prt("Browsing spells. ('?' to toggle description)", 0, 0);

	d->browse = true;
	menu_select(m, 0, true);

	screen_load();
}

int textui_get_gener_spell(struct player *p, const char *error,
	int (*spell_filter)(const struct player *p, int spell_index))
{
	//plog("entering tggs");
	struct menu *m;

	handle_stuff(p);

	m = gener_spell_menu_new(p, spell_filter, false);
	if (m) {
		int spell_index = gener_spell_menu_select(m);
		gener_spell_menu_destroy(m);
		return spell_index;
	} else if (error) {
		msg("%s", error);
	}

	return -1;
}

static int dummy_gener_spell_filter(const struct player *p, int spell_index)
{
	return 1;
}

void textui_gener_spell_browse(void)
{
	struct menu *m;

	m = gener_spell_menu_new(player, dummy_gener_spell_filter, true);
	if (m) {
		gener_spell_menu_browse(m);
		gener_spell_menu_destroy(m);
	} else {
		msg("You have nothing to browse.");
	}
}
