
import pandas as pd
import matplotlib.pyplot as plt

def plot_cpu(csv_file):
    df = pd.read_csv(csv_file)

    plt.figure(figsize=(12, 3))
    plt.step(df['time'], df['cpu'], where='post')

    plt.xlabel("Temps")
    plt.ylabel("CPU PID")
    plt.title("Activité CPU dans le temps")
    plt.grid()
    plt.show()

if __name__ == "__main__":
    plot_cpu("trace.csv")
