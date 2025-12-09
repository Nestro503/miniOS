#include <stdio.h>
#include "scenario.h"
#include "../io/io.h"

/**
 * Construit un scénario interactif :
 * - demande à l'utilisateur combien de processus créer
 * - pour chacun : priorité, burst CPU, instant d'arrivée
 * - et éventuellement un périphérique I/O associé
 *
 * Paramètres :
 *  - out_tasks : tableau de PCB* alloué par le main
 *  - max_tasks : taille max du tableau
 *
 * Retour :
 *  - nombre de processus effectivement créés
 */
int scenario_build_interactive(PCB **out_tasks, int max_tasks, SchedulingPolicy policy) {
    int n = 0;

    printf("\n=== Configuration du scenario ===\n");
    printf("Nombre de processus (max %d) : ", max_tasks);

    if (scanf("%d", &n) != 1 || n <= 0 || n > max_tasks) {
        fprintf(stderr, "Valeur invalide, utilisation de 3 processus par defaut.\n");
        n = (max_tasks >= 3) ? 3 : max_tasks;
    }

    for (int i = 0; i < n; ++i) {

        int prio = PRIORITY_MEDIUM;  // DEFAULT
        int burst = 1;
        int arrival = 0;

        printf("\n--- Processus %d ---\n", i + 1);

        /* PRIORITÉ */
        if (policy == SCHED_ROUND_ROBIN) {
            printf("Round Robin choisi -> Priorite ignoree, MEDIUM appliquee automatiquement.\n");
            prio = PRIORITY_MEDIUM;
        } else {
            printf("Priorite (0 = LOW, 1 = MEDIUM, 2 = HIGH) : ");
            if (scanf("%d", &prio) != 1 ||
                prio < PRIORITY_LOW ||
                prio > PRIORITY_HIGH)
            {
                fprintf(stderr, "Priorite invalide, MEDIUM utilisee.\n");
                prio = PRIORITY_MEDIUM;
            }
        }

        /* BURST CPU */
        printf("Burst CPU (duree > 0) : ");
        if (scanf("%d", &burst) != 1 || burst <= 0) {
            fprintf(stderr, "Burst invalide, 5 utilise par defaut.\n");
            burst = 5;
        }

        /* ARRIVEE */
        printf("Instant d'arrivee (>= 0) : ");
        if (scanf("%d", &arrival) != 1 || arrival < 0) {
            fprintf(stderr, "Arrival_time invalide, 0 utilise par defaut.\n");
            arrival = 0;
        }

        /* CREATION DU PROCESSUS */
        PCB *p = process_create((ProcessPriority)prio, burst, arrival);
        if (!p) {
            fprintf(stderr, "Erreur : impossible de creer le processus %d\n", i + 1);
            return i;  // on retourne seulement ceux créés
        }

        /* CHOIX D'EVENTUELLE I/O */
        printf("Ce processus utilisera-t-il une I/O bloquante ?\n");
        printf(" 0 = aucune I/O\n");
        printf(" 1 = PRINTER\n");
        printf(" 2 = KEYBOARD\n");
        printf(" 3 = MOUSE\n");
        printf(" 4 = DISK\n");
        printf(" 5 = SCREEN\n");
        printf(" 6 = NETWORK\n");
        printf("Choix : ");

        int io_choice = 0;
        if (scanf("%d", &io_choice) != 1) {
            io_choice = 0;
        }

        switch (io_choice) {
            case 1: p->io_device = IO_DEVICE_PRINTER;  break;
            case 2: p->io_device = IO_DEVICE_KEYBOARD; break;
            case 3: p->io_device = IO_DEVICE_MOUSE;    break;
            case 4: p->io_device = IO_DEVICE_DISK;     break;
            case 5: p->io_device = IO_DEVICE_SCREEN;   break;
            case 6: p->io_device = IO_DEVICE_NETWORK;  break;
            default:
                p->io_device = -1;  // aucune I/O
                break;
        }

        out_tasks[i] = p;
    }

    return n;
}
