
#ifndef MINIOS_SEMAPHORE_H
#define MINIOS_SEMAPHORE_H

/*
typedef struct {
    int count;               // Nombre de ressources disponibles
    queue_t wait_queue;      // File des processus bloqués
} sem_t;
*/

#include "../process/process.h"

/**
 * Sémaphore comptant (value >= 0)
 * - value représente le nombre de ressources disponibles
 * - queue contient les PCBs en attente quand value == 0
 */

typedef struct SemWaitNode {
    PCB*                proc;
    struct SemWaitNode* next;
} SemWaitNode;

typedef struct Semaphore {
    int        value;
    SemWaitNode* queue;
} Semaphore;

/**
 * Initialise le sémaphore avec une valeur initiale.
 * ex : Semaphore s; semaphore_init(&s, 3);
 */
void semaphore_init(Semaphore* s, int initial);

/**
 * P() / wait() / down()
 * - si value > 0 : décrémente et continue
 * - sinon : bloque current et l’ajoute à la file
 */
void semaphore_wait(Semaphore* s, PCB* current);

/**
 * V() / signal() / up()
 * - si une file d’attente existe : réveille le premier
 * - sinon : incrémente value
 */
void semaphore_signal(Semaphore* s);

#endif // MINIOS_SEMAPHORE_H
