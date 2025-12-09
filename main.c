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

    /* 4) Boucle de simulation */
    while (!scheduler_is_finished()) {

        /* Admission des processus qui arrivent à ce tick */
        for (int i = 0; i < nb_tasks; ++i) {
            PCB *p = tasks[i];
            if (p->state == NEW &&
                p->arrival_time <= global_scheduler.current_time) {
                scheduler_add_ready(p);
            }
        }

        /* Processus courant éventuel */
        PCB *cur = global_scheduler.current;

        /* Exemple de déclenchement d'I/O :
         * - seulement si le process a un io_device valide
         * - n'est pas déjà en attente I/O
         * - au tick 3 (tu peux adapter la condition)
         */
        if (cur &&
            cur->io_device != -1 &&
            !cur->waiting_for_io &&
            global_scheduler.current_time == 3)
        {
            io_request(cur,
                       (io_device_t)cur->io_device,
                       3,  // durée de l'I/O
                       (uint32_t)global_scheduler.current_time);
        }

        /* 5) Un tick du scheduler :
         *  - consomme du CPU sur RUNNING
         *  - gère les réveils I/O via blocked_until dans scheduler_tick()
         *  - choisit un nouveau RUNNING si besoin
         */
        scheduler_tick();
    }

    /* 6) Fin de simulation */
    trace_close();
    printf("Simulation terminee au temps = %d\n",
           global_scheduler.current_time);

    /* Optionnel : état final de la mémoire simulée */
    memory_dump();

    /* Libération des PCB */
    for (int i = 0; i < nb_tasks; ++i) {
        free(tasks[i]);
    }

    return 0;
}
