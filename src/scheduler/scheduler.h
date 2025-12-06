
#ifndef MINIOS_SCHEDULER_H
#define MINIOS_SCHEDULER_H

#include <stdbool.h>
#include "../process/process.h"

#define NUM_PRIORITIES 3

typedef enum {
    SCHED_ROUND_ROBIN = 0,        // RR (non préemptif)
    SCHED_PRIORITY,      // Priorité statique (préemptif)
    SCHED_P_RR        // Priorité + RR (préemptif entre priorités)
} SchedulingPolicy;



typedef struct PCBQueue {
    PCB *head;
    PCB *tail;
    int  size;
} PCBQueue;



typedef struct Scheduler {
    SchedulingPolicy policy;     // RR / PRIORITY / HYBRID

    int current_time;   // horloge logique globale
    int rr_time_quantum;   // quantum (utilisé pour RR & HYBRID)

    // Processus couramment en cours d’exécution (RUNNING) ou NULL si CPU idle
    PCB *current;

    // Files READY par priorité (on les utilise ou pas selon la politique)
    PCBQueue ready_queues[NUM_PRIORITIES];

    // File BLOCKED (tous les processus en attente I/O/mutex/semaphore)
    PCBQueue blocked_queue;

    // File TERMINATED (pour stats + traces)
    PCBQueue terminated_queue;

    // Statistiques basiques
    int context_switches;
    int total_processes;

    
} Scheduler;


// Variable globale
extern Scheduler global_scheduler;


// Gestion de la queue
void pcb_queue_init(PCBQueue *q);   // Init de la queue
void pcb_queue_up(PCBQueue *q, PCB *p); // Ajoute un PCB (fin de file car FIFO)
PCB *pcb_queue_give(PCBQueue *q);  // Donne le premier PCB de la file au CPU
bool pcb_queue_empty(PCBQueue *q); // Test si queue empty


// Scheduler API

void scheduler_init(SchedulingPolicy policy, int rr_time_quantum); // init du scheduler, choix quantum, CHOIX DE LA POLITIQUE...
void scheduler_add_ready(PCB *p); // Passage à l'état ready (utile pour préemption)
void scheduler_block(PCB *p); // Bloque le processus (I/O, Semaphore, MUTEX)
void scheduler_terminate(PCB *p); // Fin d'un process

void scheduler_tick(void);
PCB *scheduler_pick_next(void);  // Choisi le prochain process à RUNNING en fct de la politique actuelle
bool scheduler_is_finished(void); // Tout les process FINISHED

#endif //MINIOS_SCHEDULER_H
