
#include "mutex.h"

#include "../memory/memory.h"
#include "../scheduler/scheduler.h"
#include "../trace/logger.h"
#include "../trace/trace_event_types.h"

/* Petit helper : retirer un PCB de la blocked_queue globale */
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

static MutexWaitNode* mutex_alloc_node(PCB* p) {
    MutexWaitNode* n = (MutexWaitNode*) mini_malloc(sizeof(MutexWaitNode));
    if (!n) return NULL;
    n->proc = p;
    n->next = NULL;
    return n;
}

static void mutex_free_node(MutexWaitNode* n) {
    mini_free(n);
}

/* Ajoute en fin de file */
static void mutex_queue_push(Mutex* m, PCB* p) {
    MutexWaitNode* n = mutex_alloc_node(p);
    if (!n) return; // pas de mémoire => on abandonne discrètement

    if (!m->wait_queue) {
        m->wait_queue = n;
    } else {
        MutexWaitNode* cur = m->wait_queue;
        while (cur->next) cur = cur->next;
        cur->next = n;
    }
}

/* Récupère le premier en file */
static PCB* mutex_queue_pop(Mutex* m) {
    if (!m->wait_queue) return NULL;
    MutexWaitNode* n = m->wait_queue;
    m->wait_queue = n->next;
    PCB* p = n->proc;
    mutex_free_node(n);
    return p;
}

void mutex_init(Mutex* m) {
    if (!m) return;
    m->locked     = 0;
    m->owner      = NULL;
    m->wait_queue = NULL;
}

/**
 * Version bloquante :
 * - si free => on prend et on continue
 * - sinon => on bloque le PCB courant, et on l'aligne dans la file du mutex
 */
void mutex_lock(Mutex* m, PCB* current) {
    if (!m || !current) return;

    if (!m->locked) {
        // personne ne tient le mutex
        m->locked = 1;
        m->owner  = current;
        current->waiting_on_mutex = m;
        return;
    }

    // Mutex déjà pris : on se met en attente
    current->waiting_on_mutex = m;

    // Option : on met un "blocked_until" très loin pour éviter le réveil par I/O
    current->blocked_until = 1e9; // ou INT_MAX, selon ton style

    // Dans les traces, on peut distinguer la raison
    scheduler_block(current, "mutex", "BLOCKED_MUTEX");

    // On maintient aussi une file d'attente par mutex
    mutex_queue_push(m, current);
}

void mutex_unlock(Mutex* m, PCB* current) {
    if (!m) return;

    // Simple protection : seul le owner devrait unlock
    if (m->owner != current) {
        // Soit on ignore, soit on log une erreur
        // trace_event(...); // si tu veux
        return;
    }

    PCB* next = mutex_queue_pop(m);
    if (next) {
        // Il y avait quelqu'un en attente : on lui passe le mutex
        remove_from_blocked_queue(next);  // on le retire de la file BLOCKED globale

        next->waiting_on_mutex = NULL;
        next->blocked_until = -1;        // plus de raison temporelle d'être bloqué

        m->owner  = next;
        m->locked = 1;                   // toujours verrouillé, mais par next

        // On le remet en READY dans le scheduler
        scheduler_add_ready(next);

        // Trace UNBLOCKED (optionnel mais propre)
        trace_event(
            global_scheduler.current_time,
            next->pid,
            EVENT_UNBLOCKED,
            "READY",
            "mutex",
            -1,
            "READY"
        );
    } else {
        // Personne en attente : mutex libre
        m->locked = 0;
        m->owner  = NULL;
    }
}
