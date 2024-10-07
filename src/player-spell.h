/**
 * \file player-spell.h
 * \brief Spell and prayer casting/praying
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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

void player_spells_init(struct player *p);
void player_spells_free(struct player *p);
struct magic_realm *class_magic_realms(const struct player_class *c,
									   int *count);
const struct class_book *object_kind_to_book(const struct object_kind *kind);
const struct class_book *player_object_to_book(const struct player *p,
	const struct object *obj);
const struct class_spell *spell_by_index(const struct player *p, int index);
int spell_collect_from_book(const struct player *p, const struct object *obj,
	int **spells);
int spell_book_count_spells(const struct player *p, const struct object *obj,
	bool (*tester)(const struct player *p, int spell_index));
int caster_level_bonus(const struct player *p, const struct class_spell *spell);
bool spell_okay_list(const struct player *p,
	bool (*spell_test)(const struct player *p, int spell_index),
	const int spells[], int n_spells);
bool spell_okay_to_cast(const struct player *p, int spell_index);
bool spell_okay_to_study(const struct player *p, int spell_index);
bool spell_okay_to_browse(const struct player *p, int spell_index);
int16_t spell_chance(int spell_index);
void spell_learn(int spell_index);
bool spell_cast(int spell_index, int dir, struct command *cmd);
bool gener_spell_cast(int spell_index, int dir, struct command *cmd);

int school_find_idx(const char *name);
const char *school_idx_to_name(int idx);

extern void get_spell_info(int index, char *buf, size_t len);
//extern bool cast_spell(int tval, int index, int dir);
extern bool spell_needs_aim(int spell_index);
extern bool innate_needs_aim(int innate_index);
extern bool gener_spell_needs_aim(const struct player_spell *spell);

int innate_spell_mana(const struct monster_race *mon);

int gener_spell_power(const struct player *p, const struct player_spell *s);
void gener_spell_learn(struct player *p, const struct player_spell *s);
struct player_spell *player_spell_lookup(int index);
int player_spell_mana(const struct player_spell *ps);
int player_spell_fail(const struct player_spell *ps);
void get_player_spell_info(int spell_index, char *p, size_t len);

struct magic_realm *get_player_realm(const struct player *p);

