#include "memory.h"

#include <stdint.h>
#include <stdio.h>
#include "../process/process.h"
// --- AJOUTS POUR LE LOGGING ---
#include "../trace/logger.h"
#include "../scheduler/scheduler.h"

/************************************************************
   Paramètres du heap simulé
 ************************************************************/

// 64 MiB = 64 * 1024 * 1024 octets
#define HEAP_SIZE   (64u * 1024u * 1024u)

/* Zone de mémoire simulée (heap utilisateur). */
static uint8_t heap[HEAP_SIZE];

/* Description d’un bloc dans la free list. */
typedef struct block {
    size_t size;           // taille utile du bloc (données utilisateur)
    int    free;           // 1 = libre, 0 = occupé
    struct block* next;    // bloc suivant dans la free list
} block_t;

/* Premier bloc du heap. */
static block_t* first_block = NULL;


/************************************************************
   Fonctions internes
 ************************************************************/

/* Alignement (optionnel) pour éviter des problèmes sur certaines archis). */
static size_t align_size(size_t size) {
    const size_t align = sizeof(void*);
    return (size + align - 1) & ~(align - 1);
}

/* Convertit un pointeur utilisateur vers l’en-tête de bloc correspondant. */
static block_t* ptr_to_block(void* ptr) {
    if (!ptr) return NULL;
    return (block_t*)((uint8_t*)ptr - sizeof(block_t));
}

/* Affichage d'une taille lisible (B / KiB / MiB). */
static void print_human_size(size_t bytes) {
    if (bytes >= 1024u * 1024u) {
        printf("%zu MiB", bytes / (1024u * 1024u));
    } else if (bytes >= 1024u) {
        printf("%zu KiB", bytes / 1024u);
    } else {
        printf("%zu B", bytes);
    }
}


/************************************************************
   API publique
 ************************************************************/

void memory_init(void) {
    /* On place un bloc unique couvrant tout le heap. */
    first_block         = (block_t*) heap;
    first_block->size = HEAP_SIZE - sizeof(block_t);
    first_block->free = 1;
    first_block->next = NULL;
}

void* mini_malloc(size_t size) {
    if (size == 0 || !first_block)
        return NULL;

    size = align_size(size);

    block_t* curr = first_block;

    while (curr) {
        if (curr->free && curr->size >= size) {

            /* Si le bloc est beaucoup plus grand, on le découpe (split). */
            if (curr->size >= size + sizeof(block_t) + 8) {
                uint8_t* split_addr = (uint8_t*)curr + sizeof(block_t) + size;
                block_t* new_block  = (block_t*)split_addr;

                new_block->size = curr->size - size - sizeof(block_t);
                new_block->free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }

            curr->free = 0;

            // --- DEBUT LOG ALLOCATION ---
            // On récupère le PID courant, ou -1 si c'est le système
            int owner = (global_scheduler.current) ? global_scheduler.current->pid : -1;
            char size_str[32];
            sprintf(size_str, "%zu", curr->size); // On logue la taille du bloc alloué

            trace_event(
                    global_scheduler.current_time,
                    owner,
                    "MEMORY",
                    "ALLOC",
                    size_str, // Raison = taille en octets
                    -1,
                    "MEM"
            );
            // --- FIN LOG ALLOCATION ---

            return (uint8_t*)curr + sizeof(block_t);
        }
        curr = curr->next;
    }

    /* Plus de place disponible. */
    return NULL;
}

void mini_free(void* ptr) {
    if (!ptr || !first_block)
        return;

    block_t* block = ptr_to_block(ptr);

    /* Vérifie que l’adresse pointe bien dans le heap simulé. */
    if ((uint8_t*)block < heap || (uint8_t*)block >= heap + HEAP_SIZE)
        return;  // pointeur invalide -> on ignore ou on log

    /* Vérifie que ce bloc appartient bien à notre free list. */
    block_t* curr = first_block;
    block_t* prev = NULL;
    int found = 0;

    while (curr) {
        if (curr == block) {
            found = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    if (!found)
        return;  // bloc inconnu -> on ignore

    /* Protection contre double free. */
    if (block->free)
        return;

    // --- DEBUT LOG FREE (Avant de fusionner, pour avoir la bonne taille) ---
    int owner = (global_scheduler.current) ? global_scheduler.current->pid : -1;
    char size_str[32];
    sprintf(size_str, "%zu", block->size);

    trace_event(
            global_scheduler.current_time,
            owner,
            "MEMORY",
            "FREE",
            size_str, // Raison = taille en octets
            -1,
            "MEM"
    );
    // --- FIN LOG FREE ---

    /* Marque le bloc comme libre. */
    block->free = 1;

    /* Fusion avec le bloc suivant s’il est libre. */
    if (block->next && block->next->free) {
        block->size += sizeof(block_t) + block->next->size;
        block->next  = block->next->next;
    }

    /* Fusion avec le bloc précédent s’il est libre. */
    if (prev && prev->free) {
        prev->size += sizeof(block_t) + block->size;
        prev->next  = block->next;
    }
}

/* Affichage propre : liste des blocs (état du heap simulé). */
void memory_dump(void) {
    block_t* curr = first_block;
    int index = 0;

    printf("=== HEAP STATE (%zu MiB) ===\n", HEAP_SIZE / (1024u * 1024u));

    while (curr) {
        printf("Bloc %d : %s | ",
               index,
               curr->free ? "FREE" : "USED");

        print_human_size(curr->size);
        printf(" | @%p\n", (void*)curr);

        curr = curr->next;
        index++;
    }

    printf("================================\n");
}

void memory_dump_with_processes(struct PCB **tasks, int nb_tasks) {
    block_t* curr = first_block;
    int index = 0;

    printf("=== HEAP STATE BY PROCESS (%zu MiB) ===\n",
           HEAP_SIZE / (1024u * 1024u));

    while (curr) {
        // Pointeur "utilisateur" (ce que mini_malloc renvoie)
        void *payload = (uint8_t*)curr + sizeof(block_t);
        int owner_pid = -1;

        // On cherche quel process possède ce bloc (si c'est un bloc USED)
        if (!curr->free && tasks && nb_tasks > 0) {
            for (int i = 0; i < nb_tasks; ++i) {
                struct PCB *p = tasks[i];
                if (!p) continue;
                if (p->mem_base == payload) {
                    owner_pid = p->pid;
                    break;
                }
            }
        }

        printf("Bloc %d : %s | ",
               index,
               curr->free ? "FREE" : "USED");

        print_human_size(curr->size);

        if (!curr->free) {
            if (owner_pid != -1) {
                printf(" | PID=%d", owner_pid);
            } else {
                printf(" | PID=?");
            }
        } else {
            printf(" | FREE");
        }

        printf(" | @%p\n", (void*)curr);

        curr = curr->next;
        index++;
    }

    printf("=======================================\n");
}