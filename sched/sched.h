#pragma once
#include <stdint.h>

#define TASK_DEAD     0
#define TASK_READY    1
#define TASK_RUNNING  2
#define TASK_SLEEPING 3

void sched_init(void);
uint32_t task_create(void (*entry)(void), uint32_t priority);
void task_exit(void);
void task_yield(void);
void task_sleep(uint64_t ms);
void schedule(void);
void timer_tick(void);
uint32_t get_current_tid(void);
void debug_sched_state(void);
