#include "angband.h"
#include "cave.h"
#include "game-world.h"
#include "init.h"
#include "mon-desc.h"
#include "player-calcs.h"
#include "player-util.h"
#include "project.h"



static struct feature *feat_new(int fidx, int size)
{
	struct feature *new = mem_zalloc(sizeof *new);

	assert(fidx > FEAT_NONE && fidx < FEAT_MAX);

	if (size <= 0) return NULL;

	new->kind = &f_info[fidx];
	new->size = size;

	return new;
}

static void feat_free(struct feature *feat)
{
	mem_free(feat);
}

bool feat_add(struct feature **list, int fidx, int size)
{
	assert(list);
	assert(fidx > FEAT_NONE && fidx < FEAT_MAX);

	struct feature *new, *curr;

	if (!(*list) || (*list)->kind->fidx > fidx) {
		// new feat goes first
		curr = NULL;
	}
	else if ((*list)->kind->fidx == fidx) {
		// already have that feat there
		return false;
	}
	else {
		// put it somewhere in the middle
		for (curr = *list; curr && curr->next; curr = curr->next) {
			if (curr->next->kind->fidx == fidx) {
				return false;
			}
			else if (curr->next->kind->fidx > fidx) {
				break;
			}
		}
	}

	new = feat_new(fidx, size);

	if (curr) {
		new->next = curr->next;
		curr->next = new;
	}
	else {
		new->next = *list;
		*list = new;
	}

	return true;
}

bool feat_remove(struct chunk *c, struct loc grid, int fidx)
{
	struct feature **prev = NULL, *to_del;
	struct square *sq = &c->squares[grid.y][grid.x];

	prev = &sq->feat;

	while (true) {
		if (!(*prev)) return false;
		if ((*prev)->kind->fidx > fidx) return false;
		if ((*prev)->kind->fidx == fidx) break;

		prev = &((*prev)->next);
	}

	to_del = *prev;

	*prev = to_del->next;

	feat_free(to_del);

	return true;
}
	
	
struct feature *square_feat(struct chunk *c, struct loc grid)
{
	return c->squares[grid.y][grid.x].feat;
}

struct feature *square_feat_by_type(struct chunk *c, struct loc grid, int fidx)
{
	struct feature *feat;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat->kind->fidx == fidx) {
			return feat;
		}
	}

	return NULL;
}

bool square_remove_feats_by_flag(struct chunk *c, struct loc grid, int flag)
{
	struct feature *feat = square_feat(c, grid);

	while (feat) {
		if (tf_has(feat->kind->flags, flag)) {
			feat_remove(c, grid, feat->kind->fidx);
			feat = square_feat(c, grid);
		}
		else if (feat->next && tf_has(feat->next->kind->flags, flag)) {
			feat_remove(c, grid, feat->next->kind->fidx);
		}
		else {
			feat = feat->next;
		}
	}

	return true;
}

bool square_change_feat(struct chunk *c, struct loc grid, int old, int new)
{
	struct feature *feat;

	assert(new > FEAT_NONE && new < FEAT_MAX);

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat->kind->fidx == old) {
			square_add_feat(c, grid, new, feat->size);
			feat_remove(c, grid, old);
			return true;
		}
	}

	return false;
}

bool square_has_feat(const struct chunk *c, struct loc grid, int fidx)
{
	const struct feature *feat;

	for (feat = c->squares[grid.y][grid.x].feat; feat; feat = feat->next) {
		if (feat->kind->fidx == fidx) {
			return true;
		}
	}

	return false;
}

static void square_set_feat_size(struct chunk *c, struct loc grid, int fidx, int size)
{
	struct feature *curr;
	struct square *sq = &c->squares[grid.y][grid.x];

	assert(fidx > FEAT_NONE && fidx < FEAT_MAX);

	for (curr = sq->feat; curr; curr = curr->next) {
		if (curr->kind->fidx == fidx) {
			curr->size = size;
			return;
		}
		else if (!curr->next || curr->next->kind->fidx > fidx) {
			break;
		}
	}

	square_add_feat(c, grid, fidx, size);
}

/**
 * a feat's believed version unless the feat mimics another feat and the actual feat isn't known
 * by the player to be in the square in question
 * eg if a secret door mimics a wall, a secret door feat's believed feat is a secret door if the
 * player believes a secret door to be in that square and a wall otherwise
 */
static int feat_believed(struct player *p, struct loc grid, int feat)
{
	struct feature_kind *kind;

	assert(feat > FEAT_NONE && feat < FEAT_MAX);
	kind = &f_info[feat];

	if (!kind->mimic) return kind->fidx;
	if (square_has_feat(p->cave, grid, kind->fidx)) return kind->fidx;

	return kind->mimic->fidx;
}

/**
 * L: returns whether the feat is hidden to the player - ie the player believes the feat's
 * mimic is in the square and doesn't know that the real feat is in the square
 */
bool feat_is_hidden(struct player *p, struct loc grid, int fidx)
{
	int mimic;
	bool is_hidden = false;
	struct feature *feat;

	if (!f_info[fidx].mimic) return false;

	mimic = f_info[fidx].mimic->fidx;

	for (feat = square_feat(p->cave, grid); feat; feat = feat->next) {
		if (feat->kind->fidx == mimic) {
			is_hidden = true;
		}
		else if (feat->kind->fidx == fidx) {
			return false;
		}
	}

	return is_hidden;
}

static void square_update_feat_memorization(const struct chunk *c, struct player *p, struct loc grid)
{
	struct feature *new = NULL;
	const struct feature *real;
	int fidx;

	assert(p);
	assert(c);
	assert(p->cave);
	assert(square_in_bounds((struct chunk *)c, grid));

	for (real = c->squares[grid.y][grid.x].feat; real; real = real->next) {
		fidx = feat_believed(p, grid, real->kind->fidx);

		assert(fidx > FEAT_NONE && fidx < FEAT_MAX);
		feat_add(&new, fidx, real->size);
	}

	square_set_feat(p->cave, grid, new);
}

static void square_memorize_feat_one(struct player *p, struct loc grid, const struct feature *feat, bool real)
{
	int to_memorize = feat->kind->fidx;

	if (feat->kind->mimic &&
			!real &&
			!square_has_feat(p->cave, grid, feat->kind->fidx)) {
		to_memorize = feat->kind->mimic->fidx;
	}

	to_memorize = real || !feat->kind->mimic ? feat->kind->fidx : feat->kind->mimic->fidx;

	square_set_feat_size(p->cave, grid, to_memorize, feat->size);
}

void square_memorize_feats(struct player *p, const struct chunk *c, struct loc grid)
{
	if (c != cave) return;

	assert(p);
	assert(c);
	assert(p->cave);

	square_update_feat_memorization(c, p, grid);
}

void square_forget_feats(struct player *p, struct loc grid)
{
	square_clear_feats(p->cave, grid);
}

void square_memorize_feat_real(struct player *p, const struct chunk *c, struct loc grid, const struct feature *feat)
{
	struct feature_kind *mimic = feat->kind->mimic;

	if (mimic && !square_has_feat(c, grid, mimic->fidx)) {
		feat_remove(p->cave, grid, mimic->fidx);
	}

	square_memorize_feat_one(p, grid, feat, true);
}

/**
 * True if the same feats are in both lists (lists are the same discarding size)
 */
bool feats_equal(const struct feature *feat1, const struct feature *feat2)
{
	const struct feature *test1, *test2;

	for (test1 = feat1, test2 = feat2; test1 && test2; test1 = test1->next, test2 = test2->next) {
		if (test1->kind->fidx != test2->kind->fidx) return false;
	}

	if (!test1 || !test2) {
		return false;
	}

	return true;
}



struct terrain_element_kind *t_elem_kind_by_idx(int idx)
{
	struct terrain_element_kind *t_kind;

	for (t_kind = te_info; t_kind; t_kind = t_kind->next) {
		if (t_kind->idx == idx) return t_kind;
	}

	return NULL;
}



struct terrain_element_level *t_elem_level(const struct terrain_element_kind *kind, int timer)
{
	struct terrain_element_level *lev;

	assert(kind);
	assert(timer > 0);

	for (lev = kind->levels; lev && lev->next; lev = lev->next) {
		if (lev->next->min_dur > timer) {
			break;
		}
	}
	
	assert(lev);

	return lev;
}



bool t_elem_has_flag(const struct terrain_element *t_elem, int flag)
{
	assert(t_elem);
	assert(flag > TF_NONE && flag < TF_MAX);
	return tf_has(t_elem_level(t_elem->kind, t_elem->timer)->flags, flag);
}

const char *t_elem_name(const struct terrain_element *t_elem)
{
	assert(t_elem);
	return t_elem_level(t_elem->kind, t_elem->timer)->name;
}

uint8_t t_elem_d_attr(const struct terrain_element *t_elem)
{
	assert(t_elem);
	return t_elem_level(t_elem->kind, t_elem->timer)->d_attr;
}

uint8_t t_elem_d_char(const struct terrain_element *t_elem)
{
	assert(t_elem);
	return t_elem_level(t_elem->kind, t_elem->timer)->d_char;
}

int t_elem_timeout(const struct terrain_element *t_elem)
{
	assert(t_elem);
	return t_elem_level(t_elem->kind, t_elem->timer)->timeout;
}

int t_elem_proj(const struct terrain_element *t_elem)
{
	assert(t_elem);
	return t_elem_level(t_elem->kind, t_elem->timer)->proj;
}

int t_elem_proj_range(const struct terrain_element *t_elem)
{
	assert(t_elem);
	return t_elem_level(t_elem->kind, t_elem->timer)->proj_range;
}


bool t_elem_reduces(const struct terrain_element *t_elem)
{
	return t_elem_timeout(t_elem) > 0 || t_elem->kind->idx == TE_FIRE;
}


/*static int t_elem_max_proj_range(struct terrain_element_kind *kind)
{
	struct terrain_element_level *lev;
	int highest = -1;

	for (lev = kind->levels; lev; lev = lev->next) {
		highest = MAX(highest, lev->proj_range);
	}

	return highest;
}*/



/*bool t_elem_is_los(const struct terrain_element_kind *kind)
{
	return tf_has(kind->flags, TF_LOS);
}*/

bool sq_any_t_elem_has_flag(const struct square *sq, int flag)
{
	const struct terrain_element *t_elem;

	for (t_elem = sq->t_elem; t_elem; t_elem = t_elem->next) {
		if (t_elem_has_flag(t_elem, flag)) return true;
	}

	return false;
}

bool sq_all_t_elem_has_flag(const struct square *sq, int flag)
{
	const struct terrain_element *t_elem;

	for (t_elem = sq->t_elem; t_elem; t_elem = t_elem->next) {
		if (!t_elem_has_flag(t_elem, flag)) return false;
	}

	return true;
}



void square_memorize_t_elem(struct chunk *c, struct loc grid)
{
	struct terrain_element *actual, *known, *temp;

	if (!player->cave) return;

	assert(c);
	assert(player->cave);
	assert(square_in_bounds(c, grid));
	assert(square_in_bounds(player->cave, grid));

	actual = square_t_elem(c, grid);
	known = square_t_elem(player->cave, grid);

	while (actual || known) {
		if (known && (!actual || actual->kind->idx > known->kind->idx)) {
			temp = known;
			known = known->next;
			terrain_elem_remove(player->cave, grid, temp->kind->idx);
		}
		else if (actual && (!known || known->kind->idx > actual->kind->idx)) {
			temp = actual;
			actual = actual->next;
			terrain_element_add(player->cave, grid, temp->kind->idx, (uint16_t)temp->timer);
		}
		else {
			actual = actual->next;
			known = known->next;
		}
	}


	/*const struct terrain_element *t_elem;
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
	}*/
}



struct terrain_element *terrain_element_new(int timer, int idx)
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



bool terrain_element_add(struct chunk *c, struct loc grid, int idx, uint16_t timer)
{
	struct terrain_element *prev = NULL, *new = terrain_element_new(timer, idx);

	if (!square_canputterrainelem(c, grid, idx)) return false;

	if ((!square_t_elem(c, grid)) || (square_t_elem(c, grid)->kind->idx > idx)) {
		new->next = square_t_elem(c, grid);
		c->squares[grid.y][grid.x].t_elem = new;
	}
	else {
		prev = square_t_elem(c, grid);
		while (prev->next && prev->next->kind->idx < idx) {
			prev = prev->next;
		}

		assert(prev->kind->idx < idx);
		if (prev->kind->idx == idx) {
			terrain_elem_free(new);
			return false;
		}

		new->next = prev->next;
		prev->next = new;
	}

	if (square_isview(c, grid)) {
		player->upkeep->update |= PU_UPDATE_VIEW;
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

			if (square_isview(c, grid)) {
				player->upkeep->update |= PU_UPDATE_VIEW;
			}

			return true;
		}
	}

	return false;
}


/**
 * reduces or increases the duration as appropriate of the terrain element  idx  on
 * square  grid .
 * returns the new duration of the terrain element
 */
int terrain_element_change_dur(struct chunk *c, struct loc grid, uint16_t idx, int change)
{
	struct terrain_element *t_elem;
	struct terrain_element_level *lev;

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		assert(t_elem->timer > 0);

		if (t_elem->kind->idx == (int)idx) {
			lev = t_elem_level(t_elem->kind, t_elem->timer);

			if (t_elem->timer + change <= 0) {
				terrain_elem_remove(c, grid, idx);

				return 0;
			} else {
				t_elem->timer += change;
			}

			if (square_isview(c, grid)) {
				if ((t_elem->timer < lev->min_dur) ||
						(lev->next && (t_elem->timer >= lev->next->min_dur))) {
					player->upkeep->update |= PU_UPDATE_VIEW;
				}
			}

			return (int)t_elem->timer;
		}
	}

	if (change > 0) {
		terrain_element_add(c, grid, idx, (uint16_t)change);
	}

	return t_elem_timer(c, grid, idx);
}

/**
 * Increases the duration of the terrain element by  change , or adds it if it did
 * not previously exist in the square
 * Returns  true  if a terrain element was added
 */
int terrain_element_increase_dur(struct chunk *c, struct loc grid, uint16_t idx, uint16_t change)
{
	return terrain_element_change_dur(c, grid, idx, (int)change);
}

/**
 * Reduces the duration of the terrain element by  change , and removes it if it would
 * reduce to 0 or lower.
 * Returns the new duration of the terrain_element
 */
int terrain_element_reduce_dur(struct chunk *c, struct loc grid, uint16_t idx, uint16_t change)
{
	return terrain_element_change_dur(c, grid, idx, -((int)change));
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

struct terrain_element *square_t_elem_by_type(struct chunk *c, struct loc grid, uint16_t which)
{
	struct terrain_element *t_elem;

	for (t_elem = square_t_elem(c, grid); t_elem && t_elem->kind->idx != which; t_elem = t_elem->next) {
	}

	return t_elem;
}




bool grid_is_danger(const struct monster *mon, struct chunk *c, struct loc grid)
{
	const struct terrain_element *t_elem;
	int proj;

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		proj = t_elem_proj(t_elem);

		if (proj < 0) continue;
		if (!mon_proj_is_immune(mon, proj)) {
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
	int dir, basedelta, delta, lin_div = 12U;
	struct square *sq = &c->squares[grid.y][grid.x];
	struct terrain_element *t_elem = sq->t_elem;
	struct loc newgrid;
	bool did_something = false;

	while (t_elem && t_elem->kind->idx != kind) {
		t_elem = t_elem->next;
	}

	if (!t_elem) return did_something;
	if (!t_elem_has_flag(t_elem, TF_CLOUD)) return did_something;

	assert(square_in_bounds_fully(c, grid));

	basedelta = t_elem->timer / lin_div;

	assert(lin_div >= 8);

	for (dir = 1; dir <= 9; ++dir) {
		delta = randint0(basedelta);
		newgrid = loc_sum(grid, ddgrid[dir]);

		if (delta <= 0) continue;
		if (loc_eq(newgrid, grid)) continue;
		if (!square_in_bounds_fully(c, newgrid)) continue;
		if (!square_canputterrainelem(c, newgrid, (uint16_t)kind)) continue;

		delta = MIN(delta, MAX(0, changes[grid.y][grid.x] - INT8_MIN));
		delta = MIN(delta, MAX(0, INT8_MAX - changes[newgrid.y][newgrid.x]));

		changes[newgrid.y][newgrid.x] += delta;
		changes[grid.y][grid.x] -= delta;

		if (delta != 0) did_something = true;
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
	uint16_t i, reductions[TE_MAX] = { 0 };

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		if (t_elem->kind->idx == TE_FIRE) {
			curr_fire = t_elem->timer;
		} else {
			++n_t_elem;
		}
	}

	power = (2 * power + n_t_elem) / (n_t_elem + 1);

	burn_amt = 0;

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		if (t_elem_has_flag(t_elem, TF_BURN_FAST)) {
			temp_power = MAX(5, power * power / 3);
		}
		else if (t_elem_has_flag(t_elem, TF_BURN)) {
			temp_power = MIN(power * 2, 5);
		}
		else {
			temp_power = 0;
		}

		temp_power = MIN(temp_power, t_elem->timer);

		burn_amt += temp_power;
		reductions[t_elem->kind->idx] = temp_power;
	}

	for (i = 0; i < TE_MAX; ++i) {
		if (reductions[i] > 0) {
			terrain_element_reduce_dur(c, grid, i, reductions[i]);
		}
	}

	if (burn_amt > 0) {
		terrain_element_increase_dur(c, grid, TE_FIRE, (uint16_t)burn_amt);
	}
	else if (curr_fire > 0) {
		terrain_element_reduce_dur(c, grid, TE_FIRE, (curr_fire * 2 + 1) / 3);
	}

	return t_elem_timer(c, grid, TE_FIRE);
}


static void t_elem_effect_message(struct chunk *c, struct loc grid, int which)
{
	struct terrain_element *t_elem = square_t_elem_by_type(c, grid, which);
	int proj;
	const struct monster *mon = square_monster(c, grid);

	if (!t_elem) return;

	proj = t_elem_proj(t_elem);

	if (square_isplayer(c, grid) && !mon_proj_is_immune(&player->mon, proj)) {
		msg("You are surrounded by %s.", t_elem_name(t_elem));
		disturb(player);
	}

	else if (mon && mon->race && monster_is_in_view(mon) && !mon_proj_is_immune(mon, proj)) {
		char mon_name[80];

		monster_desc(mon_name, sizeof mon_name, mon, MDESC_CAPITAL);

		msg("%s is surrounded by %s.", mon_name, t_elem_name(t_elem));
	}
}

static void t_elem_effects_per_square(struct chunk *c, struct loc grid, uint8_t (**array)[PROJ_MAX])
{
	struct terrain_element *t_elem;
	struct loc new_grid;
	int proj_type, range;
	uint8_t *array_elem;

	if (!square_isprojectable(c, grid)) return;

	for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
		range = t_elem_proj_range(t_elem);
		proj_type = t_elem_proj(t_elem);

		if (proj_type < 0) continue;
		assert(proj_type >= 0 && proj_type < PROJ_MAX);

		for (new_grid.x = grid.x - range; new_grid.x <= grid.x + range; ++new_grid.x) {
			for (new_grid.y = grid.y - range; new_grid.y <= grid.y + range; ++new_grid.y) {
				if (distance(new_grid, grid) > range) continue;
				if (!square_in_bounds_fully(c, new_grid)) continue;
				if (!loc_eq(grid, new_grid) && !projectable(c, grid, new_grid, 0)) continue;

				array_elem = &array[new_grid.y][new_grid.x][proj_type];

				if ((uint16_t)t_elem->timer > UINT8_MAX) {
					*array_elem = UINT8_MAX;
				} else if (*array_elem > UINT8_MAX - (uint16_t)t_elem->timer) {
					*array_elem = UINT8_MAX;
				} else {
					*array_elem += t_elem->timer;
				}
			}
		}
	}
}

void t_elem_effects(struct chunk *c)
{
	struct loc grid;
	uint16_t flg = PROJECT_HIDE | PROJECT_JUMP | PROJECT_KILL | PROJECT_ITEM | PROJECT_GRID | PROJECT_PLAY;
	int i, dam;
	uint8_t (**sq_projs)[PROJ_MAX];

	struct terrain_element *dummy = NULL;

	sq_projs = mem_zalloc(sizeof *sq_projs * c->height);
	for (i = 0; i < c->height; ++i) {
		sq_projs[i] = mem_zalloc(sizeof **sq_projs * c->width);
	}

	for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
		for (grid.y = 1; grid.y < c->width - 1; ++grid.y) {
			t_elem_effects_per_square(c, grid, sq_projs);
			if (!dummy && square_t_elem(c, grid)) {
				dummy = square_t_elem(c, grid);
			}
		}
	}

	for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
		for (grid.y = 1; grid.y < c->height - 1; ++grid.y) {
			for (i = 0; i < PROJ_MAX; ++i) {
				dam = (int)sq_projs[grid.y][grid.x][i];
				if (dam > 0) {
					assert(dummy);
					project(source_t_elem(dummy), 0, grid, dam, i, flg, 0, 0, NULL);
					t_elem_effect_message(c, grid, i);
				}
			}
		}
	}

	for (i = 0; i < c->height; ++i) {
		mem_free(sq_projs[i]);
	}
	mem_free(sq_projs);
}

void t_elem_reduce_durations(struct chunk *c)
{
	struct loc grid;
	struct terrain_element *t_elem;
	int reduce_fact = turn / turns_per_process_world, timeout;

	for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
		for (grid.y = 1; grid.y < c->height - 1; ++grid.y) {
			for (t_elem = square_t_elem(c, grid); t_elem; t_elem = t_elem->next) {
				timeout = t_elem_timeout(t_elem);
				if (timeout > 0 && !(reduce_fact % timeout)) {
					terrain_element_reduce_dur(c, grid, t_elem->kind->idx, 1);
				}
			}
		}
	}
}



static bool feat_produce_t_elem(struct chunk *c, struct loc grid)
{
	const struct feature_kind *feat = square_feat_old(c, grid);
	struct terrain_element_kind *kind;
	uint16_t idx, amt, curr, increase;
	bool did_something;

	for (kind = te_info; kind; kind = kind->next) {
		idx = kind->idx;

		amt = feat->t_elem[idx];

		if (amt <= 0) continue;

		curr = t_elem_timer(c, grid, idx);

		if (curr >= amt) continue;

		increase = amt - curr;

		terrain_element_increase_dur(c, grid, idx, increase);

		did_something = did_something || increase > 0;

		if (feat->t_elem_msg && square_isview(c, grid)) {
			msg("The %s %s %s.", feat->name, feat->t_elem_msg, t_elem_level(kind, (int)increase)->name);
		}
	}

	return did_something;
}

#define T_ELEM_PRODUCE_FREQ 5

bool cave_produce_t_elem(struct chunk *c)
{
    struct loc grid;
    bool did_something = false;

	float x, x_freq;
	int y, y_freq;
	int trn = turn / turns_per_process_world;

	y_freq = my_int_sqrt(T_ELEM_PRODUCE_FREQ);
	x_freq = (float)T_ELEM_PRODUCE_FREQ / (float)y_freq;

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
}

static bool cave_all_produce_t_elem(struct chunk *c)
{
	struct loc grid;
	bool did_something = false;

	for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
		for (grid.y = 1; grid.y < c->height - 1; ++grid.y) {
			did_something = feat_produce_t_elem(c, grid) || did_something;
		}
	}

	return did_something;
}

void cave_handle_t_elem(struct chunk *c)
{
	t_elem_effects(c);
	cave_produce_t_elem(c);
	t_elem_spread(c);
	t_elem_reduce_durations(c);
}

void cave_init_t_elem(struct chunk *c, int times)
{
	int i;

	for (i = 0; i < times; ++i) {
		if (!(i % T_ELEM_PRODUCE_FREQ)) {
			cave_all_produce_t_elem(c);
		}

		t_elem_spread(c);
		t_elem_reduce_durations(c);
	}
}
