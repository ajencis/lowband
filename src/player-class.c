/**
 * \file player-class.c
 * \brief Player classes
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


#include "player.h"
#include "z-form.h"
#include "z-util.h"

struct player_class *player_id2class(guid id)
{
	struct player_class *c;
	for (c = classes; c; c = c->next) {
		if (guid_eq(c->cidx, id)) {
			break;
		}
	}
	return c;
}

bool any_class_has_flag(const struct player *p, int flag)
{
	int i;

	for (i = 0; p->classes[i] && i < MAX_PLAYER_CLASSES; ++i) {
		if (pf_has(p->classes[i]->flags, flag)) {
			return true;
		}
	}

	return false;
}

static const char *class_title_by_level(const struct player_class *c, int level)
{
	int index = level / 5 - 1;

	index = MAX(0, MIN(9, index));

	return c->title[index];
}

size_t class_name(const struct player *p, char *buf, size_t bufsize)
{
	int i;
	size_t result = 0;

	assert(p->classes[0]);

	result = strnfmt(buf, bufsize, "%s", p->classes[0]->name);

	for (i = 1; p->classes[i] && i < MAX_PLAYER_CLASSES; ++i) {
		result = my_strcat(buf, format(" | %s", p->classes[i]->name), bufsize);
	}

	return result;
}

size_t class_title(const struct player *p, char *buf, size_t bufsize)
{
	int i;
	size_t result = 0;

	assert(p->classes[0]);

	result = strnfmt(buf, bufsize, "%s", class_title_by_level(p->classes[0], p->lev));

	for (i = 1; p->classes[i] && i < MAX_PLAYER_CLASSES; ++i) {
		result = my_strcat(buf, format(" | %s", class_title_by_level(p->classes[i], p->lev)), bufsize);
	}

	return result;
}


static int class_sort(const void *a, const void *b)
{
	const struct player_class *c1 = a, *c2 = b;

	if (!c1 && !c2) {
		return 0;
	}
	if (!c1) {
		return 1;
	}
	if (!c2) {
		return -1;
	}

	return my_stricmp(c1->name, c2->name);
}

static bool player_can_add_class(const struct player *p, int cidx)
{
	int i;

	for (i = 0; i < MAX_PLAYER_CLASSES && p->classes[i]; ++i) {
		if (p->classes[i]->cidx == (unsigned)cidx) {

			return false;
		}
	}

	return i < MAX_PLAYER_CLASSES;
}

static void player_insert_class(struct player *p, int cidx, int ind)
{
	int i;

	for (i = MAX_PLAYER_CLASSES - 1; i > ind; --i) {
		p->classes[i] = p->classes[i - 1];
	}

	p->classes[ind] = player_id2class(cidx);
}

bool player_add_class(struct player *p, int cidx)
{
	int i;
	const struct player_class *new = player_id2class(cidx);

	if (!player_can_add_class(p, cidx)) {
		return false;
	}

	for (i = 0; i < MAX_PLAYER_CLASSES; ++i) {
		if (!p->classes[i] || (class_sort(p->classes[i], new) > 0)) {
			player_insert_class(p, cidx, i);

			return true;
		} 
	}

	return false;
}

bool player_remove_class(struct player *p, int cidx)
{
	int i, j;

	for (i = 0; i < MAX_PLAYER_CLASSES && p->classes[i]; ++i) {
		if (p->classes[i]->cidx == (unsigned)cidx) {
			break;
		}
	}

	if (i >= MAX_PLAYER_CLASSES || !p->classes[i]) {
		return false;
	}

	for (j = i; j < MAX_PLAYER_CLASSES - 1; ++j) {
		p->classes[j] = p->classes[j + 1];
	}

	p->classes[MAX_PLAYER_CLASSES - 1] = NULL;

	return true;
}

void player_set_class(struct player *p, int cidx)
{
	int i;

	p->classes[0] = player_id2class(cidx);

	for (i = 1; i < MAX_PLAYER_CLASSES; ++i) {
		p->classes[i] = NULL;
	}
}
