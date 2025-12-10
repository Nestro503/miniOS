#ifndef MINIOS_MUTEX_H
#define MINIOS_MUTEX_H

#include "../process/process.h"

/**
 * Mutex binaire simple.
 *
 * locked = 0 : libre
 * locked = 1 : possédé par "owner"
 */
typedef struct MutexWaitNode {
    PCB*                  proc;
    struct MutexWaitNode* next;
} MutexWaitNode;

typedef struct Mutex {
    int            locked;      // 0 = libre, 1 = pris
    PCB*           owner;       // processus qui tient le mutex
    MutexWaitNode* wait_queue;  // file FIFO de PCBs en attente
} Mutex;

/**
 * Initialise un mutex (non alloué dynamiquement).
 */
void mutex_init(Mutex* m);

/**
 * Prend le mutex de manière bloquante.
 * - Si le mutex est libre, current devient owner et continue.
 * - Si le mutex est déjà pris, current est mis en attente (BLOCKED)
 *   et ajouté à la file d'attente du mutex.
 *
 * Cas particulier :
 *  - si current == NULL, mode "anonyme" (ex : I/O simulée) :
 *      on marque juste le mutex comme pris si libre, sans blocage ni file.
 */
void mutex_lock(Mutex* m, PCB* current);

/**
 * Libère le mutex.
 * - Si des processus sont en attente, le prochain est réveillé et devient owner.
 * - Sinon, le mutex redevient libre.
 *
 * Cas particulier :
 *  - si current == NULL en mode anonyme, on libère juste le mutex.
 */
void mutex_unlock(Mutex* m, PCB* current);

#endif // MINIOS_MUTEX_H
