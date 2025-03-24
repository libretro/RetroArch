/*
 * ctr_bottom_states.h
 */

#ifndef CTR_BOTTOM_STATES_H_
#define CTR_BOTTOM_STATES_H_


extern void ctr_update_state_path(void *data);
extern void ctr_update_state_date(void *data);
extern void save_state_to_file();
extern bool ctr_update_state_date_from_file(void *data);

#endif /* CTR_BOTTOM_STATES_H_ */

