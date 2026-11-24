#ifndef PS2_AUDIO_MOCK_H
#define PS2_AUDIO_MOCK_H
unsigned mock_waits(void);
unsigned mock_alarms_rung(void);
unsigned mock_callbacks(void);
int      mock_sema_live(void);
void     mock_reset_counts(void);
void     mock_empty_ring(void);
void mock_iop_start(void);
void mock_iop_stop(void);
void mock_set_draining(int on);
void mock_alarm_join(void);
int  mock_fillbuf_threshold(void);
#endif
