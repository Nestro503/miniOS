#ifndef MINIOS_IO_H
#define MINIOS_IO_H

#include <stdint.h>
#include "../process/process.h"   // pour PCB

typedef PCB process_t;

/* Types de périphériques simulés */
typedef enum {
    IO_DEVICE_PRINTER = 0,
    IO_DEVICE_KEYBOARD,
    IO_DEVICE_MOUSE,
    IO_DEVICE_DISK,
    IO_DEVICE_SCREEN,
    IO_DEVICE_NETWORK
} io_device_t;

/* Initialise le module I/O (file + sémaphores) */
void io_init(void);

/* Lance une I/O bloquante pour un processus. */
void io_request(process_t* proc, io_device_t dev,
                uint32_t duration, uint32_t now);

/* Met à jour les I/O (réveille celles qui ont fini). */
void io_update(uint32_t now);

#endif //MINIOS_IO_H
