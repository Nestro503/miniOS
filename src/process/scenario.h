//
// Created by CAMFI on 09/12/2025.
//

#ifndef SCENARIO_H
#define SCENARIO_H


#ifndef MINIOS_SCENARIO_H
#define MINIOS_SCENARIO_H

#include "process.h"
#include "../scheduler/scheduler.h"

// Construit un scenario interactif :
// - out_tasks : tableau de PCB* (fourni par main)
// - max_tasks : taille max du tableau
// Retourne : nombre de processus effectivement créés.
int scenario_build_interactive(PCB **out_tasks, int max_tasks, SchedulingPolicy policy);

#endif // MINIOS_SCENARIO_H



#endif //SCENARIO_H
