
#ifndef MINIOS_PROCESS_H
#define MINIOS_PROCESS_H

#include <stddef.h>
#include <stdbool.h>


typedef enum {
    NEW = 0,
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} ProcessState;


typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_MEDIUM,
    PRIORITY_HIGH
} ProcessPriority;



typedef struct PCB {
    /* IDENTIFICATION */
    int pid;
    ProcessPriority priority; // PRIORITY_LOW PRIORITY_MEDIUM PRIORITY_HIGH

    /* ÉTAT DU PROCESSUS */
    ProcessState state;

    /* TEMPS (pour les stats + scheduler) */
    int arrival_time;        // temps où le processus entre dans le système
    int start_time;          // premier RUNNING
    int finish_time;         // moment TERMINATED
    int remaining_time;      // temps CPU restant (burst)
    int last_run_time;       // pour Round Robin / fairness
    int quantum_remaining;   // Pour Round Robin / combien de quantum tick reste-y-il ?


    /* CONTEXTE D’EXÉCUTION (simulé) */
    void *stack;             // pointeur vers la pile simulée
    void *context;           // registre / contexte (simulé car user-level)

    /* MÉMOIRE ALLOUÉE */
    void **allocations;      // tableau des pointeurs alloués via mini_malloc
    int alloc_count;         // combien d’allocations ce process a faites
    int alloc_capacity;      // taille max du tableau (peut être dynamique)

    /* I/O BLOQUANTES */
    int blocked_until;       // temps de réveil si I/O en attente
    bool waiting_for_io;      // booléen : 1 si en I/O, 0 sinon

    /* SYNCHRONISATION */
    void *waiting_on_mutex;      // mutex sur lequel il est bloqué
    void *waiting_on_semaphore;  // sémaphore sur lequel il est bloqué

    /* CHAÎNAGE POUR LES FILES D’ÉTAT */
    struct PCB *next;        // utile pour listes READY, BLOCKED, etc.

} PCB;



#endif //MINIOS_PROCESS_H
