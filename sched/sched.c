#include <stdint.h>
#include <stddef.h>
#include <../vm_pages.h>
#include <sched.h>

#define MAX_TASKS 64
#define STACK_SIZE 8192
#define TIME_SLICE_MS 10
#define IDLE_TASK_PRIORITY 0

struct task_context {
    uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30;
    uint64_t sp;
};

struct task {
    uint32_t tid;
    uint32_t state;
    uint32_t priority;
    uint32_t time_slice;
    uint64_t stack_base;
    uint64_t sleep_until;
    struct task_context ctx;
    struct task* next;
};

static struct task tasks[MAX_TASKS];
static struct task* current_task = NULL;
static struct task* ready_head = NULL;
static struct task* idle_task = NULL;
static uint32_t next_tid = 1;
static uint64_t tick_count = 0;

extern void context_switch(struct task_context* old_ctx, struct task_context* new_ctx);
extern uint64_t get_timer_ticks(void);
extern void kernel_panic(const char* msg);

static void idle_loop(void) {
    while(1) {
        asm volatile("wfi");
    }
}

static void enqueue_task(struct task* task) {
    if (!ready_head) {
        ready_head = task;
        task->next = task;
    } else {
        task->next = ready_head->next;
        ready_head->next = task;
    }
}

static struct task* dequeue_task(void) {
    if (!ready_head) return NULL;
    
    struct task* task = ready_head->next;
    if (task == ready_head) {
        ready_head = NULL;
    } else {
        ready_head->next = task->next;
    }
    task->next = NULL;
    return task;
}

static struct task* alloc_task(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) {
            return &tasks[i];
        }
    }
    return NULL;
}

void sched_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_DEAD;
        tasks[i].tid = 0;
        tasks[i].next = NULL;
    }
    
    idle_task = &tasks[0];
    idle_task->tid = 0;
    idle_task->state = TASK_READY;
    idle_task->priority = IDLE_TASK_PRIORITY;
    idle_task->time_slice = 1;
    idle_task->stack_base = alloc_page();
    if (!idle_task->stack_base) {
        kernel_panic("failed to alloc idle stack");
    }
    
    idle_task->ctx.sp = idle_task->stack_base + STACK_SIZE - 16;
    idle_task->ctx.x30 = (uint64_t)idle_loop;
    
    current_task = idle_task;
    tick_count = 0;
}

uint32_t task_create(void (*entry)(void), uint32_t priority) {
    struct task* task = alloc_task();
    if (!task) return 0;
    
    task->tid = next_tid++;
    task->state = TASK_READY;
    task->priority = priority;
    task->time_slice = priority + 1;
    task->sleep_until = 0;
    task->stack_base = alloc_page();
    
    if (!task->stack_base) {
        task->state = TASK_DEAD;
        return 0;
    }
    
    task->ctx.sp = task->stack_base + STACK_SIZE - 16;
    task->ctx.x30 = (uint64_t)entry;
    
    enqueue_task(task);
    return task->tid;
}

void task_exit(void) {
    if (current_task == idle_task) return;
    
    current_task->state = TASK_DEAD;
    free_page(current_task->stack_base);
    current_task = NULL;
    
    schedule();
}

void task_yield(void) {
    if (current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
        enqueue_task(current_task);
    }
    schedule();
}

void task_sleep(uint64_t ms) {
    if (current_task == idle_task) return;
    
    current_task->state = TASK_SLEEPING;
    current_task->sleep_until = tick_count + ms;
    schedule();
}

void schedule(void) {
    struct task* next = NULL;
    
    
    tick_count = get_timer_ticks();
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && 
            tasks[i].sleep_until <= tick_count) {
            tasks[i].state = TASK_READY;
            enqueue_task(&tasks[i]);
        }
    }
    
    struct task* best = NULL;
    uint32_t best_priority = 0;
    
    if (ready_head) {
        struct task* curr = ready_head;
        do {
            if (curr->priority > best_priority) {
                best = curr;
                best_priority = curr->priority;
            }
            curr = curr->next;
        } while (curr != ready_head);
    }
    
    next = best ? best : idle_task;
    
    if (next != current_task) {
        struct task* prev = current_task;
        current_task = next;
        
        if (next != idle_task) {
            if (ready_head == next) {
                if (next->next == next) {
                    ready_head = NULL;
                } else {
                    struct task* curr = ready_head;
                    while (curr->next != next) curr = curr->next;
                    curr->next = next->next;
                    ready_head = next->next;
                }
            } else {
                struct task* curr = ready_head;
                while (curr->next != next) curr = curr->next;
                curr->next = next->next;
            }
            next->next = NULL;
            next->state = TASK_RUNNING;
        }
        
        context_switch(&prev->ctx, &next->ctx);
    }
}

void timer_tick(void) {
    tick_count++;
    
    if (current_task != idle_task) {
        current_task->time_slice--;
        if (current_task->time_slice == 0) {
            current_task->time_slice = current_task->priority + 1;
            if (current_task->state == TASK_RUNNING) {
                current_task->state = TASK_READY;
                enqueue_task(current_task);
            }
            schedule();
        }
    }
}

uint32_t get_current_tid(void) {
    return current_task ? current_task->tid : 0;
}

void debug_sched_state(void) {
}
