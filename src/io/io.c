#include "io.h"
#include <stdio.h>
#include "../scheduler/scheduler.h"

void io_init(void) {
    // Rien à initialiser pour le moment.
}

const char* io_device_to_str(io_device_t dev) {
    switch (dev) {
        case IO_DEVICE_PRINTER:  return "PRINTER";
        case IO_DEVICE_KEYBOARD: return "KEYBOARD";
        case IO_DEVICE_MOUSE:    return "MOUSE";
        case IO_DEVICE_DISK:     return "DISK";
        case IO_DEVICE_SCREEN:   return "SCREEN";
        case IO_DEVICE_NETWORK:  return "NETWORK";
        default:                 return "?";
    }
}

void io_request(PCB *proc, io_device_t dev,
                uint32_t duration, uint32_t now)
{
    if (!proc) return;

    int wake_time = (int)(now + duration);

    proc->waiting_for_io = true;
    proc->blocked_until  = wake_time;
    proc->io_device      = (int)dev;

    printf("[IO] P%d -> I/O sur %s pour %u ticks (reveil @ %d)\n",
           proc->pid, io_device_to_str(dev), duration, wake_time);

    // Le scheduler s’occupe de le mettre en file BLOCKED et de tracer.
    scheduler_block(proc, "io", "IO");
}

void io_update(uint32_t now) {
    (void)now;
    // Les réveils sont gérés dans scheduler_tick()
    // via blocked_until / waiting_for_io.
}
