#include "angband.h"
#include "cave.h"
#include "game-world.h"
#include "mon-desc.h"
#include "player-util.h"
#include "project.h"



struct terrain_element_kind *t_elem_kind_by_idx(uint16_t idx)
{
	struct terrain_element_kind *t_kind;

	for (t_kind = te_info; t_kind; t_kind = t_kind->next) {
		if (t_kind->idx == idx) return t_kind;
	}

	return NULL;
}



bool t_elem_is_los(const struct terrain_element_kind *kind)
{
	return tf_has(kind->flags, TF_LOS);
}

bool sq_any_t_elem_has_flag(const struct square *sq, int flag)
{
	const struct terrain_element *t_elem;

	for (t_elem = sq->t_elem; t_elem; t_elem = t_elem->next) {
		if (tf_has(t_elem->kind->flags, flag)) return true;
	}

	return false;
}

bool sq_all_t_elem_has_flag(const struct square *sq, int flag)
{
	const struct terrain_element *t_elem;

	for (t_elem = sq->t_elem; t_elem; t_elem = t_elem->next) {
		if (!tf_has(t_elem->kind->flags, flag)) return false;
	}

	return true;
}



void square_memorize_t_elem(struct chunk *c, struct loc grid)
{
	const struct terrain_element *t_elem;
	struct terrain_element *new;

	if (!player->cave) return;

	assert(c);
	assert(player->cave);
	assert(square_in_bounds(c, grid));
	assert(square_in_bounds(player->cave, grid));

	const struct square *sq = square(c, grid);
	struct square *sq_known = &player->cave->squares[grid.y][grid.x];

	if (c != cave) return;

	if (sq_known->t_elem) {
		terrain_elem_remove_all(player->cave, grid);
		assert(!player->cave->squares[grid.y][grid.x].t_elem);
	}

	for (t_elem = sq->t_elem; t_elem; t_elem = t_elem->next) {
		new = terrain_element_new(t_elem->timer, t_elem->kind->idx);
		new->next = player->cave->squares[grid.y][grid.x].t_elem;
		player->cave->squares[grid.y][grid.x].t_elem = new;
	}
}



struct terrain_element *terrain_element_new(int timer, uint16_t idx)
{
	struct terrain_element *new;
	struct terrain_element_kind *kind = t_elem_kind_by_idx(idx);

	if (!kind) return NULL;

	new = mem_zalloc(sizeof *new);

	new->timer = timer;
	new->kind = kind;
	new->next = NULL;

	return new;
}

void terrain_elem_free(struct terrain_element *to_free)
{
	mem_free(to_free);
}



bool terrain_element_add(struct chunk *c, struct loc grid, uint16_t idx, uint16_t timer)
{
	struct terrain_element *prev = NULL, *new = terrain_element_new(timer, idx);

	if ((!square_t_elem(c, grid)) || (square_t_elem(c, grid)->kind->idx < idx)) {
		new->next = square_t_elem(c, grid);
		c->squares[grid.y][grid.x].t_elem = new;
	}
	else {
		prev = square_t_elem(c, grid);
		while (prev->next && prev->next->kind->idx < idx) {
			prev = prev->next;
		}

		new->next = prev->next;
		prev->next = new;
	}

	return true;

	/*struct terrain_element *t_elem = terrain_element_new(timer, idx);
	struct terrain_element **list = &c->squares[grid.y][grid.x].t_elem;

	while ((*list) && ((*list)->kind->idx < idx)) {
		list = &((*list)->next);
	}

	if (*list) assert((*list)->kind->idx != idx);

	t_elem->next = *list;
	*list = t_elem;

	return true;*/
}

bool terrain_elem_remove(struct chunk *c, struct loc grid, uint16_t idx)
{
	struct terrain_element *t_elem, **prev;

	for (prev = &c->squares[grid.y][grid.x].t_elem, t_elem = *prev; t_elem; prev = &t_elem->next, t_elem = *prev) {
		if (t_elem->kind->idx == idx) {
			*prev = t_elem->next;
			terrain_elem_free(t_elem);
			return true;
		}
	}

	return false;
}

/**
 * Increases the duration of the terrain element by  change , or adds it if it did
 * not previously exist in the square
 * Returns  true  if a terrain element was added
 */
bool terrain_element_increase_dur(struct chunk *c, struct loc grid, uint16_t idx, uint16_t change)
{
	struct terrain_element *t_elem;

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		if (t_elem->kind->idx == idx) {
			change = MAX(0, MIN(change, UINT16_MAX - t_elem->timer));
			t_elem->timer += change;
			return false;
		}
	}

	return terrain_element_add(c, grid, idx, change);
}

/**
 * Reduces the duration of the terrain element by  change , and removes it if it would
 * reduce to 0 or lower.
 * Returns  true  if the terrain element was removed
 */
bool terrain_element_reduce_dur(struct chunk *c, struct loc grid, uint16_t idx, uint16_t change)
{
	struct terrain_element *t_elem;

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		if (t_elem->kind->idx == idx) {
			if (change >= t_elem->timer) {
				return terrain_elem_remove(c, grid, idx);
			}

			t_elem->timer -= change;
			return false;
		}
	}

	return false;
}

bool terrain_element_change_dur(struct chunk *c, struct loc grid, uint16_t idx, int change)
{
	if (change < 0) return terrain_element_reduce_dur(c, grid, idx, (unsigned)(-change));
	if (change > 0) return terrain_element_increase_dur(c, grid, idx, (unsigned)change);

	return false;
}

bool terrain_elem_remove_all(struct chunk *c, struct loc grid)
{
	struct terrain_element *curr, *next;
	bool did_something = false;

	for (curr = square_t_elem(c, grid); curr; curr = next) {
		next = curr->next;
		did_something = true;

		terrain_elem_free(curr);
	}

	c->squares[grid.y][grid.x].t_elem = NULL;

	return did_something;
}

int t_elem_timer(struct chunk *c, struct loc grid, uint16_t which)
{
	struct terrain_element *t_elem = square_t_elem(c, grid);

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		if (t_elem->kind->idx == which) return t_elem->timer;
	}

	return 0;
}




bool grid_is_danger(const struct monster *mon, struct chunk *c, struct loc grid)
{
	const struct terrain_element *t_elem;

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		if (!mon_proj_is_immune(mon, t_elem->kind->proj)) {
			return true;
		}
	}

	return false;
}

bool mon_in_t_elem_danger(const struct monster *mon, struct chunk *c)
{
	return grid_is_danger(mon, c, mon->grid);
}




static bool t_elem_spread_one(struct chunk *c, struct loc grid, int kind, int8_t **changes)
{
	uint16_t dir, basedelta, delta, lin_div = 12U;
	struct square *sq = &c->squares[grid.y][grid.x];
	struct terrain_element *t_elem = sq->t_elem;
	struct loc newgrid;
	bool did_something = false;

	while (t_elem && t_elem->kind->idx != kind) {
		t_elem = t_elem->next;
	}

	if (!t_elem) return did_something;
	assert(square_in_bounds_fully(c, grid));

	basedelta = t_elem->timer / lin_div;

	assert(lin_div >= 8);

	for (dir = 1; dir <= 9; ++dir) {
		delta = (uint16_t)randint0(basedelta);
		newgrid = loc_sum(grid, ddgrid[dir]);

		if (delta <= 0) continue;
		if (loc_eq(newgrid, grid)) continue;
		if (!square_in_bounds_fully(c, newgrid)) continue;
		if (!square_canputterrainelem(c, newgrid)) continue;

		delta = MIN(delta, MAX(0, changes[grid.y][grid.x] - INT8_MIN));
		delta = MIN(delta, MAX(0, INT8_MAX - changes[newgrid.y][newgrid.x]));

		changes[newgrid.y][newgrid.x] += delta;
		changes[grid.y][grid.x] -= delta;

		if (delta > 0) did_something = true;
	}

	return did_something;
}

void t_elem_spread(struct chunk *c)
{
	const struct terrain_element_kind *kind;
	int i;
	struct loc grid;

	int8_t **change = mem_zalloc(sizeof *change * c->height);

	for (i = 0; i < c->height; ++i) {
		change[i] = mem_zalloc(sizeof **change * c->width);
	}

	for (kind = te_info; kind; kind = kind->next) {
		if (!tf_has(kind->flags, TF_CLOUD)) continue;

		for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
			for (grid.y = 1; grid.y < c->height - 1; ++ grid.y) {
				t_elem_spread_one(c, grid, kind->idx, change);
			}
		}

		for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
			for (grid.y = 1; grid.y < c->height - 1; ++grid.y) {
				terrain_element_change_dur(c, grid, kind->idx, change[grid.y][grid.x]);

				change[grid.y][grid.x] = 0;
			}
		}
	}

	for (i = 0; i < c->height; ++i) {
		mem_free(change[i]);
	}

	mem_free(change);
}


/**
 * Sets a square on fire, or continues its burning
 * Returns the new amount of fire on the square
 */
int burn_square(struct chunk *c, struct loc grid, int power)
{
	int n_t_elem = 0, burn_amt, temp_power, curr_fire = 0;
	struct terrain_element *t_elem;
	const struct terrain_element_kind *t_elem_k;

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		if (t_elem->kind->idx == TE_FIRE) {
			curr_fire = t_elem->timer;
		} else {
			++n_t_elem;
		}
	}

	power = (2 * power + 2 * n_t_elem - 1) / (n_t_elem + 1);

	burn_amt = 0;

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		t_elem_k = t_elem->kind;
		if (tf_has(t_elem_k->flags, TF_BURN_FAST)) {
			temp_power = MAX(5, power * power / 3);
		}
		else if (tf_has(t_elem_k->flags, TF_BURN)) {
			temp_power = MIN(power * 2, 5);
		}
		else {
			temp_power = 0;
		}

		temp_power = MIN(temp_power, t_elem->timer);

		burn_amt += temp_power;
		t_elem->timer -= temp_power;
	}

	if (burn_amt > 0) {
		terrain_element_increase_dur(c, grid, TE_FIRE, (uint16_t)burn_amt);
	}
	else if (curr_fire > 0) {
		terrain_element_reduce_dur(c, grid, TE_FIRE, (curr_fire + 1) / 2);
	}

	return t_elem_timer(c, grid, TE_FIRE);
}

/*static int burn_square_default(struct chunk *c, struct loc grid)
{
	int amt = t_elem_timer(c, grid, TE_FIRE);

	if (amt <= 0) return t_elem_timer(c, grid, TE_FIRE);

	return burn_square(c, grid, TE_FIRE);
}*/



static void t_elem_effect_message(struct chunk *c, struct loc grid, struct terrain_element *t_elem)
{
	const struct projection *proj = &projections[t_elem->kind->proj];
	const struct monster *mon = square_monster(c, grid);

	assert(c);
	if (!t_elem) return;

	if (square_isplayer(c, grid) && !mon_proj_is_immune(&player->mon, proj->index)) {
		msg("You are surrounded by %s.", t_elem->kind->name);
		disturb(player);
	}

	else if (mon && mon->race && monster_is_in_view(mon) && !mon_proj_is_immune(mon, proj->index)) {
		char mon_name[80];

		monster_desc(mon_name, sizeof mon_name, mon, MDESC_CAPITAL);

		msg("%s is surrounded by %s.", mon_name, t_elem->kind->name);
	}
}

void t_elem_effects(struct chunk *c)
{
	struct loc grid;
	uint16_t flg = PROJECT_HIDE | PROJECT_JUMP | PROJECT_KILL | PROJECT_ITEM | PROJECT_GRID | PROJECT_PLAY;
	bool reduce_dur = !(turn % (turns_per_process_world * 10));
	//struct terrain_element_kind *kind;
	//bool vanish_messages[TE_MAX] = { 0 };

	for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
		for (grid.y = 1; grid.y < c->height - 1; ++grid.y) {
			struct terrain_element *t_elem;
			struct square *sq = &c->squares[grid.y][grid.x];

			for (t_elem = sq->t_elem; t_elem; t_elem = t_elem->next) {
				int dam;

				if (t_elem->kind->proj < 0) continue;

				dam = my_int_sqrt(t_elem->timer * 10);

				t_elem_effect_message(c, grid, t_elem);

				project(source_t_elem(t_elem), 0, grid, dam, t_elem->kind->proj, flg, 0, 0, NULL);

				if (reduce_dur && terrain_element_reduce_dur(c, grid, t_elem->kind->idx, 1)) {
					continue;
				}
			}
		}
	}
}



static bool feat_produce_t_elem(struct chunk *c, struct loc grid)
{
	const struct feature *feat = square_feat(c, grid);
	uint16_t i, amt, curr, increase;
	bool did_something;

	for (i = 0; i < TE_MAX; ++i) {
		amt = feat->t_elem[i];

		if (amt) {
			curr = t_elem_timer(c, grid, i);
			increase = amt - curr;

			terrain_element_increase_dur(c, grid, i, increase);

			did_something = did_something || increase > 0;

			if (square_isview(c, grid) && feat->t_elem_msg) {
				msg("The %s %s %s.", feat->name, feat->t_elem_msg, t_elem_kind_by_idx(i)->name);
			}
		}
	}

	return did_something;
}

bool cave_produce_t_elem(struct chunk *c)
{
    struct loc grid;
    bool did_something = false;

	int freq = 5;

	float x, x_freq;
	int y, y_freq;
	int trn = turn / turns_per_process_world;

	y_freq = my_int_sqrt(freq);
	x_freq = (float)freq / (float)y_freq;

	y = trn % y_freq;
	trn /= y_freq;
	x = trn - ((int)((float)trn / x_freq)) * x_freq;

	while (true) {
		while (x >= (float)c->width) {
			x -= (float)c->width;
			y += y_freq;
		}

		if (y >= c->height) break;

		grid = loc((int)x, y);
		if (feat_produce_t_elem(c, grid)) {
			did_something = true;
		}

		x += x_freq;
	}

	return did_something;

	/*int frequency = 5; // one in  frequency  grids produce every turn
	int start = (turn / turns_per_process_world) % frequency;

	for (grid.x = start, grid.y = 0; grid.y < c->height; grid.x += frequency, grid.y += (grid.x / c->width), grid.x = (grid.x % c->width)) {
		if (feat_produce_t_elem(c, grid)) {
			did_something = true;
		}
	}

	return did_something;

	int inc_fact = 4;
    int turn_fact = (turn / turns_per_process_world) % inc_fact;
    int y_start = (turn_fact % inc_fact) + 1;
    int x_start = ((turn_fact / inc_fact) % inc_fact) + 1;

	for (grid.x = turn_fact)

    for (grid.x = x_start; grid.x < c->width - 1; grid.x += inc_fact) {
        for (grid.y = y_start; grid.y < c->height - 1; grid.y += inc_fact) {
            if (feat_produce_t_elem(c, grid)) {
                did_something = true;
            }
        }
    }

    return did_something;*/
}
