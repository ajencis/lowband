#include "angband.h"
#include "cave.h"
#include "game-world.h"
#include "init.h"
#include "mon-desc.h"
#include "player-calcs.h"
#include "player-util.h"
#include "project.h"


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

static struct feature *feat_new(int fidx, int size)
{
	struct feature *new;

	assert(fidx >= FEAT_NONE && fidx < FEAT_MAX);

	if (size <= 0) return NULL;

	new = mem_zalloc(sizeof *new);

	new->kind = &f_info[fidx];
	new->size = size;

	assert(new->kind);
	assert(new->kind->name);

	return new;
}

static void feat_free(struct feature *feat)
{
	mem_free(feat);
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
 * default feats of  FEAT_NONE  get special-cased for the moment
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
		square_force_add_feat(c, grid, feat_default, 100);
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
		for (grid.y = 0; grid.y < c->width; ++grid.y) {
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

bool square_force_add_feat(struct chunk *c, struct loc grid, int fidx, int size)
{
	bool success;
	struct feature *feat, *next;

	assert(c);
	assert(square_in_bounds(c, grid));
	assert(fidx >= FEAT_NONE && fidx < FEAT_MAX);

	success = list_add_feat(&c->squares[grid.y][grid.x].feat, fidx, size);

	if (!success) {
		return false;
	}

	for (feat = square_feat(c, grid); feat; feat = next) {
		next = feat->next;

		if (feat_incompatible(fidx, feat->kind->fidx) == feat->kind->fidx) {
			square_force_remove_feat(c, grid, feat->kind->fidx);
		}
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

bool square_add_feat(struct chunk *c, struct loc grid, int fidx, int size)
{
	struct feature *feat;
	bool success;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		if (feat_incompatible(fidx, feat->kind->fidx) == fidx) {
			return false;
		}
	}

	success = square_force_add_feat(c, grid, fidx, size);

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

	result = square_add_feat(c, grid, fidx, size);

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
			square_force_add_feat(c, grid, new, feat->size);
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

static void square_set_feat_size(struct chunk *c, struct loc grid, int fidx, int size)
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

	square_add_feat(c, grid, fidx, size);
}

static void square_increase_feat_size(struct chunk *c, struct loc grid, int fidx, int amt)
{
	struct feature *feat = square_feat_by_type(c, grid, fidx);
	int curr = feat ? feat->size : 0;

	square_set_feat_size(c, grid, fidx, curr + amt);
}

static void square_reduce_feat_size(struct chunk *c, struct loc grid, int fidx, int amt)
{
	square_increase_feat_size(c, grid, fidx, -amt);
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
			square_add_feat(p->cave, grid, i, new[i]);
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

static void grid_feat_proj(struct chunk *c, struct loc grid, md_array *proj_amt)
{
	struct loc ogrid;
	struct feature *feat;
	int which, dist, amt;

	for (feat = square_feat(c, grid); feat; feat = feat->next) {
		which = feat->kind->proj;
		dist = feat->kind->proj_range;
		amt = feat->kind->proj_amt;

		if (which < 0) continue;

		for (ogrid.x = grid.x - dist; ogrid.x <= grid.x + dist; ++ogrid.x) {
			for (ogrid.y = grid.y - dist; ogrid.y <= grid.y + dist; ++ogrid.y) {
				if (!square_in_bounds_fully(c, ogrid)) continue;
				if (!square_isprojectable(c, ogrid)) continue;
				if (distance(grid, ogrid) > dist) continue;

				mda_element_add(proj_amt, amt, ogrid.y, ogrid.x, which);
			}
		}
	}
}

static void cave_feat_proj(struct chunk *c)
{
	md_array *proj_amt;
	struct loc grid;
	int which, amt, i;
	int flg = PROJECT_HIDE | PROJECT_JUMP | PROJECT_KILL | PROJECT_ITEM | PROJECT_GRID | PROJECT_PLAY;

	if (c != cave) return;

	proj_amt = mda_new(3, c->height, c->width, PROJ_MAX);

	for (i = 0; i < c->project_points->n; ++i) {
		grid_feat_proj(c, c->project_points->pts[i], proj_amt);
	}

	for (grid.x = 1; grid.x < c->width - 1; ++grid.x) {
		for (grid.y = 1; grid.y < c->height - 1; ++grid.y) {
			for (which = 0; which < PROJ_MAX; ++which) {
				amt = mda_element_get(proj_amt, grid.y, grid.x, which);

				if (amt > 0) {
					project(source_grid(grid), 0, grid, amt, which, flg, 0, 0, NULL);
				}
			}
		}
	}

	mda_free(proj_amt);
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

static bool cave_all_feats_valid(struct chunk *c)
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
	const char *c_name = c->name ? c->name : "(unnamed cave)";
	const char *type = c == cave ? "cave" : (c == player->cave ? "player->cave" : NULL);
	char type_str[128] = "";
	char error[128];
	va_list vp;

	assert(c);
	assert(fmt);

	va_start(vp, fmt);
	vstrnfmt(error, sizeof error, fmt, vp);
	va_end(vp);

	if (type) {
		strnfmt(type_str, sizeof type_str, " (%s)", type);
	}

	//msg_add_fmt("Error: chunk %s%s %s at grid (%i,%i)!",
	dbg_log_fmt("feat", "Error: chunk %s%s %s at grid (%i,%i)!",
	//plog_fmt("Error: chunk %s%s %s at grid (%i,%i)!",
			c_name,
			type_str,
			error,
			grid.x,
			grid.y);
}

bool square_feat_valid(struct chunk *c, struct loc grid)
{
	assert(c);

	struct feature *feat1, *feat2, *start = square_feat(c, grid);
	bool has_def, shld_def;

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

	if (c == cave) {
		assert(c->timeout_points);
		assert(c->spread_points);
		assert(c->produce_points);
		assert(c->project_points);
		if (!first_feat_meets_pred(c, grid, feat_times_out) && point_set_contains(c->timeout_points, grid)) {
			feat_valid_plog(c, grid, "has no timeout feat but grid is marked");
			return false;
		}
		if (!first_feat_meets_pred(c, grid, feat_spreads) && point_set_contains(c->spread_points, grid)) {
			feat_valid_plog(c, grid, "has no spread feat but grid is marked");
			return false;
		}
		if (!first_feat_meets_pred(c, grid, feat_produces) && point_set_contains(c->produce_points, grid)) {
			feat_valid_plog(c, grid, "has no produce feat but grid is marked");
			return false;
		}
		if (!first_feat_meets_pred(c, grid, feat_projects) && point_set_contains(c->project_points, grid)) {
			feat_valid_plog(c, grid, "has no project feat but grid is marked");
			return false;
		}
	}

	for (feat1 = start; feat1; feat1 = feat1->next) {
		if (feat1->size <= 0) {
			feat_valid_plog(c, grid, "has feat %s of size %i", feat1->kind->name, feat1->size);
			return false;
		}

		if (feat_times_out(feat1->kind->fidx) && !point_set_contains(c->timeout_points, grid)) {
			feat_valid_plog(c, grid, "has timeout feat %s but grid is not marked", feat1->kind->name);
			return false;
		}
		if (feat_spreads(feat1->kind->fidx) && !point_set_contains(c->spread_points, grid)) {
			feat_valid_plog(c, grid, "has spread feat %s but grid is not marked", feat1->kind->name);
			return false;
		}
		if (feat_produces(feat1->kind->fidx) && !point_set_contains(c->produce_points, grid)) {
			feat_valid_plog(c, grid, "has produce feat %s but grid is not marked", feat1->kind->name);
			return false;
		}
		if (feat_projects(feat1->kind->fidx) && !point_set_contains(c->project_points, grid)) {
			feat_valid_plog(c, grid, "has project feat %s but grid is not marked", feat1->kind->name);
			return false;
		}

		for (feat2 = feat1->next; feat2; feat2 = feat2->next) {
			if (feat_compare(feat1->kind->fidx, feat2->kind->fidx) > 0) {
				feat_valid_plog(c, grid, "has incorrectly ordered feats %s[prio %i] before %s[prio %i]",
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
	const struct feature_kind *feat = square_feat(c, grid)->kind;
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

	return;

	for (i = 0; i < times; ++i) {
		if (!(i % T_ELEM_PRODUCE_FREQ)) {
			cave_all_produce_t_elem(c);
		}

		t_elem_spread(c);
		t_elem_reduce_durations(c);
	}
}
