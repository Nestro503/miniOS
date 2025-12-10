#ifndef MINIOS_MENU_H
#define MINIOS_MENU_H

#include "../scheduler/scheduler.h"

/**
 * Affiche le menu de démarrage :
 * 1. Lancer une simulation
 * 2. Voir la démo (fichier CSV statique)
 * Retourne 1 ou 2.
 */
int menu_start_choice(void);

SchedulingPolicy menu_choose_policy(void);
int menu_choose_quantum(void);

#endif // MINIOS_MENU_H