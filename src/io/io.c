//
// Created by Erwan on 27/11/2025.
//

#include "io.h"

#include <stddef.h>
#include <stdint.h>
#include "../memory/memory.h"   // pour mini_malloc / mini_free


/* fournis par le "scheduler" (implémentés dans ton main de test pour l’instant) */
extern void scheduler_block_process_on(process_t* p, io_device_t dev);
extern void scheduler_make_ready(process_t* p);

/* ---------------------------------------------------------
 *  File d'attente des I/O
 * --------------------------------------------------------- */

typedef struct io_wait {
    process_t*      proc;
    uint32_t        wake_time;   // tick où le processus doit être réveillé
    io_device_t     dev;         // sur quel périphérique il attend
    struct io_wait* next;
} io_wait_t;

static io_wait_t* io_queue = NULL;

/* allocation/libération des noeuds de la file I/O
 * (ici on utilise ton heap simulé, mais tu pourrais mettre malloc/free)
 */
static io_wait_t* io_alloc_node(void) {
    return (io_wait_t*) mini_malloc(sizeof(io_wait_t));
}

static void io_free_node(io_wait_t* node) {
    mini_free(node);
}

/* ---------------------------------------------------------
 *  API
 * --------------------------------------------------------- */

void io_init(void) {
    io_queue = NULL;
}

/* insertion triée par wake_time dans io_queue */
static void io_insert_node(io_wait_t* node) {
    if (!io_queue || node->wake_time < io_queue->wake_time) {
        node->next = io_queue;
        io_queue   = node;
        return;
    }

    io_wait_t* curr = io_queue;
    while (curr->next && curr->next->wake_time <= node->wake_time) {
        curr = curr->next;
    }
    node->next = curr->next;
    curr->next = node;
}

void io_request(process_t* proc, io_device_t dev,
                uint32_t duration, uint32_t now) {
    if (!proc) return;

    uint32_t wake = now + duration;

    /* 1) Bloquer le processus via le scheduler, sur ce device */
    scheduler_block_process_on(proc, dev);

    /* 2) Allouer un noeud I/O */
    io_wait_t* node = io_alloc_node();
    if (!node) {
        // pas de mémoire -> on abandonne silencieusement pour la simu
        return;
    }

    node->proc      = proc;
    node->dev       = dev;
    node->wake_time = wake;
    node->next      = NULL;

    /* 3) Insérer dans la file I/O */
    io_insert_node(node);
}

void io_update(uint32_t now) {
    /* Réveiller tous les processus dont le wake_time est atteint */
    while (io_queue && io_queue->wake_time <= now) {
        io_wait_t* node = io_queue;
        io_queue = io_queue->next;

        /* Remettre le processus en READY via le scheduler */
        scheduler_make_ready(node->proc);

        io_free_node(node);
    }
}

