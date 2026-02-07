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
