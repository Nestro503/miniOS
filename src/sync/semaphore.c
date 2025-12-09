
#include "semaphore.h"
#include "../memory/memory.h"
#include "../scheduler/scheduler.h"
#include "../trace/logger.h"
#include "../trace/trace_event_types.h"

/* On réutilise le même helper que pour les mutex, mais en static ici aussi */
static void remove_from_blocked_queue(PCB* p) {
    PCBQueue* q = &global_scheduler.blocked_queue;
    PCB* prev = NULL;
    PCB* cur  = q->head;

    while (cur) {
        if (cur == p) {
            if (prev) {
                prev->next = cur->next;
            } else {
                q->head = cur->next;
            }
            if (q->tail == cur) {
                q->tail = prev;
            }
            cur->next = NULL;
            q->size--;
            break;
        }
        prev = cur;
        cur  = cur->next;
    }
}

static SemWaitNode* sem_alloc_node(PCB* p) {
    SemWaitNode* n = (SemWaitNode*) mini_malloc(sizeof(SemWaitNode));
    if (!n) return NULL;
    n->proc = p;
    n->next = NULL;
    return n;
}

static void sem_free_node(SemWaitNode* n) {
    mini_free(n);
}

/* Ajout en fin de file */
static void sem_queue_push(Semaphore* s, PCB* p) {
    SemWaitNode* n = sem_alloc_node(p);
    if (!n) return;

    if (!s->queue) {
        s->queue = n;
    } else {
        SemWaitNode* cur = s->queue;
        while (cur->next) cur = cur->next;
        cur->next = n;
    }
}

/* Retire et renvoie le premier en attente */
static PCB* sem_queue_pop(Semaphore* s) {
    if (!s->queue) return NULL;
    SemWaitNode* n = s->queue;
    s->queue = n->next;
    PCB* p = n->proc;
    sem_free_node(n);
    return p;
}

void semaphore_init(Semaphore* s, int initial) {
    if (!s) return;
    if (initial < 0) initial = 0;
    s->value = initial;
    s->queue = NULL;
}

void semaphore_wait(Semaphore* s, PCB* current) {
    if (!s) return;

    if (s->value > 0) {
        s->value--;
        if (current) {
            current->waiting_on_semaphore = s;
        }
        return;
    }

    // Si on n'a pas de PCB (current == NULL), on est dans un cas "anonyme"
    // comme les I/O : on n'a pas de processus à bloquer, on laisse simplement
    // value à 0 (ressource saturée) et on sort.
    if (!current) {
        return;
    }

    // Cas normal : plus de ressources et un processus réel -> on bloque
    current->waiting_on_semaphore = s;
    current->blocked_until = 1000000000; // très loin pour éviter un réveil par le scheduler_tick

    scheduler_block(current, "semaphore", "BLOCKED_SEM");

    sem_queue_push(s, current);
}


void semaphore_signal(Semaphore* s) {
    if (!s) return;

    PCB* next = sem_queue_pop(s);
    if (next) {
        // Réveiller un processus en attente
        remove_from_blocked_queue(next);

        next->waiting_on_semaphore = NULL;
        next->blocked_until = -1;

        scheduler_add_ready(next);

        trace_event(
            global_scheduler.current_time,
            next->pid,
            EVENT_UNBLOCKED,
            "READY",
            "semaphore",
            -1,
            "READY"
        );
    } else {
        // Personne en attente, on rend simplement une "place"
        s->value++;
    }
}
