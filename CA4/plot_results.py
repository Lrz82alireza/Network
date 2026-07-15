import os
import pandas as pd
import matplotlib.pyplot as plt
import re
from datetime import datetime

# Define the 4 test scenarios including the Bonus Phase
tests = [
    {"id": 1, "name": "Test 1 (0% Loss, 0ms Delay)", "cwnd": "Junk/Logs/cwnd_test1.csv", "log": "Junk/Logs/events_test1.log", "color": "blue"},
    {"id": 2, "name": "Test 2 (10% Loss, 50ms Delay)", "cwnd": "Junk/Logs/cwnd_test2.csv", "log": "Junk/Logs/events_test2.log", "color": "orange"},
    {"id": 3, "name": "Test 3 (20% Loss, 100ms Delay)", "cwnd": "Junk/Logs/cwnd_test3.csv", "log": "Junk/Logs/events_test3.log", "color": "red"},
    {"id": 4, "name": "Test 4 (BONUS: 10% Loss, Fast Recovery)", "cwnd": "Junk/Logs/cwnd_test4.csv", "log": "Junk/Logs/events_test4.log", "color": "green"}
]

# Create output directory for plots
plot_dir = "Junk/Plots"
os.makedirs(plot_dir, exist_ok=True)

# =========================================================
# 1. Plot CWND over Time (5 Subplots: 4 Individual + 1 Combined)
# =========================================================
fig, axes = plt.subplots(5, 1, figsize=(12, 20))
fig.suptitle("Congestion Window (cwnd) over Time", fontsize=16)

# Plot individual tests on the first 4 subplots
for idx, test in enumerate(tests):
    if os.path.exists(test["cwnd"]):
        df = pd.read_csv(test["cwnd"])
        axes[idx].plot(df['Time(s)'], df['Cwnd'], label=test["name"], color=test["color"], linewidth=2)
        axes[idx].set_title(test["name"])
        axes[idx].set_xlabel("Time (seconds)")
        axes[idx].set_ylabel("CWND (Packets)")
        axes[idx].grid(True, linestyle='--', alpha=0.7)
        axes[idx].legend()

# Plot the combined overlay on the 5th subplot
axes[4].set_title("Combined Comparison (All Tests)")
for test in tests:
    if os.path.exists(test["cwnd"]):
        df = pd.read_csv(test["cwnd"])
        axes[4].plot(df['Time(s)'], df['Cwnd'], label=test["name"], color=test["color"], linewidth=2, alpha=0.8)

axes[4].set_xlabel("Time (seconds)")
axes[4].set_ylabel("CWND (Packets)")
axes[4].grid(True, linestyle='--', alpha=0.7)
axes[4].legend()

plt.tight_layout(rect=[0, 0, 1, 0.97])
plt.savefig(f"{plot_dir}/cwnd_comparison.png")
print(f"Saved CWND plot (with combined view) to {plot_dir}/cwnd_comparison.png")


# =========================================================
# 2. Parse Events Log for Throughput and Plot (5 Subplots)
# =========================================================
fig_th, axes_th = plt.subplots(5, 1, figsize=(12, 20))
fig_th.suptitle("Throughput over Time", fontsize=16)

axes_th[4].set_title("Combined Comparison (All Tests) - LOG SCALE")

for idx, test in enumerate(tests):
    if not os.path.exists(test["log"]):
        continue
        
    timestamps = []
    packets_sent = 0
    retransmissions = 0
    timeouts = 0
    
    with open(test["log"], 'r') as f:
        for line in f:
            time_match = re.search(r'\[(.*?)\]', line)
            if not time_match: continue
            
            time_str = time_match.group(1)
            try:
                t = datetime.strptime(time_str, "%H:%M:%S.%f")
                timestamps.append((t, line))
            except ValueError:
                continue
                
            if "Sent DATA packet" in line:
                packets_sent += 1
            if "Retransmitting" in line:
                retransmissions += 1
            if "Timeout occurred" in line:
                timeouts += 1

    if not timestamps: continue
    
    start_time = timestamps[0][0]
    end_time = timestamps[-1][0]
    total_duration = (end_time - start_time).total_seconds()
    
    # Dynamic bin size: Divide total time into ~40 bins (min 0.005s)
    bin_size = max(0.005, total_duration / 40.0)
    
    time_bins = {}
    for t, line in timestamps:
        if "Sent DATA packet" in line:
            elapsed = (t - start_time).total_seconds()
            bin_idx = int(elapsed / bin_size) * bin_size
            time_bins[bin_idx] = time_bins.get(bin_idx, 0) + 1024  # Assuming ~1024 bytes payload

    if time_bins:
        sorted_bins = sorted(time_bins.keys())
        throughputs = [time_bins[b] / bin_size for b in sorted_bins]
        
        # Plot on individual subplot (Rows 1-4)
        axes_th[idx].plot(sorted_bins, throughputs, label=test["name"], color=test["color"], linewidth=2, marker='o', markersize=3)
        axes_th[idx].set_title(f"{test['name']} - (Bin size: {bin_size:.3f}s)")
        axes_th[idx].set_xlabel("Time (seconds)")
        axes_th[idx].set_ylabel("Throughput (B/s)")
        axes_th[idx].set_xlim(left=0) 
        axes_th[idx].grid(True, linestyle='--', alpha=0.7)
        axes_th[idx].legend()

        # Plot on combined subplot (Row 5)
        axes_th[4].plot(sorted_bins, throughputs, label=test["name"], color=test["color"], linewidth=2, alpha=0.7, marker='o', markersize=3)

    # Print Metrics to Terminal
    print(f"\n--- {test['name']} Metrics ---")
    print(f"Total packets sent: {packets_sent}")
    print(f"Total retransmissions: {retransmissions}")
    print(f"Total timeouts: {timeouts}")

# Finalize the combined subplot (Row 5)
axes_th[4].set_xlabel("Time (seconds)")
axes_th[4].set_ylabel("Throughput (B/s) - Log Scale")
axes_th[4].set_xlim(left=0) 
axes_th[4].set_yscale('log') # <--- THE GOLDEN FIX FOR THE SCALING ISSUE
axes_th[4].grid(True, which="both", linestyle='--', alpha=0.7)
axes_th[4].legend()

plt.tight_layout(rect=[0, 0, 1, 0.97])
plt.savefig(f"{plot_dir}/throughput_comparison.png")
print(f"\nSaved Throughput plot (with combined view) to {plot_dir}/throughput_comparison.png")