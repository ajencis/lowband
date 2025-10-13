/**
 * \file z-type.h
 * \brief Support various data types.
 *
 * Copyright (c) 2007 Angband Developers
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

#ifndef INCLUDED_ZTYPE_H
#define INCLUDED_ZTYPE_H

#include "h-basic.h"

struct loc {
	int x;
	int y;
};

struct loc loc(int x, int y);
bool loc_eq(struct loc grid1, struct loc grid2);
bool loc_is_zero(struct loc grid);
struct loc loc_sum(struct loc grid1, struct loc grid2);
struct loc loc_diff(struct loc grid1, struct loc grid2);
struct loc rand_loc(struct loc grid, int x_spread, int y_spread);
struct loc loc_offset(struct loc grid, int dx, int dy);


/**
 * Defines a (value, name) pairing.  Variable names used are historical.
 */
typedef struct grouper grouper;
struct grouper {
	int tval;
	const char *name;
};

/**
 * A set of points that can be constructed to apply a set of changes to
 */
struct point_set {
	int n;
	int allocated;
	struct loc *pts;
};

struct point_set *point_set_new(int initial_size);
void point_set_dispose(struct point_set *ps);
void add_to_point_set(struct point_set *ps, struct loc grid);
int point_set_size(struct point_set *ps);
bool point_set_contains(struct point_set *ps, struct loc grid);
void add_to_point_set_no_dup(struct point_set *ps, struct loc grid);
void remove_from_point_set(struct point_set *ps, struct loc grid);
void clear_point_set(struct point_set *ps);


/**
 * L: A single array treated as a multidimensional one; the number of dimensions
 * is variable and the number ofd variable arguments must always match the number
 * of dimensions
 */
struct multidimensional_array;
typedef struct multidimensional_array md_array;

struct multidimensional_array *mda_new(int dimensions, ...);
void mda_free(struct multidimensional_array *array);
int mda_element_get(struct multidimensional_array *array, ...);
int mda_element_set(struct multidimensional_array *array, int new_val, ...);
int mda_element_add(struct multidimensional_array *array, int to_add, ...);

#endif /* !INCLUDED_ZTYPE_H */
