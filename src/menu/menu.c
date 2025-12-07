#include <stdio.h>
#include "menu.h"

SchedulingPolicy menu_choose_policy(void) {
    int choice = 0;

    printf("=== MiniOS - Choix de la politique d'ordonnancement ===\n");
    printf("1 - Round Robin (non preemptif)\n");
    printf("2 - Priorite statique (preemptif)\n");
    printf("3 - Priorite + Round Robin (preemptif entre priorites)\n");
    printf("Votre choix : ");

    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "Entree invalide, utilisation de PRIORITY par defaut.\n");
        return SCHED_PRIORITY;
    }

    switch (choice) {
        case 1: return SCHED_ROUND_ROBIN;
        case 2: return SCHED_PRIORITY;
        case 3: return SCHED_P_RR;
        default:
            fprintf(stderr, "Choix inconnu, utilisation de PRIORITY par defaut.\n");
        return SCHED_PRIORITY;
    }
}
