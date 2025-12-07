#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>



#include "src/process/process.h"
#include "src/scheduler/scheduler.h"
#include "src/trace/logger.h"
#include "src/menu/menu.h"
#include "src/memory/memory.h"
#include "src/io/io.h"



#define NB_TASKS 3


/* ================= Processus & états (simulation) ================= */

typedef enum {
    PROC_READY = 0,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_TERMINATED
} proc_state_t;

struct process {
    int          pid;
    const char*  name;
    proc_state_t state;
    io_device_t  device;            // périphérique sur lequel il est bloqué
    int          total_cpu_needed;  // nombre total de ticks CPU qu'il doit faire
    int          cpu_done;          // ticks CPU déjà effectués

    // plan I/O : pour structurer la simu
    int          current_step;      // index du step en cours
    int          cpu_in_step;       // CPU fait dans ce step
};

typedef struct process process_t;

/* ================= Helpers d'affichage ================= */

static const char* state_to_str(proc_state_t st) {
    switch (st) {
        case PROC_READY:      return "READY";
        case PROC_RUNNING:    return "RUNNING";
        case PROC_BLOCKED:    return "BLOCKED";
        case PROC_TERMINATED: return "TERMINATED";
        default:              return "?";
    }
}

static const char* dev_to_str(io_device_t dev) {
    switch (dev) {
        case IO_DEVICE_PRINTER:  return "PRINTER";
        case IO_DEVICE_KEYBOARD: return "KEYBOARD";
        case IO_DEVICE_MOUSE:    return "MOUSE";
        case IO_DEVICE_DISK:     return "DISK";
        case IO_DEVICE_SCREEN:   return "SCREEN";
        case IO_DEVICE_NETWORK:  return "NETWORK";
        default:                 return "-";
    }
}

/* ================= Plan I/O déterministe =================
 * On définit pour chaque process une petite séquence :
 *  - cpu_before_io : nb de ticks CPU à faire avant de lancer I/O
 *  - dev          : périphérique utilisé
 *  - io_dur       : durée de l'I/O
 */

typedef struct {
    int         cpu_before_io;
    io_device_t dev;
    uint32_t    io_dur;
} io_step_t;

/* Plan pour chacun des 4 processus (fixe, pas aléatoire) */
static const io_step_t plan_A[] = {
    { 2, IO_DEVICE_PRINTER,  2 },
    { 1, IO_DEVICE_DISK,     3 },
};
static const int plan_A_len = sizeof(plan_A) / sizeof(plan_A[0]);
static const int plan_A_tail_cpu = 1;  // CPU final après la dernière I/O

static const io_step_t plan_B[] = {
    { 1, IO_DEVICE_KEYBOARD, 2 },
    { 2, IO_DEVICE_SCREEN,   1 },
};
static const int plan_B_len = sizeof(plan_B) / sizeof(plan_B[0]);
static const int plan_B_tail_cpu = 2;

static const io_step_t plan_C[] = {
    { 3, IO_DEVICE_MOUSE,    1 },
};
static const int plan_C_len = sizeof(plan_C) / sizeof(plan_C[0]);
static const int plan_C_tail_cpu = 1;

static const io_step_t plan_D[] = {
    { 2, IO_DEVICE_NETWORK,  2 },
    { 1, IO_DEVICE_PRINTER,  1 },
};
static const int plan_D_len = sizeof(plan_D) / sizeof(plan_D[0]);
static const int plan_D_tail_cpu = 1;

/* Helper pour récupérer le plan d'un process selon son pid */
static const io_step_t* get_plan(process_t* p, int* len_out, int* tail_cpu_out) {
    switch (p->pid) {
        case 1: *len_out = plan_A_len; *tail_cpu_out = plan_A_tail_cpu; return plan_A;
        case 2: *len_out = plan_B_len; *tail_cpu_out = plan_B_tail_cpu; return plan_B;
        case 3: *len_out = plan_C_len; *tail_cpu_out = plan_C_tail_cpu; return plan_C;
        case 4: *len_out = plan_D_len; *tail_cpu_out = plan_D_tail_cpu; return plan_D;
        default: *len_out = 0; *tail_cpu_out = 0; return NULL;
    }
}

/* Calcule total_cpu_needed = somme des cpu_before_io + cpu_tail */
static int compute_total_cpu_needed(process_t* p) {
    int len, tail;
    const io_step_t* plan = get_plan(p, &len, &tail);
    int total = tail;
    for (int i = 0; i < len; ++i) {
        total += plan[i].cpu_before_io;
    }
    return total;
}

/* ================= Fonctions attendues par io.c ================= */

void scheduler_block_process_on(process_t* p, io_device_t dev) {
    if (!p) return;
    p->state  = PROC_BLOCKED;
    p->device = dev;
    printf("[SCHED] %s (pid=%d) -> BLOCKED on %s\n",
           p->name, p->pid, dev_to_str(dev));
}

void scheduler_make_ready(process_t* p) {
    if (!p) return;
    if (p->state != PROC_TERMINATED) {
        p->state  = PROC_READY;
        p->device = IO_DEVICE_DISK; // valeur quelconque en READY
        printf("[SCHED] %s (pid=%d) -> READY (I/O done)\n",
               p->name, p->pid);
    }
}

/* ================= Mini scheduler (round-robin simple) ================= */

static process_t* pick_next_ready(process_t* procs[], size_t n, size_t* last_index) {
    if (n == 0) return NULL;
    size_t start = (*last_index + 1) % n;

    for (size_t i = 0; i < n; ++i) {
        size_t idx = (start + i) % n;
        if (procs[idx]->state == PROC_READY) {
            *last_index = idx;
            return procs[idx];
        }
    }
    return NULL;
}

/* ================= Affichage de l'état de tous les processus ================= */

static void print_states(uint32_t tick, process_t* procs[], size_t n) {
    printf("Tick %u :\n", tick);
    for (size_t i = 0; i < n; ++i) {
        process_t* p = procs[i];
        printf("  %-6s : %-10s", p->name, state_to_str(p->state));
        if (p->state == PROC_BLOCKED) {
            printf("  (dev=%-8s)", dev_to_str(p->device));
        } else {
            printf("  (dev=%-8s)", "-");
        }
        printf("  cpu_done=%d / %d\n", p->cpu_done, p->total_cpu_needed);
    }
}

/* ================= Simulation ================= */

static void simulate_io(void) {
    printf("===== SIMULATION I/O (4 processes, ressources fixes) =====\n");

    memory_init();   // heap simulé
    io_init();

    // Allouer les processus dans le heap simulé (mini_malloc)
    process_t* p1 = (process_t*) mini_malloc(sizeof(process_t));
    process_t* p2 = (process_t*) mini_malloc(sizeof(process_t));
    process_t* p3 = (process_t*) mini_malloc(sizeof(process_t));
    process_t* p4 = (process_t*) mini_malloc(sizeof(process_t));

    if (!p1 || !p2 || !p3 || !p4) {
        printf("Erreur d'allocation des processus.\n");
        return;
    }

    p1->pid = 1; p1->name = "ProcA"; p1->state = PROC_READY; p1->device = IO_DEVICE_PRINTER;
    p1->cpu_done = 0; p1->current_step = 0; p1->cpu_in_step = 0;
    p1->total_cpu_needed = compute_total_cpu_needed(p1);

    p2->pid = 2; p2->name = "ProcB"; p2->state = PROC_READY; p2->device = IO_DEVICE_PRINTER;
    p2->cpu_done = 0; p2->current_step = 0; p2->cpu_in_step = 0;
    p2->total_cpu_needed = compute_total_cpu_needed(p2);

    p3->pid = 3; p3->name = "ProcC"; p3->state = PROC_READY; p3->device = IO_DEVICE_PRINTER;
    p3->cpu_done = 0; p3->current_step = 0; p3->cpu_in_step = 0;
    p3->total_cpu_needed = compute_total_cpu_needed(p3);

    p4->pid = 4; p4->name = "ProcD"; p4->state = PROC_READY; p4->device = IO_DEVICE_PRINTER;
    p4->cpu_done = 0; p4->current_step = 0; p4->cpu_in_step = 0;
    p4->total_cpu_needed = compute_total_cpu_needed(p4);

    process_t* procs[4] = { p1, p2, p3, p4 };
    size_t n = 4;
    size_t last_index = (size_t)-1;

    for (uint32_t tick = 0; tick < 12; ++tick) {
        printf("\n=========== TICK %u ===========\n", tick);

        /* 1) Mise à jour des I/O (réveils éventuels) */
        io_update(tick);

        /* 2) Choisir un processus READY pour un tick CPU */
        process_t* current = pick_next_ready(procs, n, &last_index);

        if (current && current->state == PROC_READY) {
            current->state = PROC_RUNNING;
            printf("[CPU] %s (pid=%d) RUNNING\n", current->name, current->pid);

            // Récupérer son plan
            int len, tail;
            const io_step_t* plan = get_plan(current, &len, &tail);

            // Exécuter 1 tick de CPU
            current->cpu_done++;
            current->cpu_in_step++;

            // Cas 1 : encore des steps I/O prévus
            if (current->current_step < len) {
                io_step_t step = plan[current->current_step];

                if (current->cpu_in_step == step.cpu_before_io) {
                    // Time to request I/O sur ce périphérique
                    printf("[EVT] %s demande I/O sur %s pour %u ticks\n",
                           current->name, dev_to_str(step.dev), step.io_dur);

                    io_request(current, step.dev, step.io_dur, tick);

                    // io_request le passe en BLOCKED, on prépare le step suivant
                    current->current_step++;
                    current->cpu_in_step = 0;
                } else {
                    // Il reste encore du CPU à faire avant la prochaine I/O
                    if (current->state == PROC_RUNNING) {
                        current->state = PROC_READY;
                    }
                }
            }
            // Cas 2 : plus de steps I/O, on va vers la fin
            else {
                if (current->cpu_done >= current->total_cpu_needed) {
                    current->state = PROC_TERMINATED;
                    printf("[EVT] %s a terminé son travail.\n", current->name);
                } else {
                    // Encore du CPU "tail" à faire
                    if (current->state == PROC_RUNNING)
                        current->state = PROC_READY;
                }
            }
        } else {
            printf("[CPU] Aucun processus READY à ce tick.\n");
        }

        /* 3) Affichage de l'état global */
        print_states(tick, procs, n);
    }

    printf("\n===== FIN SIMULATION I/O =====\n");

    // À la fin, on peut dump la mémoire pour voir ce qui reste
    memory_dump();

    // Puis libérer les process (dans un vrai OS ce serait en fin de vie)
    mini_free(p1);
    mini_free(p2);
    mini_free(p3);
    mini_free(p4);

    printf("\n===== MEMOIRE APRES FREE DES PROCESS =====\n");
    memory_dump();
}

/* ================= MAIN ================= */

int main(void) {

    /* 1) Menu utilisateur */
    SchedulingPolicy policy = menu_choose_policy();

    /* 2) Init logger et scheduler */
    trace_init("trace.csv");
    scheduler_init(policy, 0);

    /* 3) Création de tâches (exemple) */
    PCB *tasks[NB_TASKS];

    tasks[0] = process_create(PRIORITY_HIGH,   5, 0);
    tasks[1] = process_create(PRIORITY_MEDIUM, 3, 2);
    tasks[2] = process_create(PRIORITY_LOW,    4, 4);

    /* 4) Simulation */
    while (!scheduler_is_finished()) {

        for (int i = 0; i < NB_TASKS; ++i) {
            PCB *p = tasks[i];
            if (p->state == NEW && p->arrival_time <= global_scheduler.current_time) {
                scheduler_add_ready(p);
            }
        }

        scheduler_tick();
    }

    /* 5) Fin */
    trace_close();

    printf("Simulation terminee au temps = %d\n", global_scheduler.current_time);

    for (int i = 0; i < NB_TASKS; ++i) {
        free(tasks[i]);
    }

    return 0;
}