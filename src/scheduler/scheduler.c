//
// Created by Erwan on 27/11/2025.
//

#include "scheduler.h"

Scheduler global_scheduler;


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

    // Politique RR : on ignore la priorité, tout le monde en MEDIUM
    if (global_scheduler.policy == SCHED_ROUND_ROBIN) {
        pcb_queue_up(&global_scheduler.ready_queues[PRIORITY_MEDIUM], p);
    }
    // Politique priorité ou P_RR : on respecte la prio du PCB
    else {
        pcb_queue_up(&global_scheduler.ready_queues[p->priority], p);
    }

    // ================================
    //  PRÉEMPTION — SCHED_PRIORITY
    // ================================
    if (global_scheduler.policy == SCHED_PRIORITY && global_scheduler.current != NULL) {

        PCB *current = global_scheduler.current;

        // si le nouveau READY a une priorité strictement supérieure
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

        case SCHED_PRIORITY: // pas de break donc c'est la meme chose pour prio et hybride
        case SCHED_P_RR:
            // même logique de choix entre prios :
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
    } else {
        global_scheduler.current = NULL;
    }


    return next;
}




void scheduler_block(PCB *p) {
    if (!p) return;

    p->state = BLOCKED;
    pcb_queue_up(&global_scheduler.blocked_queue, p);

    if (global_scheduler.current == p) {
        global_scheduler.current = NULL;
    }
}


void scheduler_terminate(PCB *p) {
    if (!p) return;

    p->state = TERMINATED;
    p->finish_time = global_scheduler.current_time;
    pcb_queue_up(&global_scheduler.terminated_queue, p);

    if (global_scheduler.current == p) {
        global_scheduler.current = NULL;
    }
}




bool scheduler_is_finished(void) {
    // Simple : si tous les process créés sont dans terminated_queue
    return (global_scheduler.terminated_queue.size == global_scheduler.total_processes);
}

void scheduler_tick(void) {
    global_scheduler.current_time++;

    // 1) gérer le processus courant (si il y en a un)
    PCB *p = global_scheduler.current;

    if (p != NULL) {
        // il consomme du CPU
        p->remaining_time--;
        p->last_run_time = global_scheduler.current_time;

        // s'il finit son burst CPU
        if (p->remaining_time <= 0) {
            scheduler_terminate(p);
        }
        // sinon : pour l'instant, RR non préemptif / priorité préemptive
        // la vraie logique de préemption entre prios viendra après
    }

    // 2) gérer les réveils I/O des BLOCKED
    int nb_blocked = global_scheduler.blocked_queue.size;
    for (int i = 0; i < nb_blocked; ++i) {
        PCB *b = pcb_queue_give(&global_scheduler.blocked_queue);
        if (b->blocked_until <= global_scheduler.current_time) {
            b->waiting_for_io = false;
            scheduler_add_ready(b);
        } else {
            // il reste bloqué, on le remet dans blocked_queue
            pcb_queue_up(&global_scheduler.blocked_queue, b);
        }
    }

    // 3) si plus de process courant, choisir le suivant
    if (global_scheduler.current == NULL) {
        scheduler_pick_next();
    }
}
