

#ifndef INCLUDED_MON_CALCS_H
#define INCLUDED_MON_CALCS_H

int mon_power(const struct monster_race *mon, int power);

void calc_mon_bonuses(struct monster *mon, struct player_state *state);
void update_mon_state(struct monster *mon);

#endif
