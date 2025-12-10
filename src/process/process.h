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
    ProcessPriority priority; // PRIORITY_LOW / PRIORITY_MEDIUM / PRIORITY_HIGH

    /* ÉTAT DU PROCESSUS */
    ProcessState state;

    /* TEMPS (pour les stats + scheduler) */
    int arrival_time;        // temps où le processus entre dans le système
    int start_time;          // premier RUNNING
    int finish_time;         // moment TERMINATED
    int remaining_time;      // temps CPU restant (burst)
    int last_run_time;       // pour Round Robin / fairness
    int quantum_remaining;   // Pour Round Robin / ticks restants dans le quantum

    /* CONTEXTE D’EXÉCUTION (simulé) */
    void *stack;             // pointeur vers la pile simulée
    void *context;           // registre / contexte (simulé car user-level)

    /* MÉMOIRE ALLOUÉE (mini_malloc) — allocations multiples éventuelles */
    void **allocations;      // tableau des pointeurs alloués via mini_malloc
    int alloc_count;         // nombre d’allocations
    int alloc_capacity;      // taille max du tableau

    /* I/O BLOQUANTES */
    int  blocked_until;      // temps de réveil si I/O en attente
    bool waiting_for_io;     // true si en I/O, false sinon
    int  io_device;          // code du périphérique I/O (ou -1 si aucune I/O)
    int  io_duration;        // durée de l'I/O en ticks (0 = pas d’I/O)
    int  io_start_time;      // tick global auquel lancer l'I/O (>=0, -1 = jamais)

    /* SYNCHRONISATION */
    void *waiting_on_mutex;      // mutex sur lequel il est bloqué
    void *waiting_on_semaphore;  // sémaphore sur lequel il est bloqué

    /* MÉMOIRE PROPRE AU PROCESSUS (zone principale) */
    size_t mem_size;        // taille mémoire demandée pour ce process
    void *mem_base;         // pointeur / adresse renvoyée par le mini-malloc

    /* CHAÎNAGE POUR LES FILES (READY, BLOCKED, etc.) */
    struct PCB *next;

} PCB;

/**
 * Création d’un processus :
 *  - priority      : PRIORITY_LOW / MEDIUM / HIGH
 *  - burst_time    : temps CPU total (remaining_time initial)
 *  - arrival_time  : temps d’arrivée dans le système
 *  - mem_size      : taille mémoire demandée sur le heap simulé
 */
PCB *process_create(ProcessPriority priority,
                    int burst_time,
                    int arrival_time,
                    size_t mem_size);

/**
 * Alloue un bloc de taille 'size' pour le processus p sur le mini-heap,
 * et enregistre ce pointeur dans p->allocations pour pouvoir tout
 * libérer facilement à la fin.
 */
void *process_alloc(PCB *p, size_t size);

/**
 * Libère toutes les allocations enregistrées dans p->allocations
 * (sauf mem_base, qui est gérée à part) et nettoie la structure.
 */
void process_free_all(PCB *p);


#endif //MINIOS_PROCESS_H
