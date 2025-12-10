import pandas as pd
import matplotlib.pyplot as plt
import numpy as np


# ==========================================================
#               CHARGEMENT DU TRACE CSV
# ==========================================================
def load_trace(csv_file):
    df = pd.read_csv(csv_file)
    df = df.sort_values(by=["time"])
    return df


# ==========================================================
#       RECONSTRUCTION D’UNE TIMELINE MONOPROCESSEUR
# ==========================================================
def build_cpu_timeline(df):
    """
    Construit :
    - cpu_usage[t] = 1 si CPU RUNNING, sinon 0
    - running_intervals = liste des intervalles (start, duration)
    """

    max_time = int(df["time"].max()) + 2
    cpu_usage = np.zeros(max_time)

    running_intervals = []

    current_pid = None
    start_time = None

    for i in range(len(df)):
        row = df.iloc[i]
        t = int(row["time"])
        pid = row["pid"]
        state = row["state"]

        # Début RUNNING
        if state == "RUNNING":
            # Fin RUNNING précédente
            if current_pid is not None:
                running_intervals.append((start_time, t - start_time))
            current_pid = pid
            start_time = t

        # Fin RUNNING
        elif state in ["READY", "BLOCKED", "TERMINATED"]:
            if current_pid == pid:
                running_intervals.append((start_time, t - start_time))
                current_pid = None
                start_time = None

    # RUN se terminant à la fin
    if current_pid is not None:
        running_intervals.append((start_time, max_time - start_time))

    # Construire la timeline CPU
    for start, dur in running_intervals:
        for t in range(start, start + dur):
            cpu_usage[t] = 1

    return cpu_usage, running_intervals


# ==========================================================
#               STATISTIQUES PAR PROCESSUS
# ==========================================================
def compute_stats(df, cpu_usage):
    stats = {}
    pids = df["pid"].unique()

    for pid in pids:
        proc = df[df["pid"] == pid]

        t_start = proc["time"].min()

        # Temps de fin
        term = proc[proc["state"] == "TERMINATED"]
        t_end = term["time"].iloc[0] if len(term) else proc["time"].max()

        # Temps de réponse
        ready = proc[proc["state"] == "READY"]
        running = proc[proc["state"] == "RUNNING"]

        if len(ready) > 0 and len(running) > 0:
            t_first_ready = ready["time"].min()
            t_first_run = running["time"].min()
            response = t_first_run - t_first_ready
        else:
            response = None

        # Turnaround
        turnaround = t_end - t_start

        # Temps d'attente (approx)
        ready_times = ready["time"].values
        waiting = ready_times[-1] - ready_times[0] if len(ready_times) > 1 else 0

        stats[pid] = {
            "start": t_start,
            "end": t_end,
            "response": response,
            "turnaround": turnaround,
            "waiting": waiting
        }

    cpu_percent = (np.sum(cpu_usage) / len(cpu_usage)) * 100

    return stats, cpu_percent


# ==========================================================
#             CONTEXT SWITCHES
# ==========================================================
def count_context_switches(df):
    running = df[df["state"] == "RUNNING"].sort_values(by="time")
    switches = 0
    last_pid = None

    for _, row in running.iterrows():
        pid = row["pid"]
        if last_pid is not None and pid != last_pid:
            switches += 1
        last_pid = pid

    return switches


# ==========================================================
#               DIAGRAMME DE GANTT
# ==========================================================
def plot_gantt(df, ax):
    colors = {
        "READY": "yellow",
        "RUNNING": "green",
        "BLOCKED": "red",
        "TERMINATED": "black"
    }

    pids = sorted(df["pid"].unique())

    for pid in pids:
        proc = df[df["pid"] == pid].sort_values(by="time")

        for i in range(len(proc) - 1):
            t0 = proc.iloc[i]["time"]
            t1 = proc.iloc[i + 1]["time"]
            state = proc.iloc[i]["state"]
            ax.barh(pid, t1 - t0, left=t0, color=colors.get(state, "gray"))

        # Marqueur TERMINATED
        term = proc[proc["state"] == "TERMINATED"]
        if len(term) > 0:
            ax.scatter(term.iloc[0]["time"], pid, color="black", s=40)

    ax.invert_yaxis()
    ax.set_ylabel("PID")
    ax.set_title("Diagramme de Gantt — MiniOS")
    ax.grid(True, axis="x", linestyle="--", alpha=0.4)

    ax.legend([
        plt.Rectangle((0, 0), 1, 1, color="yellow"),
        plt.Rectangle((0, 0), 1, 1, color="green"),
        plt.Rectangle((0, 0), 1, 1, color="red"),
        plt.Rectangle((0, 0), 1, 1, color="black"),
    ], ["READY", "RUNNING", "BLOCKED", "TERMINATED"])


# ==========================================================
#           GRAPH CPU TEMPORAL (% UTILISATION)
# ==========================================================
def plot_cpu_usage(cpu_usage, ax):
    ax.plot(cpu_usage * 100)
    ax.set_xlabel("Temps")
    ax.set_ylabel("% CPU")
    ax.set_ylim(0, 110)
    ax.set_title("Utilisation CPU (%)")
    ax.grid(True, linestyle="--", alpha=0.5)


# ==========================================================
#         GRAPHIQUE : ÉVOLUTION DES METRIQUES OS
# ==========================================================
def plot_metrics_over_time(df, cpu_usage, stats):
    max_time = len(cpu_usage)
    times = np.arange(max_time)

    cpu_util = np.zeros(max_time)
    avg_response = []
    avg_turnaround = []

    for t in times:
        cpu_util[t] = (np.sum(cpu_usage[:t+1]) / (t+1)) * 100

        responses = [s['response'] for s in stats.values() if s['response'] is not None and s['start'] <= t]
        avg_response.append(np.mean(responses) if responses else None)

        turns = [s['turnaround'] for s in stats.values() if s['end'] <= t]
        avg_turnaround.append(np.mean(turns) if turns else None)

    plt.figure(figsize=(14, 6))
    plt.plot(times, cpu_util, label="CPU Utilisation (%)")
    plt.plot(times, avg_response, label="Temps moyen de réponse")
    plt.plot(times, avg_turnaround, label="Temps moyen de retour")

    plt.title("Évolution des métriques OS dans le temps")
    plt.xlabel("Temps")
    plt.ylabel("Valeur")
    plt.grid(True, linestyle="--", alpha=0.5)
    plt.legend()
    plt.tight_layout()
    plt.show()


# ==========================================================
#                     MAIN
# ==========================================================
def plot_from_csv(csv_file):
    df = load_trace(csv_file)

    cpu_usage, intervals = build_cpu_timeline(df)
    stats, cpu_percent = compute_stats(df, cpu_usage)
    switches = count_context_switches(df)

    print("========== STATISTIQUES MINI-OS ==========")
    print(f"Taux d'utilisation CPU : {cpu_percent:.2f}%")
    print(f"Context switches : {switches}")

    for pid, s in stats.items():
        print(f"\nPID {pid}:")
        print(f"  Temps de réponse   : {s['response']}")
        print(f"  Turnaround         : {s['turnaround']}")
        print(f"  Temps d'attente    : {s['waiting']}")

    # =============================================================
    #      NOUVELLE FIGURE UNIQUE : GANTT + COURBES DE STAT
    # =============================================================

    fig, (ax_gantt, ax_stats) = plt.subplots(
        2, 1,
        figsize=(16, 10),
        height_ratios=[3, 2],   # Plus de place pour le Gantt
        sharex=True             # Axes alignés sur le temps
    )

    # ----------------- GANTT (haut) --------------------
    plot_gantt(df, ax_gantt)

    # ----------------- CALCUL METRIQUES SUR TEMPS --------------------
    max_time = len(cpu_usage)
    times = np.arange(max_time)

    cpu_util_curve = np.zeros(max_time)
    avg_response = []
    avg_turnaround = []

    for t in times:
        # CPU %
        cpu_util_curve[t] = (np.sum(cpu_usage[:t+1]) / (t+1)) * 100

        # Temps réponse
        responses = [s['response'] for s in stats.values()
                     if s['response'] is not None and s['start'] <= t]
        avg_response.append(np.mean(responses) if responses else None)

        # Temps turnaround
        turns = [s['turnaround'] for s in stats.values()
                 if s['end'] <= t]
        avg_turnaround.append(np.mean(turns) if turns else None)

    # ----------------- COURBES (bas) --------------------
    ax_stats.plot(times, cpu_util_curve, label="CPU Utilisation (%)", linewidth=2)
    ax_stats.plot(times, avg_response, label="Temps moyen de réponse", linewidth=2)
    ax_stats.plot(times, avg_turnaround, label="Temps moyen de retour", linewidth=2)

    ax_stats.set_xlabel("Temps")
    ax_stats.set_ylabel("Valeur")
    ax_stats.set_title("Évolution des métriques de performance du MiniOS")
    ax_stats.grid(True, linestyle="--", alpha=0.4)
    ax_stats.legend()

    plt.tight_layout()
    plt.show()


# Lancer
if __name__ == "__main__":
    plot_from_csv("trace/trace.csv")

