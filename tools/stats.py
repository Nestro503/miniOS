
import pandas as pd

def compute_stats(csv_file):
    df = pd.read_csv(csv_file)

    processes = df['pid'].unique()
    print("=== STATISTIQUES MINI OS ===")

    for pid in processes:
        p = df[df['pid'] == pid]

        arrival = p.iloc[0]['time']
        end = p[p['state'] == "TERMINATED"].iloc[-1]['time']

        turnaround = end - arrival

        running_time = p[p['state'] == "RUNNING"]
        cpu_time = 0
        for i in range(len(running_time) - 1):
            cpu_time += running_time.iloc[i+1]['time'] - running_time.iloc[i]['time']

        print(f"PID {pid}:")
        print(f"  → Turnaround time : {turnaround}")
        print(f"  → CPU time       : {cpu_time}")
        print()

if __name__ == "__main__":
    compute_stats("trace.csv")
