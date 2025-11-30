
#ifndef MINIOS_LOGGER_H
#define MINIOS_LOGGER_H

#include <stdio.h>

// Initialise le fichier trace.csv
void trace_init(const char *filename);

// Enregistre un événement dans trace.csv
void trace_event(int time, int pid,
                 const char *event,
                 const char *state,
                 const char *reason,
                 int cpu,
                 const char *queue);

// Ferme le fichier
void trace_close();

#endif //MINIOS_LOGGER_H
