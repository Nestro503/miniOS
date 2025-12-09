#include <stdio.h>
#include "scenario.h"

/**
 * Construit un scénario interactif :
 * - demande à l'utilisateur combien de processus créer
 * - pour chacun : priorité, burst CPU, instant d'arrivée
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

        /* ================================
           GESTION DE LA PRIORITÉ SELON LA POLITIQUE
           ================================ */
        if (policy == SCHED_ROUND_ROBIN) {
            // Pour RR, la priorité n'existe pas → MEDIUM forcé
            printf("Round Robin choisi -> Priorite ignoree, MEDIUM appliquee automatiquement.\n");
            prio = PRIORITY_MEDIUM;
        }
        else {
            printf("Priorite (0 = LOW, 1 = MEDIUM, 2 = HIGH) : ");
            if (scanf("%d", &prio) != 1 ||
                prio < PRIORITY_LOW ||
                prio > PRIORITY_HIGH)
            {
                fprintf(stderr, "Priorite invalide, MEDIUM utilisee.\n");
                prio = PRIORITY_MEDIUM;
            }
        }

        /* ================================
           BURST CPU
           ================================ */
        printf("Burst CPU (duree > 0) : ");
        if (scanf("%d", &burst) != 1 || burst <= 0) {
            fprintf(stderr, "Burst invalide, 5 utilisé par defaut.\n");
            burst = 5;
        }

        /* ================================
           ARRIVÉE
           ================================ */
        printf("Instant d'arrivee (>= 0) : ");
        if (scanf("%d", &arrival) != 1 || arrival < 0) {
            fprintf(stderr, "Arrival_time invalide, 0 utilisé par defaut.\n");
            arrival = 0;
        }

        /* ================================
           CRÉATION DU PROCESSUS
           ================================ */
        PCB *p = process_create((ProcessPriority)prio, burst, arrival);
        if (!p) {
            fprintf(stderr, "Erreur : impossible de creer le processus %d\n", i + 1);
            return i;  // on retourne seulement ceux créés
        }

        out_tasks[i] = p;
    }

    return n;
}