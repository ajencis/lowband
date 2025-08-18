#include "angband.h"
#include "cave.h"



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
        terrain_elem_remove_all(&player->cave->squares[grid.y][grid.x].t_elem);
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



bool terrain_element_add(struct terrain_element **list, uint16_t idx, uint16_t timer)
{
    struct terrain_element *t_elem = terrain_element_new(timer, idx);

    t_elem->next = *list;
    *list = t_elem;

    return true;
}

bool terrain_elem_remove(struct terrain_element **list, uint16_t idx)
{
    struct terrain_element *t_elem, **prev;

    for (prev = list, t_elem = *prev; t_elem; prev = &t_elem->next, t_elem = *prev) {
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
bool terrain_element_increase_dur(struct square *sq, uint16_t idx, uint16_t change)
{
    struct terrain_element *t_elem;

    for (t_elem = sq->t_elem; t_elem; t_elem = t_elem->next) {
        if (t_elem->kind->idx == idx) {
            change = MAX(0, MIN(change, UINT16_MAX - t_elem->timer));
            t_elem->timer += change;
            return false;
        }
    }

    return terrain_element_add(&sq->t_elem, idx, change);
}

/**
 * Reduces the duration of the terrain element by  change , and removes it if it would
 * reduce to 0 or lower.
 * Returns  true  if the terrain element was removed
 */
bool terrain_element_reduce_dur(struct square *sq, uint16_t idx, uint16_t change)
{
    struct terrain_element *t_elem;

    for (t_elem = sq->t_elem; t_elem; t_elem = t_elem->next) {
        if (t_elem->kind->idx == idx) {
            if (change >= t_elem->timer) {
                return terrain_elem_remove(&sq->t_elem, idx);
            }

            t_elem->timer -= change;
            return false;
        }
    }

    return false;
}

bool terrain_element_change_dur(struct square *sq, uint16_t idx, int change)
{
    if (change < 0) return terrain_element_reduce_dur(sq, idx, (unsigned)(-change));
    if (change > 0) return terrain_element_increase_dur(sq, idx, (unsigned)change);

    return false;
}

bool terrain_elem_remove_all(struct terrain_element **list)
{
    struct terrain_element *curr, *next;
    bool did_something = false;

    for (curr = *list; curr; curr = next) {
        next = curr->next;
        did_something = true;

        terrain_elem_free(curr);
    }

    *list = NULL;

    return did_something;
}
