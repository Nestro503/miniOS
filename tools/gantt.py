
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

def plot_gantt(csv_file):

    df = pd.read_csv(csv_file)
    df = df.sort_values(by=["pid", "time"])

    # -------- COLORS ----------
    COLORS = {
        "READY": "yellow",
        "RUNNING": "green",
        "BLOCKED": "red",
        "TERMINATED": "black"
    }

    # -------- FIGURE ----------
    fig, (ax_gantt, ax_cpu) = plt.subplots(
        2, 1, figsize=(14, 8),
        gridspec_kw={'height_ratios': [3, 1]},
        sharex=True
    )
    fig.suptitle("MiniOS — Diagramme de Gantt & Utilisation CPU", fontsize=16)

    pids = sorted(df["pid"].unique())
    y_positions = {pid: i for i, pid in enumerate(pids)}

    # -------- GANTT ----------
    for pid in pids:
        logs = df[df.pid == pid].reset_index(drop=True)

        for i in range(len(logs) - 1):
            start = logs.loc[i, "time"]
            end = logs.loc[i + 1, "time"]
            state = logs.loc[i, "state"]

            ax_gantt.barh(
                y_positions[pid],
                end - start,
                left=start,
                color=COLORS.get(state, "gray"),
                edgecolor="black",
                height=0.6
            )

    ax_gantt.set_yticks(list(y_positions.values()))
    ax_gantt.set_yticklabels([str(pid) for pid in pids])
    ax_gantt.set_ylabel("PID")

    legend_elements = [
        Patch(facecolor=COLORS["READY"], label="READY"),
        Patch(facecolor=COLORS["RUNNING"], label="RUNNING"),
        Patch(facecolor=COLORS["BLOCKED"], label="BLOCKED"),
        Patch(facecolor=COLORS["TERMINATED"], label="TERMINATED"),
    ]
    ax_gantt.legend(handles=legend_elements, loc="upper right")

    # -------- CPU TIMELINE ----------
    timeline = []
    for i in range(len(df)-1):
        if df.loc[i, "state"] == "RUNNING":
            timeline.append((df.loc[i, "time"], df.loc[i + 1, "time"]))

    for start, end in timeline:
        ax_cpu.plot([start, end], [1, 1], linewidth=8, color="green")

    # CPU Idle = zones sans RUNNING
    ax_cpu.set_ylim(0, 1.3)
    ax_cpu.set_yticks([0, 1])
    ax_cpu.set_yticklabels(["IDLE", "ACTIVE"])
    ax_cpu.set_xlabel("Temps")

    # -------- CONTEXT SWITCHES ----------
    switches = df[df.state == "RUNNING"].shape[0] - 1
    ax_cpu.text(0.02, 1.15, f"Context switches: {switches}", fontsize=12)

    plt.tight_layout()
    plt.show()


plot_gantt("trace/trace.csv")
