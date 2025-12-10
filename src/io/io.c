#include "io.h"
#include <stdio.h>

#include "../scheduler/scheduler.h"
#include "../sync/mutex.h"
#include "../sync/semaphore.h"

/* 2 mutex + 4 sémaphores :
 *
 * - Mutex :
 *     PRINTER -> g_mutex_printer
 *     SCREEN  -> g_mutex_screen
 *
 * - Sémaphores :
 *     KEYBOARD -> g_sem_keyboard (binaire, value = 1)
 *     MOUSE    -> g_sem_mouse    (value = 2)
 *     DISK     -> g_sem_disk     (value = 2)
 *     NETWORK  -> g_sem_network  (value = 3)
 */

static Mutex     g_mutex_printer;
static Mutex     g_mutex_screen;

static Semaphore g_sem_keyboard;   // binaire
static Semaphore g_sem_mouse;
static Semaphore g_sem_disk;
static Semaphore g_sem_network;

void io_init(void) {
    /* Init des mutex */
    mutex_init(&g_mutex_printer);
    mutex_init(&g_mutex_screen);

    /* Init des sémaphores */
    semaphore_init(&g_sem_keyboard, 1); // binaire
    semaphore_init(&g_sem_mouse,   2);
    semaphore_init(&g_sem_disk,    2);
    semaphore_init(&g_sem_network, 3);

    printf("[IO] Init : PRINTER/SCREEN avec mutex, "
           "KEYBOARD(binaire)/MOUSE/DISK/NETWORK avec semaphores.\n");
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

/* Petite fonction interne : "prendre" logiquement la ressource
 * associée au périphérique.
 *
 * On utilise les mutex / sémaphores en mode "anonyme" (current == NULL)
 * pour ne pas interférer avec le scheduler (blocage / FILE BLOCKED).
 */
static void io_acquire_device(io_device_t dev) {
    switch (dev) {
        case IO_DEVICE_PRINTER:
            mutex_lock(&g_mutex_printer, NULL);
            break;
        case IO_DEVICE_SCREEN:
            mutex_lock(&g_mutex_screen, NULL);
            break;
        case IO_DEVICE_KEYBOARD:
            semaphore_wait(&g_sem_keyboard, NULL);
            break;
        case IO_DEVICE_MOUSE:
            semaphore_wait(&g_sem_mouse, NULL);
            break;
        case IO_DEVICE_DISK:
            semaphore_wait(&g_sem_disk, NULL);
            break;
        case IO_DEVICE_NETWORK:
            semaphore_wait(&g_sem_network, NULL);
            break;
        default:
            break;
    }
}

/* Et réciproquement, libérer la ressource à la fin de l'I/O */
static void io_release_device(io_device_t dev) {
    switch (dev) {
        case IO_DEVICE_PRINTER:
            mutex_unlock(&g_mutex_printer, NULL);
            break;
        case IO_DEVICE_SCREEN:
            mutex_unlock(&g_mutex_screen, NULL);
            break;
        case IO_DEVICE_KEYBOARD:
            semaphore_signal(&g_sem_keyboard);
            break;
        case IO_DEVICE_MOUSE:
            semaphore_signal(&g_sem_mouse);
            break;
        case IO_DEVICE_DISK:
            semaphore_signal(&g_sem_disk);
            break;
        case IO_DEVICE_NETWORK:
            semaphore_signal(&g_sem_network);
            break;
        default:
            break;
    }
}

void io_request(PCB *proc, io_device_t dev,
                uint32_t duration, uint32_t now)
{
    if (!proc) return;

    int wake_time = (int)(now + duration);

    /* 1) On "prend" logiquement la ressource associée */
    io_acquire_device(dev);

    /* 2) On marque l'état I/O dans le PCB */
    proc->waiting_for_io = true;
    proc->blocked_until  = wake_time;
    proc->io_device      = (int)dev;

    printf("[IO] P%d -> I/O sur %s pour %u ticks (reveil @ %d)\n",
           proc->pid, io_device_to_str(dev), duration, wake_time);

    /* 3) On bloque le processus via le scheduler */
    scheduler_block(proc, "io", "IO");
}

void io_update(uint32_t now) {
    (void)now;
    // Tu peux laisser vide : les réveils sont gérés par scheduler_tick()
}

/* Appelé par le scheduler lorsque l'I/O est terminée
 * (au moment où on passe de BLOCKED à READY).
 */
void io_release_resource_for(PCB *proc) {
    if (!proc) return;
    if (!proc->waiting_for_io) return;
    if (proc->io_device < 0 || proc->io_device >= IO_DEVICE_COUNT) return;

    io_device_t dev = (io_device_t)proc->io_device;

    printf("[IO] P%d -> fin d'I/O sur %s, liberation de la ressource.\n",
           proc->pid, io_device_to_str(dev));

    io_release_device(dev);

    /* On nettoie les champs I/O du PCB */
    proc->waiting_for_io = false;
    proc->blocked_until  = -1;
    proc->io_device      = -1;
}
