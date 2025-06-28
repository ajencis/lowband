

#ifndef INCLUDED_MON_CALCS_H
#define INCLUDED_MON_CALCS_H

int calc_mon_race_power(const struct monster_race *mr, int power);
int mon_lev(const struct monster *mon);

void calc_mon_bonuses(struct monster *mon, struct player_state *state);
void update_mon_state(struct monster *mon);
void update_mon_attacks(struct monster *mon);
void free_mon_attacks(struct monster *mon);

#endif
