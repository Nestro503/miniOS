//
// Created by Erwan on 27/11/2025.
//

#include "scheduler.h"
#include "../trace/logger.h"

Scheduler global_scheduler;



static const char *state_to_str(ProcessState s) {
    switch (s) {
        case NEW:        return "NEW";
        case READY:      return "READY";
        case RUNNING:    return "RUNNING";
        case BLOCKED:    return "BLOCKED";
        case TERMINATED: return "TERMINATED";
        default:         return "";
    }
}

static int current_cpu_id(void) {
    // mono-cœur : toujours 0
    return (global_scheduler.current != NULL) ? 0 : -1;
}



void pcb_queue_init(PCBQueue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
}

void pcb_queue_up(PCBQueue *q, PCB *p) {
    if (!p) return;
    p->next = NULL;  // important, on casse tout ancien chaînage

    if (q->tail == NULL) {
        // queue vide
        q->head = p;
        q->tail = p;
    } else {
        q->tail->next = p;
        q->tail = p;
    }
    q->size++;
}


PCB *pcb_queue_give(PCBQueue *q) {
    if (q->head == NULL) {
        return NULL;
    }

    PCB *p = q->head;
    q->head = p->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    p->next = NULL;
    q->size--;
    return p;
}


bool pcb_queue_empty(PCBQueue *q) {
    return (q->head == NULL);
}



void scheduler_init(SchedulingPolicy policy, int rr_time_quantum) {
    global_scheduler.policy = policy;
    global_scheduler.current_time = 0;
    global_scheduler.rr_time_quantum = rr_time_quantum;
    global_scheduler.current = NULL;

    for (int i = 0; i < NUM_PRIORITIES; ++i) {
        pcb_queue_init(&global_scheduler.ready_queues[i]);
    }
    pcb_queue_init(&global_scheduler.blocked_queue);
    pcb_queue_init(&global_scheduler.terminated_queue);

    global_scheduler.context_switches = 0;
    global_scheduler.total_processes = 0;
}



void scheduler_add_ready(PCB *p) {
    if (!p) return;

    p->state = READY;

    /* Sélection de la file READY cible selon la politique */
    int queue_index;

    if (global_scheduler.policy == SCHED_ROUND_ROBIN) {
        // RR : une seule file, on force tout le monde en MEDIUM
        queue_index = PRIORITY_MEDIUM;
    } else {
        // PRIORITY ou SCHED_P_RR : on respecte la priorité du PCB
        queue_index = p->priority;
    }

    pcb_queue_up(&global_scheduler.ready_queues[queue_index], p);

    /* Trace du passage en READY */
    trace_event(
        global_scheduler.current_time,
        p->pid,
        "STATE_CHANGE",
        "READY",
        "",      // pas de raison particulière
        -1,      // pas sur CPU, juste dans une file
        "READY"
    );

    /* ================================
       PRÉEMPTION — SCHED_PRIORITY
       ================================ */
    if (global_scheduler.policy == SCHED_PRIORITY && global_scheduler.current != NULL) {

        PCB *current = global_scheduler.current;

        // si le nouveau READY a une priorité strictement supérieure à celle du courant
        if (p->priority > current->priority) {
            // 1. Le processus courant redevient READY
            current->state = READY;
            pcb_queue_up(&global_scheduler.ready_queues[current->priority], current);

            // 2. Libère le CPU
            global_scheduler.current = NULL;

            // 3. On compte le changement de contexte
            global_scheduler.context_switches++;
        }
    }
}







PCB *scheduler_pick_next(void) {
    PCB *next = NULL;

    switch (global_scheduler.policy) {

        case SCHED_ROUND_ROBIN:
            // une seule file, priorité MEDIUM
                next = pcb_queue_give(&global_scheduler.ready_queues[PRIORITY_MEDIUM]);
        break;

        case SCHED_PRIORITY:
        case SCHED_P_RR:
            // HIGH -> MEDIUM -> LOW
            for (int pr = PRIORITY_HIGH; pr >= PRIORITY_LOW; --pr) {
                if (!pcb_queue_empty(&global_scheduler.ready_queues[pr])) {
                    next = pcb_queue_give(&global_scheduler.ready_queues[pr]);
                    break;
                }
            }
        break;
    }

    if (next != NULL) {
        next->state = RUNNING;

        // Si c'est la première fois qu'on le planifie, on fixe start_time
        if (next->start_time == -1) {
            next->start_time = global_scheduler.current_time;
        }

        global_scheduler.current = next;
        global_scheduler.context_switches++;

        // Trace : passage en RUNNING sur le CPU
        trace_event(
            global_scheduler.current_time,
            next->pid,
            "STATE_CHANGE",
            "RUNNING",
            "",
            0,          // id CPU (mono-cœur)
            "CPU"
        );
    } else {
        global_scheduler.current = NULL;
    }

    return next;
}





void scheduler_block(PCB *p, const char *reason, const char *queue_label) {
    if (!p) return;

    // Passage à l'état BLOQUÉ
    p->state = BLOCKED;
    pcb_queue_up(&global_scheduler.blocked_queue, p);

    // Log de l'événement de blocage
    // NOTE : "io_or_lock" est un placeholder. Plus tard tu pourras
    // appeler cette fonction avec un "reason" plus précis (io, mutex, semaphore)
    trace_event(
        global_scheduler.current_time, // time
        p->pid,                        // pid
        "STATE_CHANGE",                // event
        "BLOCKED",                     // state
        "io_or_lock",                  // reason (à affiner plus tard)
        -1,                            // cpu (pas sur CPU, en attente)
        "BLOCKED"                      // queue
    );

    // Si c'était le processus courant, le CPU devient libre
    if (global_scheduler.current == p) {
        global_scheduler.current = NULL;
    }
}




void scheduler_terminate(PCB *p) {
    if (!p) return;

    // Mise à jour de l'état et des stats
    p->state = TERMINATED;
    p->finish_time = global_scheduler.current_time;

    // Ajout dans la file des terminés
    pcb_queue_up(&global_scheduler.terminated_queue, p);

    // Trace CSV
    trace_event(
        global_scheduler.current_time, // time
        p->pid,                        // pid
        "TERMINATED",                  // event
        "TERMINATED",                  // state
        "",                            // reason (aucune raison particulière)
        -1,                            // cpu (plus sur CPU)
        "TERM"                         // queue (file des terminés)
    );

    // Libérer le CPU si c'était le process courant
    if (global_scheduler.current == p) {
        global_scheduler.current = NULL;
    }
}





bool scheduler_is_finished(void) {
    // Simple : si tous les process créés sont dans terminated_queue
    return (global_scheduler.terminated_queue.size == global_scheduler.total_processes);
}

void scheduler_tick(void) {
    // Avance l'horloge globale
    global_scheduler.current_time++;

    // 1) Gérer le processus courant (s'il y en a un)
    PCB *p = global_scheduler.current;

    if (p != NULL) {
        // Le processus courant consomme du CPU
        p->remaining_time--;
        p->last_run_time = global_scheduler.current_time;

        // S'il finit son burst CPU
        if (p->remaining_time <= 0) {
            scheduler_terminate(p);
        }
        // Sinon : pour l'instant, pas de logique de quantum / préemption ici
    }

    // 2) Gérer les réveils I/O des BLOCKED
    int nb_blocked = global_scheduler.blocked_queue.size;
    for (int i = 0; i < nb_blocked; ++i) {
        PCB *b = pcb_queue_give(&global_scheduler.blocked_queue);
        if (b->blocked_until <= global_scheduler.current_time) {
            // Il peut être réveillé
            b->waiting_for_io = false;
            b->state = READY;

            // Trace de l'événement UNBLOCKED -> READY (raison : io)
            trace_event(
                global_scheduler.current_time,
                b->pid,
                "UNBLOCKED",
                "READY",
                "io",
                -1,        // pas sur CPU
                "READY"    // retourne dans une file READY
            );

            // On le remet dans les files READY
            scheduler_add_ready(b);
        } else {
            // Il reste bloqué, on le remet dans la file BLOCKED
            pcb_queue_up(&global_scheduler.blocked_queue, b);
        }
    }

    // 3) Si plus de process courant, choisir le suivant
    if (global_scheduler.current == NULL) {
        scheduler_pick_next();
    }
}

