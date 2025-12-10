#include <stdio.h>
#include "menu.h"

int menu_start_choice(void) {
    int choice = 0;
    printf("\n==========================================\n");
    printf("        BIENVENUE DANS MINIOS             \n");
    printf("==========================================\n");
    printf("1 - Lancer une nouvelle simulation interactive\n");
    printf("2 - Visualiser le graphique de DEMO (.csv)\n");
    printf("Votre choix : ");

    if (scanf("%d", &choice) != 1) {
        // En cas d'erreur de saisie (lettre au lieu de chiffre), on vide le buffer
        while(getchar() != '\n');
        return 1; // Par défaut : simulation
    }

    if (choice == 2) return 2;
    return 1;
}

SchedulingPolicy menu_choose_policy(void) {
    int choice = 0;

    printf("\n=== MiniOS - Choix de la politique d'ordonnancement ===\n");
    printf("1 - Round Robin (preemptif, a quantum)\n");
    printf("2 - Priorite statique (preemptif, sans quantum)\n");
    printf("3 - Priorite + Round Robin (preemptif entre priorites, a quantum)\n");
    printf("Votre choix : ");

    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "Entree invalide, PRIORITY par defaut.\n");
        return SCHED_PRIORITY;
    }

    switch (choice) {
        case 1: return SCHED_ROUND_ROBIN;
        case 2: return SCHED_PRIORITY;
        case 3: return SCHED_P_RR;
        default:
            fprintf(stderr, "Choix inconnu, PRIORITY par defaut.\n");
            return SCHED_PRIORITY;
    }
}

/* Demander le quantum a l'utilisateur */
int menu_choose_quantum(void) {
    int q = 0;

    printf("\n=== Configuration du quantum ===\n");
    printf("Entrez la valeur du quantum (entier > 0) : ");

    if (scanf("%d", &q) != 1 || q <= 0) {
        fprintf(stderr, "Quantum invalide, utilisation de la valeur 2 par defaut.\n");
        return 2;
    }
    return q;
}