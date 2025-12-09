#ifndef MINIOS_IO_H
#define MINIOS_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "../process/process.h"   // pour PCB

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
 * Initialise le module I/O.
 */
void io_init(void);

/**
 * Lance une I/O bloquante pour un processus.
 *
 * - waiting_for_io = true
 * - blocked_until  = now + duration
 * - io_device      = dev
 * - scheduler_block(proc, "io", "IO")
 */
void io_request(PCB *proc, io_device_t dev,
                uint32_t duration, uint32_t now);

/**
 * Hook de mise à jour I/O.
 * Avec votre scheduler actuel, les réveils sont gérés dans scheduler_tick()
 * via blocked_until. On garde cette fonction pour compat.
 */
void io_update(uint32_t now);

/**
 * Helper pour debug / logs.
 */
const char* io_device_to_str(io_device_t dev);

#endif // MINIOS_IO_H
