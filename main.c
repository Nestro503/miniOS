#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "src/process/process.h"
#include "src/process/scenario.h"
#include "src/scheduler/scheduler.h"
#include "src/trace/logger.h"
#include "src/menu/menu.h"
#include "src/memory/memory.h"
#include "src/io/io.h"

#define MAX_TASKS 32

int main(void) {
    /* 1) Choix de la politique d'ordonnancement */
    SchedulingPolicy policy = menu_choose_policy();

    int quantum = 0;
    if (policy == SCHED_ROUND_ROBIN || policy == SCHED_P_RR) {
        // suppose que menu_choose_quantum existe, sinon remplace par un quantum fixe
        quantum = menu_choose_quantum();
    }

    /* 2) Initialisations globales */
    memory_init();                                      // heap simulé 64 MiB
    io_init();                                          // module I/O
    trace_init("../tools/trace/trace.csv");             // fichier de trace
    scheduler_init(policy, quantum);                    // scheduler

    /* 3) Construction du scénario interactif (processus utilisateur) */
    PCB *tasks[MAX_TASKS];
    int nb_tasks = scenario_build_interactive(tasks, MAX_TASKS, policy);

    /* 3 bis) Affichage du heap AVANT l'exécution (avant les free) */
    memory_dump_with_processes(tasks, nb_tasks);

    /* 4) Boucle de simulation */
    while (!scheduler_is_finished()) {

        // 1) Admission des processus
        for (int i = 0; i < nb_tasks; ++i) {
            PCB *p = tasks[i];
            if (p->state == NEW &&
                p->arrival_time <= global_scheduler.current_time) {
                scheduler_add_ready(p);
                }
        }

        // 2) Processus courant
        PCB *cur = global_scheduler.current;

        // 3) Déclenchement éventuel d'une I/O
        if (cur &&
            cur->io_device   != -1 &&
            cur->io_duration > 0 &&
            !cur->waiting_for_io &&
            global_scheduler.current_time >= cur->io_start_time)
        {
            io_request(cur,
                       (io_device_t)cur->io_device,
                       (uint32_t)cur->io_duration,
                       (uint32_t)global_scheduler.current_time);
        }


        // 4) Avance d'un tick
        scheduler_tick();
    }



    /* 6) Fin de simulation */
    trace_close();
    printf("Simulation terminee au temps = %d\n",
           global_scheduler.current_time);

    /* Optionnel : état final de la mémoire simulée */
    /* État final de la mémoire simulée, segmenté par processus */
    memory_dump_with_processes(tasks, nb_tasks);

    /* Libération des PCB */
    for (int i = 0; i < nb_tasks; ++i) {
        free(tasks[i]);
    }

    return 0;
}
