#include <stdlib.h>
#include "process.h"
#include "../scheduler/scheduler.h"
#include "../trace/logger.h"


static int next_pid = 1;  // compteur de PID

PCB *process_create(ProcessPriority priority, int burst_time, int arrival_time) {
    PCB *p = malloc(sizeof(PCB));
    if (!p) {
        return NULL; // à améliorer si besoin
    }

    // IDENTIFICATION
    p->pid = next_pid++;
    p->priority = priority;

    // ÉTAT INITIAL
    p->state = NEW;

    // TEMPS
    p->arrival_time   = arrival_time;
    p->start_time     = -1;          // pas encore passé en RUNNING
    p->finish_time    = -1;          // pas encore terminé
    p->remaining_time = burst_time;  // durée CPU simulée
    p->last_run_time  = -1;
    p->quantum_remaining = 0;        // pas utilisé pour l’instant

    // CONTEXTE (simulé)
    p->stack   = NULL;
    p->context = NULL;

    // MÉMOIRE ALLOUÉE
    p->allocations    = NULL;
    p->alloc_count    = 0;
    p->alloc_capacity = 0;

    // I/O BLOQUANTES
    p->blocked_until = -1;
    p->waiting_for_io = false;

    // SYNCHRONISATION
    p->waiting_on_mutex     = NULL;
    p->waiting_on_semaphore = NULL;

    // CHAÎNAGE
    p->next = NULL;

    // Stat globale : un processus de plus
    global_scheduler.total_processes++;



    // Trace de création (on loggue l’arrivée logique, pas le moment réel d'admission)
    trace_event(
        global_scheduler.current_time,  // ou arrival_time si tu préfères
        p->pid,
        "CREATE",
        "NEW",
        "",
        -1,
        "NEW"
    );


    return p;
}
