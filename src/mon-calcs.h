

#ifndef INCLUDED_MON_CALCS_H
#define INCLUDED_MON_CALCS_H

int mon_power(const struct monster_race *mon, int power);
bool mon_power_minimum(const struct monster *mon, int power, int min);
int get_mon_power_scale(const struct monster *mon, int power, int scaleto);
bool mon_has_power(const struct monster *mon, int power);

void calc_mon_bonuses(struct monster *mon, struct player_state *state);
void update_mon_state(struct monster *mon);
void update_mon_attacks(struct monster *mon);
void free_mon_attacks(struct monster *mon);

#endif
