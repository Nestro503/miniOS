//
// Created by Erwan on 27/11/2025.
//

#include "scheduler.h"
#include "../trace/logger.h"
#include "../io/io.h"
#include "../memory/memory.h"   // <-- adapte le chemin/nom si besoin

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

/* ===================================================================== */
/*                            FILES DE PCB                                */
/* ===================================================================== */

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

/* ===================================================================== */
/*                         INITIALISATION SCHEDULER                      */
/* ===================================================================== */

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

/* ===================================================================== */
/*                           AJOUT EN READY                              */
/* ===================================================================== */

void scheduler_add_ready(PCB *p) {
    if (!p) return;

    /* Passage en état READY */
    p->state = READY;

    /* Déterminer la file READY selon la politique */
    int queue_index;

    if (global_scheduler.policy == SCHED_ROUND_ROBIN) {
        // RR : une seule file (MEDIUM)
        queue_index = PRIORITY_MEDIUM;
    } else {
        // PRIORITY / P_RR : on respecte la priorité du PCB
        queue_index = p->priority;
    }

    /* On place le processus dans la file READY choisie */
    pcb_queue_up(&global_scheduler.ready_queues[queue_index], p);

    /* Log : entrée en READY */
    trace_event(
        global_scheduler.current_time,
        p->pid,
        "STATE_CHANGE",
        "READY",
        "",
        -1,
        "READY"
    );

    /* =====================================================
       PRÉEMPTION — SCHED_PRIORITY / SCHED_P_RR
       Si un processus de plus haute priorité arrive, il
       peut préempter le processus courant.
       ===================================================== */
    if ((global_scheduler.policy == SCHED_PRIORITY ||
         global_scheduler.policy == SCHED_P_RR) &&
        global_scheduler.current != NULL)
    {
        PCB *current = global_scheduler.current;

        /* Si le nouveau READY a une priorité strictement supérieure */
        if (p->priority > current->priority) {

            /* Le processus courant redevient READY */
            current->state = READY;
            pcb_queue_up(&global_scheduler.ready_queues[current->priority], current);

            /* Le CPU est libéré */
            global_scheduler.current = NULL;

            /* Log de la préemption */
            trace_event(
            global_scheduler.current_time,
            current->pid,
            "PREEMPTED",
            "READY",
            "higher_priority_arrived",
            -1,
            "READY"
            );
        }
    }
}

/* ===================================================================== */
/*                       SÉLECTION DU PROCHAIN PROCESS                   */
/* ===================================================================== */

PCB *scheduler_pick_next(void) {
    PCB *next = NULL;

    switch (global_scheduler.policy) {

        case SCHED_ROUND_ROBIN:
            // Une seule file READY : on utilise la file PRIORITY_MEDIUM
            next = pcb_queue_give(&global_scheduler.ready_queues[PRIORITY_MEDIUM]);
            break;

        case SCHED_PRIORITY:
        case SCHED_P_RR:
            // Politique à priorites : on cherche d'abord HIGH, puis MEDIUM, puis LOW
            for (int pr = PRIORITY_HIGH; pr >= PRIORITY_LOW; --pr) {
                if (!pcb_queue_empty(&global_scheduler.ready_queues[pr])) {
                    next = pcb_queue_give(&global_scheduler.ready_queues[pr]);
                    break;
                }
            }
            break;

        default:
            break;
    }

    if (next != NULL) {
        next->state = RUNNING;

        // Si c'est la première fois qu'on le planifie, on fixe start_time
        if (next->start_time == -1) {
            next->start_time = global_scheduler.current_time;
        }

        // Gestion du quantum :
        // on NE LE RECHARGE QUE s'il est épuisé ou non initialisé
        if (global_scheduler.policy == SCHED_ROUND_ROBIN ||
            global_scheduler.policy == SCHED_P_RR)
        {
            if (next->quantum_remaining <= 0) {
                next->quantum_remaining = global_scheduler.rr_time_quantum;
            }

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

/* ===================================================================== */
/*                            BLOQUAGE PROCESS                           */
/* ===================================================================== */

void scheduler_block(PCB *p, const char *reason, const char *queue_label) {
    if (!p) return;

    // Passage à l'état BLOQUÉ
    p->state = BLOCKED;
    pcb_queue_up(&global_scheduler.blocked_queue, p);

    // Log de l'événement de blocage
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

/* ===================================================================== */
/*                           TERMINAISON PROCESS                         */
/* ===================================================================== */

void scheduler_terminate(PCB *p) {
    if (!p) return;

    // Libération de la mémoire principale du process sur le heap simulé
    if (p->mem_base != NULL && p->mem_size > 0) {
        mini_free(p->mem_base);      // remplace par le vrai nom du free si besoin
        p->mem_base = NULL;
        p->mem_size = 0;
    }

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
        "",                            // reason
        -1,                            // cpu (plus sur CPU)
        "TERM"                         // queue (file des terminés)
    );

    // Libérer le CPU si c'était le process courant
    if (global_scheduler.current == p) {
        global_scheduler.current = NULL;
    }
}



/* ===================================================================== */
/*                     TEST FIN DE SIMULATION                            */
/* ===================================================================== */

bool scheduler_is_finished(void) {
    // Simple : si tous les process créés sont dans terminated_queue
    return (global_scheduler.terminated_queue.size == global_scheduler.total_processes);
}

/* ===================================================================== */
/*                              TICK SCHEDULER                           */
/* ===================================================================== */

void scheduler_tick(void) {
    // Avance l'horloge globale
    global_scheduler.current_time++;

    // 1) Gérer le processus courant (s'il y en a un)
    PCB *p = global_scheduler.current;

    if (p != NULL) {

        /* =======================================================
           CAS 1 : Round Robin préemptif (SCHED_ROUND_ROBIN)
                 OU priorité + RR (SCHED_P_RR)
           ======================================================= */
        if (global_scheduler.policy == SCHED_ROUND_ROBIN ||
            global_scheduler.policy == SCHED_P_RR)
        {
            p->remaining_time--;
            p->quantum_remaining--;
            p->last_run_time = global_scheduler.current_time;

            // --- Fin du burst CPU ---
            if (p->remaining_time <= 0) {
                scheduler_terminate(p);
            }
            // --- Quantum expiré ---
            else if (p->quantum_remaining <= 0) {

                // Remettre le process en READY
                p->state = READY;

                if (global_scheduler.policy == SCHED_ROUND_ROBIN) {
                    // RR simple : une seule file
                    pcb_queue_up(&global_scheduler.ready_queues[PRIORITY_MEDIUM], p);
                } else {
                    // P_RR : file correspondant à sa priorité
                    pcb_queue_up(&global_scheduler.ready_queues[p->priority], p);
                }

                // CPU libre
                global_scheduler.current = NULL;

                // log spécifique au quantum
                trace_event(global_scheduler.current_time, p->pid,
                            "TIME_SLICE_EXPIRED", "READY", "", -1, "READY");
            }
        }
        /* =======================================================
           CAS 2 : PRIORITY (pas de quantum)
           ======================================================= */
        else {
            p->remaining_time--;
            p->last_run_time = global_scheduler.current_time;

            if (p->remaining_time <= 0) {
                scheduler_terminate(p);
            }
        }
    }

    /* =======================================================
       2) Gestion des réveils I/O dans la file BLOCKED
       ======================================================= */
    int nb_blocked = global_scheduler.blocked_queue.size;

    for (int i = 0; i < nb_blocked; ++i) {
        PCB *b = pcb_queue_give(&global_scheduler.blocked_queue);

        if (b->blocked_until <= global_scheduler.current_time) {

            /* Si ce processus était bloqué sur une I/O,
             * on prévient le module I/O pour libérer
             * mutex / sémaphore associés.
             */
            if (b->waiting_for_io && b->io_device >= 0) {
                io_release_resource_for(b);
            }

            b->state = READY;

            trace_event(
                global_scheduler.current_time,
                b->pid,
                "UNBLOCKED",
                "READY",
                "io",
                -1,
                "READY"
            );

            scheduler_add_ready(b);
        }
        else {
            /* Toujours bloqué : on le remet dans la file BLOCKED */
            pcb_queue_up(&global_scheduler.blocked_queue, b);
        }
    }

    /* =======================================================
       3) Si le CPU est libre, choisir un nouveau RUNNING
       ======================================================= */
    if (global_scheduler.current == NULL) {
        scheduler_pick_next();
    }
}
