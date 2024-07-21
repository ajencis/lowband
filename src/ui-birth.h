/**
 * \file ui-birth.h
 * \brief Text-based user interface for character creation
 *
 * Copyright (c) 1987 - 2015 Angband contributors
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

#ifndef INCLUDED_UI_BIRTH_H
#define INCLUDED_UI_BIRTH_H


typedef struct power_name power_name;
struct power_name {
	int ppval;
	const char *name;
};

void ui_init_birthstate_handlers(void);
int textui_do_birth(void);

//phantom
extern bool arg_force_name;

#endif
