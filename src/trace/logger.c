
#include "logger.h"
#include <stdlib.h>
#include <string.h>

static FILE *trace_file = NULL;

void trace_init(const char *filename) {
    trace_file = fopen(filename, "w");
    if (!trace_file) {
        perror("Erreur ouverture trace.csv");
        exit(1);
    }

    // En-tête du fichier CSV
    fprintf(trace_file, "time,pid,event,state,reason,cpu,queue\n");
    fflush(trace_file);
}

void trace_event(int time, int pid, const char *event,
                 const char *state, const char *reason,
                 int cpu, const char *queue)
{
    if (!trace_file) return;

    fprintf(trace_file, "%d,%d,%s,%s,%s,%d,%s\n",
            time, pid,
            event,
            state ? state : "",
            reason ? reason : "",
            cpu,
            queue ? queue : "");

    fflush(trace_file);
}

void trace_close() {
    if (trace_file) {
        fclose(trace_file);
        trace_file = NULL;
    }
}
