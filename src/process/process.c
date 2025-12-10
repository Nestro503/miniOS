#include <stdlib.h>
#include <string.h>
#include "process.h"
#include "../scheduler/scheduler.h"
#include "../trace/logger.h"
#include "../memory/memory.h"   // adapte le chemin/nom si besoin

static int next_pid = 1;  // compteur de PID

PCB *process_create(ProcessPriority priority,
                    int burst_time,
                    int arrival_time,
                    size_t mem_size)
{
    PCB *p = malloc(sizeof(PCB));
    if (!p) {
        return NULL;
    }

    // Initialisation globale à zéro pour partir propre
    memset(p, 0, sizeof(PCB));

    /* IDENTIFICATION */
    p->pid = next_pid++;
    p->priority = priority;

    /* ÉTAT INITIAL */
    p->state = NEW;

    /* TEMPS */
    p->arrival_time   = arrival_time;
    p->start_time     = -1;          // pas encore passé en RUNNING
    p->finish_time    = -1;          // pas encore terminé
    p->remaining_time = burst_time;  // durée CPU simulée
    p->last_run_time  = -1;
    p->quantum_remaining = 0;        // initialisé par le scheduler pour RR

    /* CONTEXTE (simulé) */
    p->stack   = NULL;
    p->context = NULL;

    /* MÉMOIRE ALLOUÉE (tableau d’allocs mini-malloc, optionnel) */
    p->allocations    = NULL;
    p->alloc_count    = 0;
    p->alloc_capacity = 0;

    /* I/O BLOQUANTES */
    p->blocked_until  = -1;
    p->waiting_for_io = false;
    p->io_device      = -1;  // par défaut : aucune I/O
    p->io_duration    = 0;   // aucune I/O
    p->io_start_time  = -1;  // -1 = pas de déclenchement prévu

    /* SYNCHRO */
    p->waiting_on_mutex     = NULL;
    p->waiting_on_semaphore = NULL;

    /* MÉMOIRE PROCESSUS (zone principale) */
    p->mem_size = mem_size;
    if (mem_size > 0) {
        // Remplace mini_malloc par le nom réel si différent
        p->mem_base = mini_malloc(mem_size);
        if (p->mem_base == NULL) {
            // Pas assez de mémoire : on peut marquer le process comme terminé
            p->state = TERMINATED;
            trace_event(
                global_scheduler.current_time,
                p->pid,
                "CREATE_FAIL_OOM",
                "TERMINATED",
                "",
                -1,
                "NONE"
            );
        }
    } else {
        p->mem_base = NULL;
    }

    /* CHAÎNAGE */
    p->next = NULL;

    /* Stats globales */
    global_scheduler.total_processes++;

    /* Trace de création */
    trace_event(
        global_scheduler.current_time,
        p->pid,
        "CREATE",
        "NEW",
        "",
        -1,
        "NEW"
    );

    void *process_alloc(PCB *p, size_t size) {
        if (!p || size == 0) {
            return NULL;
        }

        void *ptr = mini_malloc(size);
        if (!ptr) {
            return NULL;
        }

        // Initialisation du tableau d'allocations si besoin
        if (p->alloc_capacity == 0) {
            p->alloc_capacity = 4;
            p->allocations = malloc(p->alloc_capacity * sizeof(void *));
            if (!p->allocations) {
                mini_free(ptr);
                p->alloc_capacity = 0;
                return NULL;
            }
        }
        // Agrandir le tableau si plein
        else if (p->alloc_count >= p->alloc_capacity) {
            int new_cap = p->alloc_capacity * 2;
            void **new_tab = realloc(p->allocations, new_cap * sizeof(void *));
            if (!new_tab) {
                mini_free(ptr);
                return NULL;
            }
            p->allocations = new_tab;
            p->alloc_capacity = new_cap;
        }

        p->allocations[p->alloc_count++] = ptr;
        return ptr;
    }

    void process_free_all(PCB *p) {
        if (!p) return;

        // Libérer toutes les allocations enregistrées
        for (int i = 0; i < p->alloc_count; ++i) {
            if (p->allocations && p->allocations[i]) {
                mini_free(p->allocations[i]);
                p->allocations[i] = NULL;
            }
        }

        p->alloc_count = 0;

        // Libérer le tableau lui-même
        if (p->allocations) {
            free(p->allocations);
            p->allocations = NULL;
        }

        p->alloc_capacity = 0;
    }

    return p;
}
