#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>



#include "src/process/process.h"
#include "src/scheduler/scheduler.h"
#include "src/trace/logger.h"
#include "src/menu/menu.h"
#include "src/memory/memory.h"
#include "src/io/io.h"
// ======================================================================
//  Test simple mutex + semaphore
// ======================================================================

#include "src/sync/mutex.h"
#include "src/sync/semaphore.h"

void test_sync_primitives(void) {
    printf("\n========== TEST MUTEX & SEMAPHORE ==========\n");

    // Initialisation mémoire + scheduler
    memory_init();
    scheduler_init(SCHED_PRIORITY, 0);

    // Création manuelle de deux processus PCB
    PCB *A = process_create(PRIORITY_HIGH,   5, 0);
    PCB *B = process_create(PRIORITY_MEDIUM, 5, 0);

    // On les rend READY
    scheduler_add_ready(A);
    scheduler_add_ready(B);

    // MUTEX
    Mutex m;
    mutex_init(&m);

    printf("\n-- MUTEX TEST --\n");

    printf("Process A lock mutex...\n");
    mutex_lock(&m, A);
    printf("Mutex owner = P%d\n", m.owner->pid);

    printf("Process B lock mutex (should BLOCK)...\n");
    mutex_lock(&m, B);
    printf("P%d is now state=%d (BLOCKED=3)\n", B->pid, B->state);

    printf("Process A unlock mutex (should wake B)...\n");
    mutex_unlock(&m, A);
    printf("Mutex new owner = P%d\n", m.owner->pid);

    // SEMAPHORE
    Semaphore s;
    semaphore_init(&s, 1);

    printf("\n-- SEMAPHORE TEST --\n");

    printf("Process A semaphore_wait...\n");
    semaphore_wait(&s, A);

    printf("Process B semaphore_wait (should BLOCK)...\n");
    semaphore_wait(&s, B);

    printf("Value after waits = %d\n", s.value);

    printf("Process A semaphore_signal (should wake B)...\n");
    semaphore_signal(&s);

    printf("Semaphore final value = %d\n", s.value);

    printf("\n========== END MUTEX & SEMAPHORE TEST ==========\n");
}

int main(void) {

    // === Lancer le test MUTEX / SEMAPHORE ===
    test_sync_primitives();

    printf("\n\n===== Lancement de la simulation principale =====\n\n");

    // === Le reste de ton main actuel ===
    SchedulingPolicy policy = menu_choose_policy();
    trace_init("trace.csv");
    scheduler_init(policy, 0);

    PCB *tasks[3];
    tasks[0] = process_create(PRIORITY_HIGH,   5, 0);
    tasks[1] = process_create(PRIORITY_MEDIUM, 3, 2);
    tasks[2] = process_create(PRIORITY_LOW,    4, 4);

    while (!scheduler_is_finished()) {
        for (int i = 0; i < 3; ++i) {
            PCB *p = tasks[i];
            if (p->state == NEW && p->arrival_time <= global_scheduler.current_time) {
                scheduler_add_ready(p);
            }
        }
        scheduler_tick();
    }

    trace_close();
    printf("Simulation terminee au temps = %d\n", global_scheduler.current_time);

    for (int i = 0; i < 3; ++i) free(tasks[i]);
    return 0;
}
