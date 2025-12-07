#include <stdio.h>
#include <stdlib.h>

#include "src/process/process.h"
#include "src/scheduler/scheduler.h"
#include "src/trace/logger.h"
#include "src/menu/menu.h"

#define NB_TASKS 3

int main(void) {

    /* 1) Menu utilisateur */
    SchedulingPolicy policy = menu_choose_policy();

    /* 2) Init logger et scheduler */
    trace_init("trace.csv");
    scheduler_init(policy, 0);

    /* 3) Création de tâches (exemple) */
    PCB *tasks[NB_TASKS];

    tasks[0] = process_create(PRIORITY_HIGH,   5, 0);
    tasks[1] = process_create(PRIORITY_MEDIUM, 3, 2);
    tasks[2] = process_create(PRIORITY_LOW,    4, 4);

    /* 4) Simulation */
    while (!scheduler_is_finished()) {

        for (int i = 0; i < NB_TASKS; ++i) {
            PCB *p = tasks[i];
            if (p->state == NEW && p->arrival_time <= global_scheduler.current_time) {
                scheduler_add_ready(p);
            }
        }

        scheduler_tick();
    }

    /* 5) Fin */
    trace_close();

    printf("Simulation terminee au temps = %d\n", global_scheduler.current_time);

    for (int i = 0; i < NB_TASKS; ++i) {
        free(tasks[i]);
    }

    return 0;
}
