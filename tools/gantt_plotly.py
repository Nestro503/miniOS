import pandas as pd
import numpy as np
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import sys  # <--- Ajouté pour récupérer l'argument du C

# ==========================================================
# 1. CHARGEMENT ET PRÉPARATION
# ==========================================================
def load_trace(csv_file):
    try:
        df = pd.read_csv(csv_file)
    except FileNotFoundError:
        print(f"Erreur : Le fichier {csv_file} est introuvable.")
        sys.exit(1)
    except pd.errors.EmptyDataError:
        print(f"Erreur : Le fichier {csv_file} est vide.")
        sys.exit(0)

    # Sécurisation si la colonne 'reason' n'existe pas (anciens fichiers traces)
    if 'reason' not in df.columns:
        df['reason'] = ""

    df['reason'] = df['reason'].astype(str)
    df = df.sort_values(by=["time", "event"])
    return df

def process_intervals(df):
    intervals = []
    # On ignore les événements MEMORY pour le Gantt
    state_df = df[~df['event'].isin(['MEMORY'])]

    for pid, group in state_df.groupby("pid"):
        # Trier par temps pour reconstruire la chronologie
        group = group.sort_values("time")
        rows = group.to_dict('records')

        for i in range(len(rows) - 1):
            start_row = rows[i]
            end_row = rows[i+1]
            t0 = start_row['time']
            t1 = end_row['time']
            state = start_row['state']

            # Gestion du 'reason' : éviter d'afficher 'nan'
            r_val = start_row['reason']
            reason = r_val if r_val != 'nan' and r_val != 'None' else ""

            if t1 > t0:
                intervals.append({
                    "pid": pid, "state": state, "start": t0, "end": t1,
                    "duration": t1 - t0, "reason": reason
                })
    return pd.DataFrame(intervals)

# ==========================================================
# 2. CALCUL MÉMOIRE & STATS
# ==========================================================
def compute_memory_curve(df, max_time):
    # Création d'une timeline vide
    timeline_x = np.arange(int(max_time) + 2)
    timeline_y = np.zeros(len(timeline_x))
    current_mem = 0

    mem_events = df[df['event'] == 'MEMORY'].sort_values(by="time")
    if mem_events.empty: return timeline_x, timeline_y

    last_time = 0
    for _, row in mem_events.iterrows():
        t = int(row['time'])
        action = row['state'] # ALLOC ou FREE

        # Remplir la période précédente avec la valeur courante
        if t < len(timeline_y):
            timeline_y[last_time:t+1] = current_mem

        try: size = int(float(row['reason']))
        except: size = 0

        if action == "ALLOC": current_mem += size
        elif action == "FREE": current_mem = max(0, current_mem - size)

        last_time = t

    # Remplir jusqu'à la fin
    if last_time < len(timeline_y):
        timeline_y[last_time:] = current_mem

    return timeline_x, timeline_y

def compute_detailed_stats(df_intervals):
    stats = {}
    total_ctx_switches = 0

    if df_intervals.empty:
        return pd.DataFrame(), 0

    pids = sorted(df_intervals['pid'].unique())

    for pid in pids:
        # On filtre par PID
        proc = df_intervals[df_intervals['pid'] == pid]

        switches = len(proc[proc['state'] == 'RUNNING'])
        total_ctx_switches += switches

        t_start = proc['start'].min()
        t_end = proc['end'].max()

        execution = proc[proc['state'] == 'RUNNING']['duration'].sum()
        attente = proc[proc['state'] == 'READY']['duration'].sum()
        blocage = proc[proc['state'] == 'BLOCKED']['duration'].sum()

        first_run = proc[proc['state'] == 'RUNNING']['start'].min()
        reponse = first_run - t_start if pd.notna(first_run) else 0
        turnaround = t_end - t_start

        stats[pid] = {
            "PID": pid,
            "Exécution": execution,
            "Attente": attente,
            "Blocage": blocage,
            "T. Réponse": reponse,
            "Turnaround": turnaround
        }
    return pd.DataFrame(stats).T.sort_index(), total_ctx_switches

# ==========================================================
# 3. VISUALISATION (v17 - Légende Compacte & Rapprochée)
# ==========================================================
def plot_minios_dashboard(csv_file):
    print(f"Génération du graphique pour : {csv_file} ...")
    raw_df = load_trace(csv_file)

    # Sécurisation si le DF est vide après chargement
    if raw_df.empty:
        print("Aucune donnée à afficher.")
        return

    df_intervals = process_intervals(raw_df)

    if df_intervals.empty:
        print("Aucun intervalle de temps valide trouvé.")
        return

    max_time = df_intervals['end'].max()
    stats_df, total_switches = compute_detailed_stats(df_intervals)
    mem_x, mem_y = compute_memory_curve(raw_df, max_time)

    sorted_pids = sorted(df_intervals['pid'].unique())
    y_labels_gantt = [f"PID {p}" for p in sorted_pids]

    # --- SETUP SUBPLOTS ---
    row_heights = [0.30, 0.15, 0.15, 0.40]

    fig = make_subplots(
        rows=4, cols=1,
        shared_xaxes=False,
        vertical_spacing=0.08,
        row_heights=row_heights,
        specs=[[{"type": "xy"}], [{"type": "xy"}], [{"type": "xy"}], [{"type": "xy"}]],
        subplot_titles=(
            "Diagramme de Gantt",
            "Utilisation CPU globale",
            "Utilisation Mémoire",
            "Comparaison des Performances par Processus"
        )
    )

    # --- COULEURS PASTELS ---
    gantt_colors = {"READY": "#FFD54F", "RUNNING": "#81C784", "BLOCKED": "#E57373", "TERMINATED": "#B0BEC5"}

    stats_colors = {
        "Exécution": "#A5D6A7",   # Vert pastel
        "Attente": "#FFF59D",     # Jaune pastel
        "Blocage": "#EF9A9A",     # Rouge pastel
        "T. Réponse": "#90CAF9",  # Bleu pastel
        "Turnaround": "#CE93D8"   # Violet pastel
    }

    legend_shown = set()

    # --- 1. GANTT ---
    for _, row in df_intervals.iterrows():
        state = row['state']
        show_leg = state not in legend_shown
        if show_leg: legend_shown.add(state)
        reason_txt = f" ({row['reason']})" if row['reason'] else ""

        fig.add_trace(go.Bar(
            x=[row['duration']], y=[f"PID {row['pid']}"], base=row['start'], orientation='h',
            marker=dict(color=gantt_colors.get(state, "#90A4AE"), line=dict(color='white', width=1)),
            name=state,
            legendgroup=state,
            showlegend=show_leg,
            hovertemplate=f"<b>PID {row['pid']}</b><br>État: {state}{reason_txt}<br>Durée: {row['duration']}s<extra></extra>",
            width=0.8,
        ), row=1, col=1)

    # --- 2. CPU ---
    cpu_x = np.arange(int(max_time) + 1)
    cpu_y = np.zeros(len(cpu_x))
    # On remplit à 100% quand n'importe quel PID est RUNNING
    for _, row in df_intervals[df_intervals['state'] == 'RUNNING'].iterrows():
        cpu_y[int(row['start']):int(row['end'])] = 100

    fig.add_trace(go.Scatter(
        x=cpu_x, y=cpu_y, mode='lines', fill='tozeroy',
        line=dict(color='#64B5F6', width=2), name='CPU %', showlegend=False,
        hovertemplate="CPU: %{y}%<extra></extra>"
    ), row=2, col=1)

    # --- 3. MÉMOIRE ---
    fig.add_trace(go.Scatter(
        x=mem_x, y=mem_y, mode='lines', fill='tozeroy',
        line=dict(color='#BA68C8', width=2), name='Mémoire', showlegend=False,
        hovertemplate="Mem: %{y} octets<extra></extra>"
    ), row=3, col=1)

    # --- 4. STATS ---
    if not stats_df.empty:
        metrics_to_plot = ["Exécution", "Attente", "Blocage", "T. Réponse", "Turnaround"]
        x_pids = [f"PID {p}" for p in sorted(stats_df.index)]

        for i, metric in enumerate(metrics_to_plot):
            fig.add_trace(go.Bar(
                x=x_pids, y=stats_df[metric], name=metric,
                marker_color=stats_colors[metric],
                showlegend=False,
                hovertemplate=f"<b>%{{x}}</b><br>{metric}: %{{y}} unités<extra></extra>",
                offsetgroup=i
            ), row=4, col=1)

        # --- LÉGENDE DESSINÉE (BAS) - RAPPROCHÉE & CONDENSÉE ---
        legend_y_pos = -0.07 # Remontée vers le graphique (était -0.10)

        # Paramètres de condensation
        start_x = 0.18  # Décalage vers la droite pour recentrer le tout (était 0.10)
        gap = 0.14      # Ecart réduit entre les items (était 0.18)

        for i, metric in enumerate(metrics_to_plot):
            x_pos = start_x + (i * gap)

            # Carré
            fig.add_shape(type="rect",
                          x0=x_pos, x1=x_pos + 0.015,
                          y0=legend_y_pos, y1=legend_y_pos + 0.02,
                          xref="paper", yref="paper",
                          fillcolor=stats_colors[metric], line=dict(width=0)
                          )

            # Texte (parfaitement aligné verticalement avec le carré)
            fig.add_annotation(
                x=x_pos + 0.025,
                y=legend_y_pos + 0.01,
                xref="paper", yref="paper",
                text=metric,
                showarrow=False,
                font=dict(size=12),
                xanchor="left",
                yanchor="middle",
                align="left"
            )

    # --- LAYOUT GLOBAL ---
    fig.update_layout(
        title_text="<b>Analyse MiniOS — Rapport d'Exécution Complet</b>",
        title_x=0.5, height=1300, template="plotly_white",
        barmode='overlay',
        margin=dict(t=100, b=100, l=50, r=50), # Marge du bas ajustée

        # --- LÉGENDE DU HAUT (GANTT) ---
        legend=dict(
            orientation="v",
            y=1.00, yanchor="top",
            x=1.00, xanchor="right",
            bgcolor="rgba(255,255,255,0.9)",
            bordercolor="LightGray",
            borderwidth=1,
            traceorder="normal"
        ),
        hovermode="closest"
    )

    # --- ANNOTATION SWITCH CONTEXTE ---
    fig.add_annotation(
        text=f"Switchs contexte: <b>{total_switches}</b>",
        xref="paper", yref="paper",
        x=1, y=1.03,
        showarrow=False,
        font=dict(size=12, color="#555"),
        align="right"
    )

    # --- AXES & GRILLES ---
    fig.update_yaxes(categoryorder='array', categoryarray=y_labels_gantt[::-1], row=1, col=1)
    fig.update_yaxes(title_text="Octets", row=3, col=1, rangemode="tozero")

    fig.update_xaxes(title_text="", row=4, col=1)
    fig.update_yaxes(title_text="Durée (unités)", row=4, col=1)

    for r in [1, 2, 3]:
        fig.update_xaxes(showticklabels=True, showgrid=True, gridwidth=1, gridcolor='#ECEFF1', row=r, col=1)

    fig.update_xaxes(matches='x', row=1, col=1)
    fig.update_xaxes(matches='x', row=2, col=1)
    fig.update_xaxes(matches='x', row=3, col=1)

    fig.show()

# ==========================================================
# 4. POINT D'ENTRÉE ADAPTÉ AU C (Gestion des arguments)
# ==========================================================
if __name__ == "__main__":
    # Valeur par défaut si aucun argument n'est fourni
    file_path = "tools/trace/trace.csv"

    # Si le programme C envoie un argument (ex: "tools/trace/demo.csv"), on l'utilise
    if len(sys.argv) > 1:
        file_path = sys.argv[1]

    plot_minios_dashboard(file_path)