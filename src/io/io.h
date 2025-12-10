#ifndef MINIOS_IO_H
#define MINIOS_IO_H

#include <stdint.h>
#include "../process/process.h"

/* Types de périphériques I/O simulés */
typedef enum {
    IO_DEVICE_PRINTER = 0,
    IO_DEVICE_KEYBOARD,
    IO_DEVICE_MOUSE,
    IO_DEVICE_DISK,
    IO_DEVICE_SCREEN,
    IO_DEVICE_NETWORK,
    IO_DEVICE_COUNT
} io_device_t;

/**
 * Initialise le module I/O et les primitives de synchro associées
 * (2 mutex + 4 sémaphores, dont un binaire).
 */
void io_init(void);

/**
 * Lance une I/O bloquante pour un processus.
 *
 * - waiting_for_io = true
 * - blocked_until  = now + duration
 * - io_device      = dev
 * - scheduler_block(proc, "io", "IO")
 *
 * En plus, on met à jour le mutex / sémaphore associé au périphérique.
 */
void io_request(PCB *proc, io_device_t dev,
                uint32_t duration, uint32_t now);

/**
 * Hook de mise à jour I/O (optionnel, pour compat).
 * Avec ton scheduler actuel, les réveils se font via blocked_until,
 * donc io_update ne fait rien.
 */
void io_update(uint32_t now);

/**
 * Appelé lorsque le scheduler réveille un processus pour fin d'I/O.
 * Ça permet de libérer le mutex / sémaphore associé au device.
 */
void io_release_resource_for(PCB *proc);

/**
 * Helper pour debug / logs.
 */
const char* io_device_to_str(io_device_t dev);

#endif // MINIOS_IO_H
