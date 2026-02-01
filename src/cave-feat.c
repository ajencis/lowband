#include "angband.h"
#include "cave.h"
#include "game-world.h"
#include "mon-desc.h"
#include "project.h"


const struct feature *ref_feat = NULL;


/**
 * compares the feats: returns -1 if feat1 comes first, 1 if feat2 comes first, or
 * 0 if they are interchangeable (which should not happen...)
 */
static int feat_compare(int feat1, int feat2)
{
	struct feature_kind *kind1 = &f_info[feat1], *kind2 = &f_info[feat2];

	assert(feat1 >= FEAT_NONE && feat1 < FEAT_MAX);
	assert(feat2 >= FEAT_NONE && feat2 < FEAT_MAX);

	if (kind1->priority > kind2->priority) return -1;
	if (kind2->priority > kind1->priority) return 1;

	if (feat1 > feat2) return 1;
	if (feat2 > feat1) return -1;

	return 0;
}

#ifdef FEAT_ALLOC_DBG

static int num_feats_alloced = 0;

#endif

static struct feature *feat_new(int fidx, int size)
{
	struct feature *new;

	assert(fidx >= FEAT_NONE && fidx < FEAT_MAX);

	if (size <= 0) return NULL;

	new = mem_zalloc(sizeof *new);

#ifdef FEAT_ALLOC_DBG	
	num_feats_alloced++;
	dbg_log_fmt("feat", "turn %i: new feat, total alloced = %i", turn, num_feats_alloced);
#endif

	new->kind = &f_info[fidx];
	new->size = size;

	assert(new->kind);
	assert(new->kind->name);

	return new;
}

static void feat_free(struct feature *feat)
{
	mem_free(feat);


#ifdef FEAT_ALLOC_DBG
	num_feats_alloced--;
	dbg_log_fmt("feat", "turn %i: freeing feat, total alloced = %i", turn, num_feats_alloced);
#endif
}

static int feat_priority(int feat)
{
	assert(feat >= FEAT_NONE && feat < FEAT_MAX);

	if (feat_is_permanent(feat)) return 1;
	if (feat_is_floor(feat)) return -1;
	if (feat == FEAT_NONE) return -2;

	return 0;
}

static bool feat_blocks_feat(int feat1, int feat2)
{
	if (feat_is_permanent(feat1)) {
		return true;
	}
	if (!feat_is_projectable(feat1)) {
		return true;
	}
	if (feat_is_structural(feat1) && feat_is_structural(feat2)) {
		return true;
	}
	if (feat2 == FEAT_NONE) {
		return true;
	}

	return false;
}

bool feat_incompat_base(int feat1, int feat2)
{
	assert(feat1 >= FEAT_NONE && feat1 < FEAT_MAX);
	assert(feat2 >= FEAT_NONE && feat2 < FEAT_MAX);

	if (feat1 == feat2) return false;

	if (feat_blocks_feat(feat1, feat2)) {
		return true;
	}
	if (feat_blocks_feat(feat2, feat1)) {
		return true;
	}

	return false;
}

/*
static void feat_desc(const struct feature *feat, char *buf, size_t bufsize)
{
	bool first = true;
	const struct feature *curr;

	buf[0] = '\0';

	for (curr = feat; curr; curr = curr->next) {
		if (!first) my_strcat(buf, ", ", bufsize);
		first = false;
		my_strcat(buf, curr->kind->name, bufsize);
	}
}
*/

/**
 * if two feats are on the same square, returns which one to remove
 * usually removes the new one if feats are incompatible
 * returns -1 if they are compatible
 */
static int feat_incompatible(int new, int old)
{
	assert(new >= FEAT_NONE && new < FEAT_MAX);
	assert(old >= FEAT_NONE && old < FEAT_MAX);
	if (new == old) return -1;
	if (new == FEAT_NONE || old == FEAT_NONE) return FEAT_NONE;

	if (feat_incompat_base(new, old)) {
		if (feat_priority(new) > feat_priority(old)) {
			return old;
		}
		return new;
	}

	return -1;
}

bool square_can_add_feat(struct chunk *c, struct loc grid, int fidx)
{
	struct feature *feat;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat_incompatible(fidx, feat->kind->fidx) == fidx) return false;
	}

	return true;
}

static bool feat_can_add(struct chunk *c, struct loc grid, int fidx_new)
{
	struct feature *feat;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat_incompat_base(feat->kind->fidx, fidx_new)) {
			return false;
		}
	}

	return true;
}

static bool list_add_feat(struct feature **list, int fidx, int size)
{
	struct feature *new, *curr;

	assert(list);
	assert(fidx >= FEAT_NONE && fidx < FEAT_MAX);

	if (!(*list) || feat_compare((*list)->kind->fidx, fidx) > 0) {
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
			else if (feat_compare(curr->next->kind->fidx, fidx) > 0) {
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

	for (curr = *list; curr; curr = curr->next) {
		assert(curr);
		assert(curr->kind);
		assert(curr->kind->name);
	}

	return true;
}

static bool square_has_default(struct chunk *c, struct loc grid)
{
	if (!c->feat_default) return false;
	return square_has_feat(c, grid, c->feat_default->fidx);
}

static bool square_should_have_default(struct chunk *c, struct loc grid)
{
	struct feature *feat;

	if (!c->feat_default) return false;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat->kind->fidx != c->feat_default->fidx &&
				feat_incompat_base(feat->kind->fidx, c->feat_default->fidx)) {
			return false;
		}
	}

	return true;
}

static struct point_set *cave_timeout_point_set(struct chunk *c)
{
	return c->timeout_points;
}

static struct point_set *cave_spread_point_set(struct chunk *c)
{
	return c->spread_points;
}

static struct point_set *cave_produce_point_set(struct chunk *c)
{
	return c->produce_points;
}

static struct point_set *cave_project_point_set(struct chunk *c)
{
	return c->project_points;
}


struct point_set_match point_set_matches[] = {
	{ cave_timeout_point_set, feat_times_out, "timeout" },
	{ cave_spread_point_set, feat_spreads, "spread" },
	{ cave_produce_point_set, feat_produces, "produce" },
	{ cave_project_point_set, feat_projects, "project" },
	{ NULL, NULL, NULL }
};

static void square_update_point_sets(struct chunk *c, struct loc grid, int changing_fidx, bool adding)
{
	int i;
	feat_predicate pred;
	struct point_set *curr_set;

	assert(c);

	for (i = 0; point_set_matches[i].set_get; ++i) {
		curr_set = point_set_matches[i].set_get(c);
		pred = point_set_matches[i].pred;

		if (!curr_set) continue;
		if (!pred(changing_fidx)) continue;

		if (adding) {
			add_to_point_set_no_dup(curr_set, grid);
		}
		else if (!first_feat_meets_pred(c, grid, pred)) {
			remove_from_point_set(curr_set, grid);
		}
	}
}

void cave_refresh_point_sets(struct chunk *c)
{
	struct loc grid;
	struct point_set *ps;
	struct feature *feat;
	int i;
	bool has_sets;

	has_sets = false;
	for (i = 0; point_set_matches[i].set_get; ++i) {
		ps = point_set_matches[i].set_get(c);

		if (ps) {
			has_sets = true;
			clear_point_set(ps);
		}
	}

	if (!has_sets) return;

	for (grid.x = 0; grid.x < c->width; ++grid.x) {
		for (grid.y = 0; grid.y < c->height; ++grid.y) {
			for (feat = square_feat(c, grid); feat; feat = feat->next) {
				square_update_point_sets(c, grid, feat->kind->fidx, true);
			}
		}
	}
}

/**
 * L: removes a feat from a square with no upkeep (rememorizing, counting feats)
 */
static bool square_delete_feat(struct chunk *c, struct loc grid, int fidx)
{
	struct feature **prev = NULL, *to_del;
	struct square *sq;

	assert(c);
	assert(square_in_bounds(c, grid));
	assert(c->squares);

	sq = &c->squares[grid.y][grid.x];
	assert(sq);

	prev = &sq->feat;

	while (true) {
		if (!(*prev)) {
			return false;
		}
		if (feat_compare((*prev)->kind->fidx, fidx) > 0) {
			return false;
		}
		if ((*prev)->kind->fidx == fidx) {
			break;
		}

		prev = &((*prev)->next);
	}

	to_del = *prev;

	assert(to_del);

	*prev = to_del->next;

	feat_free(to_del);

	square_update_point_sets(c, grid, fidx, false);

	//if (character_dungeon) assert(square_feat_valid(c, grid));

	return true;
}

/**
 * make sure each square has at least one feat by adding the cave's
 * default feat if the feat is empty
 * removes the default feat if it conflicts with other feats
 */
static void square_enforce_default_feat(struct chunk *c, struct loc grid)
{
	bool should_have_default, has_default;
	int feat_default;

	assert(c);
	assert(square_in_bounds(c, grid));

	if (!c->feat_default) {
		return;
	}

	feat_default = c->feat_default->fidx;
	has_default = square_has_default(c, grid);
	should_have_default = square_should_have_default(c, grid);

	if (should_have_default && !has_default) {
		square_force_add_feat(c, grid, feat_default);
	}
	else if (!should_have_default && has_default) {
		square_force_remove_feat(c, grid, feat_default);
	}
}

static void cave_clear_default_feat(struct chunk *c)
{
	int prev_default;
	struct loc grid;

	if (!c->feat_default) return;

	prev_default = c->feat_default->fidx;

	c->feat_default = NULL;

	for (grid.x = 0; grid.x < c->width; ++grid.x) {
		for (grid.y = 0; grid.y < c->height; ++grid.y) {
			square_delete_feat(c, grid, prev_default);
		}
	}
}

void cave_set_default_feat(struct chunk *c, int fidx)
{
	struct loc grid;
	assert(fidx >= FEAT_NONE && fidx < FEAT_MAX);

	if (c->feat_default) {
		cave_clear_default_feat(c);
	}

	c->feat_default = &f_info[fidx];

	for (grid.x = 0; grid.x < c->width; ++grid.x) {
		for (grid.y = 0; grid.y < c->height; ++grid.y) {
			square_enforce_default_feat(c, grid);
		}
	}
}

bool square_force_add_feat_size(struct chunk *c, struct loc grid, int fidx, int size)
{
	bool success;
	struct feature *feat, *next;

	assert(c);
	assert(square_in_bounds(c, grid));
	assert(fidx >= FEAT_NONE && fidx < FEAT_MAX);

	for (feat = square_feat(c, grid); feat; feat = next) {
		next = feat->next;

		if (feat_incompat_base(fidx, feat->kind->fidx)) {
			square_force_remove_feat(c, grid, feat->kind->fidx);
		}
	}

	success = list_add_feat(&c->squares[grid.y][grid.x].feat, fidx, size);

	if (!success) {
		return false;
	}

	if (c->feat_count) {
		++c->feat_count[fidx];
	}

	if (feat_is_bright(fidx)) {
		sqinfo_on(square(c, grid)->info, SQUARE_GLOW);
	}

	if (!feat_is_trap_holding(fidx)) {
		square_destroy_trap(c, grid);
	}

	if (c == cave && character_dungeon && player->cave) {
		square_note_spot(c, grid);
		square_light_spot(c, grid);
	} else {
		sqinfo_off(square(c, grid)->info, SQUARE_WALL_INNER);
		sqinfo_off(square(c, grid)->info, SQUARE_WALL_OUTER);
		sqinfo_off(square(c, grid)->info, SQUARE_WALL_SOLID);
	}

	square_enforce_default_feat(c, grid);

	square_update_point_sets(c, grid, fidx, true);

	return true;
}

bool square_force_add_feat(struct chunk *c, struct loc grid, int fidx)
{
	int size = f_info[fidx].default_size;

	return square_force_add_feat_size(c, grid, fidx, size);
}

void square_copy_feat(struct chunk *from_c, struct chunk *to_c, struct loc from_grid, struct loc to_grid)
{
	struct feature *feat;

	square_free_feats(to_c, to_grid);

	//assert(square_feat_valid(from_c, from_grid));

	for (feat = square_feat(from_c, from_grid); feat; feat = feat->next) {
		list_add_feat(&to_c->squares[to_grid.y][to_grid.x].feat, feat->kind->fidx, feat->size);

		square_update_point_sets(to_c, to_grid, feat->kind->fidx, true);
	}
}

bool square_add_feat(struct chunk *c, struct loc grid, int fidx)
{
	struct feature *feat;
	bool success;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat_incompatible(fidx, feat->kind->fidx) == fidx) {
			return false;
		}
	}

	success = square_force_add_feat(c, grid, fidx);
	return success;
}

static bool feat_can_remove(int fidx)
{
	if (feat_is_permanent(fidx)) return false;
	if (feat_is_shop(fidx)) return false;
	return true;
}

/**
 * L: removes a feat from a square and performs upkeep
 */
bool square_force_remove_feat(struct chunk *c, struct loc grid, int fidx)
{
	if (!square_delete_feat(c, grid, fidx)) return false;

	if (character_dungeon && c == cave && player->cave) {
		square_note_spot(c, grid);
		square_light_spot(c, grid);
	}

	sqinfo_off(square(c, grid)->info, SQUARE_WALL_INNER);
	sqinfo_off(square(c, grid)->info, SQUARE_WALL_OUTER);
	sqinfo_off(square(c, grid)->info, SQUARE_WALL_SOLID);

	if (c->feat_count) {
		--c->feat_count[fidx];
	}

	square_enforce_default_feat(c, grid);

	return true;
}

/**
 * L: tries to remove a feat from a square, refuses to if the feat is permanent
 */
bool square_remove_feat(struct chunk *c, struct loc grid, int fidx)
{
	assert(c);

	bool success;

	assert(fidx < FEAT_MAX && fidx >= FEAT_NONE);
	assert(c);

	if (!feat_can_remove(fidx)) {
		return false;
	}

	success = square_force_remove_feat(c, grid, fidx);

	if (success) {
		square_enforce_default_feat(c, grid);
	}

	return success;
}

/**
 * L: frees feats from a square in order to free the cave
 */
void square_free_feats(struct chunk *c, struct loc grid)
{
	struct feature *feat, *next;
	int fidx;

	assert(c);

	for (feat = square_feat(c, grid); feat; feat = next) {
		next = feat->next;
		fidx = feat->kind->fidx;
		feat_free(feat);
		square_update_point_sets(c, grid, fidx, false);
	}

	c->squares[grid.y][grid.x].feat = NULL;

	assert(!square_feat(c, grid));
}

/**
 * L: frees feats from a square in normal situations
 */
void square_clear_feats(struct chunk *c, struct loc grid)
{
	struct feature *feat, *next;
	int def_id = c->feat_default ? c->feat_default->fidx : -1;

	for (feat = square_feat(c, grid); feat; feat = next) {
		next = feat->next;
		
		if (feat_can_remove(feat->kind->fidx) && (feat->kind->fidx != def_id)) {
			square_remove_feat(c, grid, feat->kind->fidx);
		}
	}
}

struct feature *square_feat(struct chunk *c, struct loc grid)
{
	assert(c);

	struct feature *result = c->squares[grid.y][grid.x].feat;

	return result;
}

struct feature *square_feat_by_type(struct chunk *c, struct loc grid, int fidx)
{
	struct feature *feat;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat->kind->fidx == fidx) {
			return feat;
		}
		if (feat_compare(feat->kind->fidx, fidx) > 0) {
			return NULL;
		}
	}

	return NULL;
}

void square_remove_feat_by_type(struct chunk *c, struct loc grid, feat_predicate pred)
{
	struct feature *feat = square_feat(c, grid);

	while (feat && pred(feat->kind->fidx)) {
		square_remove_feat(c, grid, feat->kind->fidx);
		feat = square_feat(c, grid);
	}

	while (feat && feat->next) {
		if (pred(feat->next->kind->fidx)) {
			square_remove_feat(c, grid, feat->next->kind->fidx);
		}
		else {
			feat = feat->next;
		}
	}
}

/**
 * Tries to force grid  grid  in cave  c  to include feat  fidx  by removing
 * all feats that clash with feat  fidx  then adding it
 */
static bool square_set_feat_base(struct chunk *c, struct loc grid, int fidx, int size, bool force)
{
	struct feature *feat, *next;
	bool result;
	int def_id = c->feat_default ? c->feat_default->fidx : -1;

	assert(c);
	assert(square_in_bounds(c, grid));

	if (!force) {
		for (feat = square_feat(c, grid); feat; feat = feat->next) {
			if (feat_incompat_base(feat->kind->fidx, fidx) && !feat_can_remove(feat->kind->fidx)) {
				return false;
			}
		}
	}

	for (feat = square_feat(c, grid); feat; feat = next) {
		next = feat->next;

		if (feat_incompat_base(feat->kind->fidx, fidx)) {
			if (feat->kind->fidx == def_id) {
				continue;
			}
			square_force_remove_feat(c, grid, feat->kind->fidx);
		}
	}

	result = square_add_feat(c, grid, fidx);

	return result;
}

bool square_set_feat(struct chunk *c, struct loc grid, int fidx, int size)
{
	return square_set_feat_base(c, grid, fidx, size, false);
}

bool square_force_set_feat(struct chunk *c, struct loc grid, int fidx, int size)
{
	return square_set_feat_base(c, grid, fidx, size, true);
}

bool square_remove_feats_by_flag(struct chunk *c, struct loc grid, int flag)
{
	struct feature *feat = square_feat(c, grid);

	while (feat) {
		if (tf_has(feat->kind->flags, flag)) {
			square_remove_feat(c, grid, feat->kind->fidx);
			feat = square_feat(c, grid);
		}
		else if (feat->next && tf_has(feat->next->kind->flags, flag)) {
			square_remove_feat(c, grid, feat->next->kind->fidx);
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

	assert(new >= FEAT_NONE && new < FEAT_MAX);

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat->kind->fidx == old) {
			square_force_add_feat_size(c, grid, new, feat->size);
			square_force_remove_feat(c, grid, old);
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

void square_set_feat_size(struct chunk *c, struct loc grid, int fidx, int size)
{
	struct feature *curr;
	struct square *sq = &c->squares[grid.y][grid.x];

	assert(fidx >= FEAT_NONE && fidx < FEAT_MAX);

	if (size <= 0) {
		square_remove_feat(c, grid, fidx);
		return;
	}

	for (curr = sq->feat; curr; curr = curr->next) {
		if (curr->kind->fidx == fidx) {
			curr->size = size;
			return;
		}
		else if (!curr->next || feat_compare(curr->kind->fidx, fidx) > 0) {
			break;
		}
	}

	square_force_add_feat_size(c, grid, fidx, size);
}

void square_increase_feat_size(struct chunk *c, struct loc grid, int fidx, int amt)
{
	struct feature *feat = square_feat_by_type(c, grid, fidx);
	int curr = feat ? feat->size : 0;

	square_set_feat_size(c, grid, fidx, curr + amt);
}

void square_reduce_feat_size(struct chunk *c, struct loc grid, int fidx, int amt)
{
	square_increase_feat_size(c, grid, fidx, -amt);
}

int square_feat_size(struct chunk *c, struct loc grid, int fidx)
{
	struct feature *feat = square_feat_by_type(c, grid, fidx);

	return feat ? feat->size : 0;
}

/**
 * a feat's believed version unless the feat mimics another feat and the actual feat isn't known
 * by the player to be in the square in question
 * eg if a secret door mimics a wall, a secret door feat's believed feat is a secret door if the
 * player believes a secret door to be in that square and a wall otherwise
 */
int feat_believed(struct player *p, struct loc grid, int feat)
{
	struct feature_kind *kind;

	assert(feat >= FEAT_NONE && feat < FEAT_MAX);
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

/**
 * updates feat memorization for a square by clearing the square then adding back all of the feats that
 * would be believed to be there if they meet predicate  pred
 */
static void square_update_feat_memorization(const struct chunk *c, struct player *p, struct loc grid, bool(*pred)(int))
{
	const struct feature *real;
	int fidx, i;
	int new[FEAT_MAX] = { 0 };

	assert(p);
	assert(c);
	assert(p->cave);
	assert(square_in_bounds((struct chunk *)c, grid));

	for (real = c->squares[grid.y][grid.x].feat; real; real = real->next) {
		fidx = feat_believed(p, grid, real->kind->fidx);

		assert(fidx >= FEAT_NONE && fidx < FEAT_MAX);
		new[fidx] = MAX(new[fidx], real->size);
	}

	square_clear_feats(p->cave, grid);

	for (i = 0; i < FEAT_MAX; ++i) {
		if (new[i] > 0 && (!pred || pred(i))) {
			square_force_add_feat_size(p->cave, grid, i, new[i]);
		}
	}
}

void square_memorize_feat_one(struct player *p, struct loc grid, const struct feature *feat, bool real)
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
	if (c != cave) {
		return;
	}

	assert(p);
	assert(c);
	assert(p->cave);

	square_update_feat_memorization(c, p, grid, NULL);
}

void square_memorize_struct_feats(struct player *p, const struct chunk *c, struct loc grid)
{
	if (c != cave) {
		return;
	}

	assert(p);
	assert(c);
	assert(p->cave);

	square_update_feat_memorization(c, p, grid, feat_is_structural);
}

static void square_forget_feats_imagined_by_pred(struct player *p, struct chunk *c, struct loc grid, feat_predicate pred)
{
	struct feature *imagined, *real;

	for (imagined = square_feat(p->cave, grid), real = square_feat(c, grid); imagined; imagined = imagined->next) {
		if (p->cave->feat_default->fidx == imagined->kind->fidx) continue;
		if (pred && !pred(imagined->kind->fidx)) continue;

		while (real && feat_compare(real->kind->fidx, imagined->kind->fidx) < 0) {
			real = real->next;
		}

		if (!real || feat_compare(real->kind->fidx, imagined->kind->fidx) > 0) {
			square_remove_feat(c, grid, imagined->kind->fidx);
		}
	}
}

/**
 * Memorizes all feats on a square of a certain type
 */
static void square_memorize_feats_real_by_pred(struct player *p, struct chunk *c, struct loc grid, feat_predicate pred)
{
	struct feature *feat;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (!pred || pred(feat->kind->fidx)) {
			square_memorize_feat_real(p, c, grid, feat->kind->fidx);
		}
	}
}

void square_ensure_correct_memorization_by_pred(struct player *p, struct chunk *c, struct loc grid, feat_predicate pred)
{
	if (c != cave) return;

	assert(c);
	assert(p);

	square_forget_feats_imagined_by_pred(p, c, grid, pred);
	square_memorize_feats_real_by_pred(p, c, grid, pred);
}

void square_forget_feats(struct player *p, struct loc grid)
{
	square_clear_feats(p->cave, grid);
}

void square_memorize_feat_real(struct player *p, struct chunk *c, struct loc grid, int fidx)
{
	struct feature_kind *mimic = f_info[fidx].mimic;
	struct feature *feat = square_feat_by_type(c, grid, fidx);

	if (!feat) return;

	if (mimic && !square_has_feat(c, grid, mimic->fidx)) {
		square_remove_feat(p->cave, grid, mimic->fidx);
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


/**
 * gives the number of times something should happen this turn if that thing
 * happens  pernmille  times per thousand turns
 * note this is short turns not long turns
 */
static int per_thousand_turns(int permille, int trn)
{
	int last_check, this_check;
	int turn_use = trn % (1000 * turns_per_process_world); // overflow unlikely but paranoia

	if (permille == 0) return 0;
	if (permille < 0) return -per_thousand_turns(-permille, trn);

	// how many times it would happen between this check and the last after rounding
	last_check = (int)(permille * (turn_use - turns_per_process_world)) / 1000;
	this_check = (int)(permille * turn_use) / 1000;

	return this_check - last_check;
}

static void grid_feat_timeout(struct chunk *c, struct loc grid, int trn)
{
	struct feature *feat, *next;
	int amt;

	for (feat = square_feat(c, grid); feat; feat = next) {
		next = feat->next;

		if (!feat_times_out(feat->kind->fidx)) continue;

		amt = per_thousand_turns(feat->kind->timeout, trn);

		if (amt) {
			square_reduce_feat_size(c, grid, feat->kind->fidx, amt);
		}
	}
}

static void grid_feat_produce(struct chunk *c, struct loc grid, int trn)
{
	struct feature *feat;
	int new_fidx, i, curr;
	int to_produce[FEAT_MAX] = { 0 };

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		new_fidx = feat->kind->feat_produce;
		if (new_fidx <= FEAT_NONE || new_fidx >= FEAT_MAX) continue;
		if (per_thousand_turns(feat->kind->feat_produce_frequency, trn) <= 0) continue;

		to_produce[new_fidx] = MAX(to_produce[new_fidx], feat->kind->feat_produce_quantity);
	}

	for (i = FEAT_NONE; i < FEAT_MAX; ++i) {
		feat = square_feat_by_type(c, grid, i);

		if (feat) curr = feat->size;
		else curr = 0;

		if (to_produce[i] > curr) {
			square_set_feat_size(c, grid, i, to_produce[i]);
		}
	}
}

static void grid_feat_spread(struct chunk *c, md_array *values, struct loc grid)
{
	struct feature *feat;
	struct loc ogrid;
	int change, c_max, c_max_o, c_max_d;
	static int diag_perc = 141; // sqrt(2 * 100 * 100)

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		c_max_o = feat->size / 10;
		c_max_d = 100 * c_max_o / diag_perc;

		if (c_max_o <= 0) {
			continue;
		}
		if (!feat_spreads(feat->kind->fidx)) {
			continue;
		}

		for (ogrid.y = grid.y - 1; ogrid.y <= grid.y + 1; ++ogrid.y) {
			for (ogrid.x = grid.x - 1; ogrid.x <= grid.x + 1; ++ogrid.x) {
				if (loc_eq(ogrid, grid)) continue;
				if (!square_in_bounds(c, ogrid)) continue;
				if (!feat_can_add(c, ogrid, feat->kind->fidx)) continue;

				c_max = ((ogrid.x != grid.x) && (ogrid.y != grid.y) ? c_max_d : c_max_o);

				if (c_max <= 0) continue;

				change = randint1(c_max);

				mda_element_add(values, change, ogrid.y, ogrid.x, feat->kind->fidx);
				mda_element_add(values, -change, grid.y, grid.x, feat->kind->fidx);
			}
		}
	}
}

static void cave_feat_spread(struct chunk *c)
{
	md_array *changes = mda_new(3, c->height, c->width, PROJ_MAX);
	int fidx, change, i;
	struct loc grid;

	assert(c);
	assert(c->height <= MAX_CAVE_HEIGHT);
	assert(c->width <= MAX_CAVE_WIDTH);

	for (i = 0; i < c->spread_points->n; ++i) {
		grid_feat_spread(c, changes, c->spread_points->pts[i]);
	}
	/*for (grid.y = 1; grid.y < c->height - 1; ++grid.y) {
		for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
			grid_feat_spread(c, changes, grid);
		}
	}*/

	for (grid.y = 1; grid.y < c->height; ++grid.y) {
		for (grid.x = 1; grid.x < c->width; ++grid.x) {
			for (fidx = 0; fidx < FEAT_MAX; ++fidx) {
				change = mda_element_get(changes, grid.y, grid.x, fidx);
				if (change != 0) {
					square_increase_feat_size(c, grid, fidx, change);
				}
			}
		}
	}

	mda_free(changes);
}

static void grid_feat_proj(struct chunk *c, struct loc grid, md_array *proj_amt, md_array *feats_projecting)
{
	struct loc ogrid;
	struct feature *feat;
	int which, dist, amt;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		which = feat->kind->proj;
		dist = feat->kind->proj_range;

		if (which < 0) continue;

		ref_feat = feat;

		for (ogrid.x = grid.x - dist; ogrid.x <= grid.x + dist; ++ogrid.x) {
			for (ogrid.y = grid.y - dist; ogrid.y <= grid.y + dist; ++ogrid.y) {
				if (!square_in_bounds_fully(c, ogrid)) continue;
				if (!square_isprojectable(c, ogrid)) continue;
				if (distance(grid, ogrid) > dist) continue;

				amt = dice_roll(feat->kind->proj_amt, NULL);

				mda_element_add(proj_amt, amt, ogrid.y, ogrid.x, which);
				mda_element_set(feats_projecting, 1, ogrid.y, ogrid.x, feat->kind->fidx);
			}
		}
	}

	ref_feat = NULL;
}

static bool cave_feat_proj_msg(struct chunk *c, struct loc grid, int proj, md_array *feats_projecting)
{
	struct player *p = square_isplayer(c, grid) ? player : NULL;
	struct monster *mon = square_monster(c, grid);
	char mdesc[64], message[256];
	int i;

	if (p) {
		strnfmt(mdesc, sizeof mdesc, "you");
		mon = &p->mon;
	}
	else if (mon) {
		monster_desc(mdesc, sizeof mdesc, mon, 0);
	}

	if (!mon) return false;
	if (mon_proj_is_immune(mon, proj)) return false;
	if (!monster_is_visible(mon) && !p) return false;

	for (i = 0; i < FEAT_MAX; ++i) {
		if (f_info[i].proj != proj) continue;
		if (!mda_element_get(feats_projecting, grid.y, grid.x, i)) continue;

		strnfmt(message, sizeof message, "%s%s %s %s.",
				f_info[i].look_prefix,
				f_info[i].name,
				projections[proj].verb_third,
				mdesc);
		my_strcap(message);
		msg(message);
	}

	return true;
}

static void cave_feat_proj(struct chunk *c)
{
	md_array *proj_amt, *feats_projecting;
	struct loc grid;
	int which, amt, i;
	int flg = PROJECT_HIDE | PROJECT_JUMP | PROJECT_KILL | PROJECT_ITEM | PROJECT_GRID | PROJECT_PLAY;

	if (c != cave) return;
	assert(c->project_points);

	proj_amt = mda_new(3, c->height, c->width, PROJ_MAX);
	feats_projecting = mda_new(3, c->height, c->width, FEAT_MAX);

	for (i = 0; i < c->project_points->n; ++i) {
		grid_feat_proj(c, c->project_points->pts[i], proj_amt, feats_projecting);
	}

	for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
		for (grid.y = 1; grid.y < c->height - 1; ++grid.y) {
			for (which = 0; which < PROJ_MAX; ++which) {
				amt = mda_element_get(proj_amt, grid.y, grid.x, which);

				if (amt > 0) {
					cave_feat_proj_msg(c, grid, which, feats_projecting);
					project(source_grid(grid), 0, grid, amt, which, flg, 0, 0, NULL);
				}
			}
		}
	}

	mda_free(proj_amt);
	mda_free(feats_projecting);
}


static void cave_feat_upkeep_base(struct chunk *c, int trn)
{
	int i;

	assert(c);
	assert(c->spread_points);
	assert(c->timeout_points);
	assert(c->produce_points);
	assert(c->project_points);

	cave_feat_spread(c);
	
	for (i = 0; i < c->timeout_points->n; ++i) {
		grid_feat_timeout(c, c->timeout_points->pts[i], trn);
	}

	for (i = 0; i < c->produce_points->n; ++i) {
		grid_feat_produce(c, c->produce_points->pts[i], trn);
	}
	/*for (grid.y = 0; grid.y < c->height; ++grid.y) {
		for (grid.x = 0; grid.x < c->width; ++grid.x) {
			grid_feat_timeout(c, grid, trn);
		}
	}
	for (grid.y = 0; grid.y < c->height; ++grid.y) {
		for (grid.x = 0; grid.x < c->width; ++grid.x) {
			grid_feat_produce(c, grid, trn);
		}
	}*/
	cave_feat_proj(c);
}

bool cave_all_feats_valid(struct chunk *c)
{
	struct loc grid;
	bool valid = true;

	for (grid.x = 0; grid.x < c->width; ++grid.x) {
		for (grid.y = 0; grid.y < c->height; ++grid.y) {
			if (!square_feat_valid(c, grid)) valid = false;
		}
	}

	return valid;
}

void cave_feat_upkeep(struct chunk *c)
{
	cave_feat_upkeep_base(c, turn);

	assert(cave_all_feats_valid(c));
}

void cave_feat_initial_upkeep(struct chunk *c)
{
	int faketurn, faketurn_start;
	int num_fake = 100;

	faketurn_start = turn - num_fake * turns_per_process_world;
	faketurn_start -= faketurn_start % turns_per_process_world;

	for (faketurn = faketurn_start; faketurn <= turn; faketurn += turns_per_process_world) {
		cave_feat_upkeep_base(c, faketurn);
	}

	assert(cave_all_feats_valid(c));
}

static void feat_valid_plog(struct chunk *c, struct loc grid, const char *fmt, ...)
{
	const char *c_name;
	const char *type;
	char error[128];
	va_list vp;

	dbg_log("feat", "Error!");

	assert(c);
	assert(fmt);

	c_name = c->name ? c->name : "(unnamed cave)";

	va_start(vp, fmt);
	vstrnfmt(error, sizeof error, fmt, vp);
	va_end(vp);

	if (c == cave) {
		type = " (cave)";
	} else if (player && c == player->cave) {
		type = " (player->cave)";
	} else {
		type = "";
	}

	//msg_add_fmt("Error: chunk %s%s %s at grid (%i,%i)!",
	dbg_log_fmt("feat", "Error: chunk %s%s %s at grid (%i,%i)!",
	//plog_fmt("Error: chunk %s%s %s at grid (%i,%i)!",
			c_name,
			type,
			error,
			grid.x,
			grid.y);
}

bool square_feat_valid(struct chunk *c, struct loc grid)
{
	assert(c);
	assert(square_in_bounds(c, grid));

	struct feature *feat1, *feat2, *start = square_feat(c, grid);
	bool has_def, shld_def;
	int i;

	has_def = square_has_default(c, grid);
	shld_def = square_should_have_default(c, grid);

	if (c->feat_default) {
		if (has_def && !shld_def) {
			feat_valid_plog(c, grid, "incorrectly has default feat %s", c->feat_default->name);
			return false;
		}
		if (!has_def && shld_def) {
			feat_valid_plog(c, grid, "incorrectly does not have default feat %s", c->feat_default->name);
			return false;
		}
		if (!start) {
			feat_valid_plog(c, grid, "has no feats");
			return false;
		}
	}

	for (i = 0; point_set_matches[i].set_get; ++i) {
		struct point_set *ps = point_set_matches[i].set_get(c);
		bool should, has;

		if (!ps) continue;

		should = first_feat_meets_pred(c, grid, point_set_matches[i].pred) ? true : false;
		has = point_set_contains(ps, grid) ? true : false;

		if (should && !has) {
			feat_valid_plog(c, grid, "has a %s feat but grid is not in pointset", point_set_matches[i].name);
			return false;
		}
		else if (!should && has) {
			feat_valid_plog(c, grid, "has no %s feat but grid is in pointset", point_set_matches[i].name);
			return false;
		}
	}

	for (feat1 = start; feat1; feat1 = feat1->next) {
		if (feat1->size <= 0) {
			feat_valid_plog(c, grid, "has feat %s of size %i", feat1->kind->name, feat1->size);
			return false;
		}

		for (feat2 = feat1->next; feat2; feat2 = feat2->next) {
			assert(feat2);
			assert(feat2->kind);

			if (feat_compare(feat1->kind->fidx, feat2->kind->fidx) > 0) {
				feat_valid_plog(c, grid, "has incorrectly ordered feats %s [prio %i] before %s [prio %i]",
						feat1->kind->name, feat1->kind->priority,
						feat2->kind->name, feat2->kind->priority);
				return false;
			}
			if (feat_incompat_base(feat1->kind->fidx, feat2->kind->fidx)) {
				feat_valid_plog(c, grid, "has incompatible feats %s and %s", feat1->kind->name, feat2->kind->name);
				return false;
			}
			if (feat1->kind->fidx == feat2->kind->fidx) {
				feat_valid_plog(c, grid, "has duplicate feat %s", feat1->kind->name);
				return false;
			}
		}
	}

	return true;
}

int burn_square(struct chunk *c, struct loc grid, int power, int fidx)
{
	struct feature *feat, *next;
	int burn_amt, total_amt = 0;
	int burn_num = 0;

	assert(square_in_bounds_fully(c, grid));
	if (!square_in_bounds_fully(c, grid)) return 0;
	assert(fidx < FEAT_MAX && fidx > FEAT_NONE);
	assert(c);

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat_burns(feat->kind->fidx)) ++burn_num;
	}

	if (burn_num > 0) {
		power = (power + burn_num - 1) / burn_num;
	}

	for (feat = square_feat(c, grid); feat; feat = next) {
		next = feat->next;
		burn_amt = 0;

		if (feat_burns_fast(feat->kind->fidx)) {
			burn_amt = MIN(power, (feat->size + 2) / 3);
		}
		else if (feat_burns_slow(feat->kind->fidx)) {
			burn_amt = my_int_cbrt(feat->size * feat->size);
			burn_amt = burn_amt - feat->size * 3;
			burn_amt = MIN(power, burn_amt);
			burn_amt = (burn_amt + 2) / 3;
		}

		if (burn_amt <= 0) {
			continue;
		}

		square_reduce_feat_size(c, grid, feat->kind->fidx, burn_amt);
		//square_increase_feat_size(c, grid, fidx, burn_amt + f_info[fidx].timeout);

		total_amt += burn_amt;
	}

	feat = square_feat_by_type(c, grid, fidx);
	if (burn_amt > 0) {
		burn_amt += f_info[fidx].timeout;
		square_increase_feat_size(c, grid, fidx, burn_amt);
	}
	else if (feat) {
		square_reduce_feat_size(c, grid, fidx, -randint1(feat->size));
	}

	return total_amt;
}

bool grid_is_danger(const struct monster *mon, struct chunk *c, struct loc grid)
{
	struct feature *feat;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat->kind->proj >= 0 && !mon_proj_is_immune(mon, feat->kind->proj)) {
			return true;
		}
	}

	return false;
}


#ifdef FEAT_ALLOC_DBG

static int chunk_feat_log(struct chunk *c)
{
	struct feature *feat;
	struct loc grid;
	char c_name[80] = "(unnamed)";
	int count = 0;

	if (!c) {
		return 0;
	}

	for (grid.x = 0; grid.x < c->width; ++grid.x) {
		for (grid.y = 0; grid.y < c->height; ++grid.y) {
			for (feat = square_feat(c, grid); feat; feat = feat->next) {
				count++;
			}
		}
	}

	if (c->name) {
		strnfmt(c_name, sizeof c_name, "%s", c->name);
	}
	else if (c == cave) {
		strnfmt(c_name, sizeof c_name, "cave");
	}
	else {
		strnfmt(c_name, sizeof c_name, "chunk of depth %i", c->depth);
	}

	dbg_log_fmt("feat", " - chunk %s: %i feats", c_name, count);

	return count;
}

void chunk_list_feat_log(void)
{
	int total = 0, chunk_id;

	dbg_log_fmt("feat", "\nfeats on turn %i:", turn);

	for (chunk_id = 0; chunk_id < chunk_list_max; chunk_id++) {
		total += chunk_feat_log(chunk_list[chunk_id]);
	}

	total += chunk_feat_log(cave);

	dbg_log_fmt("feat", "\ntotal number of existing feats = %i\n\n", total);
}

#endif


