#ifndef MINIOS_MENU_H
#define MINIOS_MENU_H

#include "../scheduler/scheduler.h"

SchedulingPolicy menu_choose_policy(void);
int menu_choose_quantum(void);

#endif // MINIOS_MENU_H
