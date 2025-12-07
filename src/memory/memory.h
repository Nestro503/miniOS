

#ifndef MINIOS_MEMORY_H
#define MINIOS_MEMORY_H
#include <stddef.h>  // size_t

/**
 * Initialise le heap simulé.
 * À appeler une fois au démarrage du MiniOS.
 */
void memory_init(void);

/**
 * Allocation dynamique dans le heap simulé.
 * Équivalent simplifié de malloc().
 *
 * @param size : taille demandée en octets
 * @return pointeur vers un bloc de taille >= size, ou NULL en cas d’échec.
 */
void* mini_malloc(size_t size);

/**
 * Libération d’un bloc alloué par mini_malloc().
 * Équivalent simplifié de free().
 *
 * @param ptr : pointeur retourné par mini_malloc()
 */
void mini_free(void* ptr);

/**
 * Dump brut de la free-list (bloc par bloc).
 */
void memory_dump(void);

/**
 * Dump visuel du heap (barre ASCII + liste des blocs),
 * format lisible pour rapport / soutenance.
 */
void memory_visual_dump(void);


#endif //MINIOS_MEMORY_H
