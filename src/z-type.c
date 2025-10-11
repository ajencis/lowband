/**
 * \file z-type.c
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

#include "z-form.h"
#include "z-rand.h"
#include "z-type.h"
#include "z-virt.h"

struct loc loc(int x, int y) {
	struct loc p;
	p.x = x;
	p.y = y;
	return p;
}

/**
 * Determine if two grid locations are equal
 */
bool loc_eq(struct loc grid1, struct loc grid2)
{
	return (grid1.x == grid2.x) && (grid1.y == grid2.y);
}

/**
 * Determine if a grid location is the (0, 0) location
 */
bool loc_is_zero(struct loc grid)
{
	return loc_eq(grid, loc(0, 0));
}

/**
 * Sum two grid locations
 */
struct loc loc_sum(struct loc grid1, struct loc grid2)
{
	return loc(grid1.x + grid2.x, grid1.y + grid2.y);
}

/**
 * Take the difference of two grid locations
 */
struct loc loc_diff(struct loc grid1, struct loc grid2)
{
	return loc(grid1.x - grid2.x, grid1.y - grid2.y);
}

/**
 * Get a random location with the given x and y centres and spread 
 */
struct loc rand_loc(struct loc grid, int x_spread, int y_spread)
{
	return loc(rand_spread(grid.x, x_spread), rand_spread(grid.y, y_spread));
}

struct loc loc_offset(struct loc grid, int dx, int dy)
{
	return loc(grid.x + dx, grid.y + dy);
}

/**
 * Utility functions to work with point_sets
 */
struct point_set *point_set_new(int initial_size)
{
	struct point_set *ps = mem_alloc(sizeof(struct point_set));
	ps->n = 0;
	ps->allocated = MAX(initial_size, 1);
	ps->pts = mem_zalloc(sizeof(*(ps->pts)) * ps->allocated);
	return ps;
}

void point_set_dispose(struct point_set *ps)
{
	mem_free(ps->pts);
	mem_free(ps);
}

/**
 * Add the point to the given point set, making more space if there is
 * no more space left.
 */
void add_to_point_set(struct point_set *ps, struct loc grid)
{
	ps->pts[ps->n] = grid;
	ps->n++;
	if (ps->n >= ps->allocated) {
		ps->allocated *= 2;
		ps->pts = mem_realloc(ps->pts, sizeof(*(ps->pts)) * ps->allocated);
	}
}

int point_set_size(struct point_set *ps)
{
	return ps->n;
}

static int point_set_index(struct point_set *ps, struct loc grid)
{
	int i;
	for (i = 0; i < ps->n; ++i) {
		if (loc_eq(ps->pts[i], grid)) {
			return i;
		}
	}

	return -1;
}

int point_set_contains(struct point_set *ps, struct loc grid)
{
	return point_set_index(ps, grid) >= 0;
}

/**
 * L: adds to a point set only if the point set doesn't have that point yet
 */
void add_to_point_set_no_dup(struct point_set *ps, struct loc grid)
{
	if (!point_set_contains(ps, grid)) {
		add_to_point_set(ps, grid);
	}
}

/**
 * L: removes from a point set; can mess up ordering
 */
void remove_from_point_set(struct point_set *ps, struct loc grid)
{
	int ind = point_set_index(ps, grid);

	if (ind >= 0) {
		assert(ps->n > 0);
		assert(ind < ps->n);
		assert(ind < ps->allocated);
		assert(ps->n < ps->allocated);
		ps->pts[ind] = ps->pts[ps->n - 1];
		--ps->n;
	}
}

void clear_point_set(struct point_set *ps)
{
	ps->n = 0;
}


struct multidimensional_array {
	int n_dimensions;
	int *dimensions_size;
	int *array;
};

struct multidimensional_array *mda_new(int dimensions, ...)
{
	int *dimensions_size;
	int *array;
	int i, new_num;
	uint64_t factor = 1U;
	va_list ap;
	struct multidimensional_array *new_array;

	if (dimensions <= 0) return NULL;

	dimensions_size = mem_zalloc(sizeof *dimensions_size * dimensions);

	va_start(ap, dimensions);

	for (i = 0; i < dimensions; ++i) {
		new_num = va_arg(ap, int);
		dimensions_size[i] = new_num;

		assert(UINT64_MAX / (unsigned)new_num > factor);

		factor *= (unsigned)new_num;

		if (new_num <= 0) {
			mem_free(dimensions_size);
			return NULL;
		}
	}

	va_end(ap);

	array = mem_zalloc(factor * sizeof *array);

	new_array = mem_zalloc(sizeof *new_array);

	new_array->n_dimensions = dimensions;
	new_array->dimensions_size = dimensions_size;
	new_array->array = array;

	return new_array;
}

void mda_free(struct multidimensional_array *array)
{
	mem_free(array->dimensions_size);
	mem_free(array->array);
	mem_free(array);
}

static int *vmda_element(struct multidimensional_array *array, va_list args)
{
	int i, curr_va, curr_size;
	size_t index = 0U;

	for (i = 0; i < array->n_dimensions; ++i) {
		curr_va = va_arg(args, int);
		curr_size = array->dimensions_size[i];

		if (curr_va < 0 || curr_va >= curr_size) {
			quit_fmt("Error: array dimension %i of length %i accessed at index %i!", i, curr_size, curr_va);
			_wassert(_CRT_WIDE("curr_va >= 0 && curr_va < curr_size"), _CRT_WIDE(__FILE__), (unsigned)(__LINE__));
		}

		index *= (unsigned)curr_size;
		index += (unsigned)curr_va;
	}

	return &array->array[index];
}

int mda_element_get(struct multidimensional_array *array, ...)
{
	int *result;
	va_list va;

	va_start(va, array);

	result = vmda_element(array, va);

	va_end(va);

	return *result;
}

int mda_element_set(struct multidimensional_array *array, int new_val, ...)
{
	int *result;
	va_list va;

	va_start(va, new_val);

	result = vmda_element(array, va);

	va_end(va);

	*result = new_val;

	return *result;
}

int mda_element_add(struct multidimensional_array *array, int to_add, ...)
{
	int *result;
	va_list va;

	va_start(va, to_add);

	result = vmda_element(array, va);

	va_end(va);

	*result += to_add;

	return *result;
}
