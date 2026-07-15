import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re
from datetime import datetime

log_standard = "Junk/Logs/events_standard.log"
log_bonus = "Junk/Logs/events_bonus.log"
cwnd_bonus = "Junk/Logs/cwnd_bonus.csv"
plot_dir = "Junk/Plots"

def parse_bonus_log(log_path):
    metrics = {'timeouts': 0, 'fast_retransmits': 0, 'time': 0.0, 'fr_relative_times': []}
    start_time = None
    
    if not os.path.exists(log_path): return metrics
    
    with open(log_path, 'r') as f:
        for line in f:
            time_match = re.search(r'\[(.*?)\]', line)
            if not time_match: continue
            
            try:
                t = datetime.strptime(time_match.group(1), "%H:%M:%S.%f")
            except ValueError:
                continue

            if "Starting transmission" in line:
                start_time = t
            elif "Timeout occurred" in line:
                metrics['timeouts'] += 1
            elif "Triggering Fast Retransmit" in line:
                metrics['fast_retransmits'] += 1
                if start_time:
                    metrics['fr_relative_times'].append((t - start_time).total_seconds())
            elif "Total transfer time:" in line:
                match = re.search(r'Total transfer time:\s*([\d\.]+)', line)
                if match:
                    metrics['time'] = float(match.group(1))
                    
    return metrics

# Parse Logs
std_metrics = parse_bonus_log(log_standard)
fast_metrics = parse_bonus_log(log_bonus)

# Print Terminal Report
print(f"\n--- BONUS PHASE REPORT ---")
print(f"Standard Mode -> Timeouts: {std_metrics['timeouts']}, Transfer Time: {std_metrics['time']:.2f}s")
print(f"Fast Mode     -> Timeouts: {fast_metrics['timeouts']}, Transfer Time: {fast_metrics['time']:.2f}s")
print(f"Fast Retransmit events triggered: {fast_metrics['fast_retransmits']}")

# Plotting
fig, axes = plt.subplots(1, 3, figsize=(18, 6), gridspec_kw={'width_ratios': [2, 1, 1]})
fig.suptitle("Bonus Phase Analysis: Fast Retransmit & Fast Recovery", fontsize=18, fontweight='bold')

# 1. Plot CWND with Fast Retransmit Markers
if os.path.exists(cwnd_bonus):
    df = pd.read_csv(cwnd_bonus)
    axes[0].plot(df['Time(s)'], df['Cwnd'], label='Cwnd (Fast Mode)', color='blue', linewidth=2)
    
    fr_times = fast_metrics['fr_relative_times']
    if fr_times:
        # Interpolate to find exact Cwnd value at the time of Fast Retransmit for plotting
        fr_cwnds = np.interp(fr_times, df['Time(s)'], df['Cwnd'])
        axes[0].scatter(fr_times, fr_cwnds, color='red', marker='X', s=150, zorder=5, label='Fast Retransmit Triggered')
        
    axes[0].set_title("Cwnd over Time with Fast Retransmit Events", fontsize=14)
    axes[0].set_xlabel("Time (seconds)")
    axes[0].set_ylabel("CWND (Packets)")
    axes[0].grid(True, linestyle='--', alpha=0.7)
    axes[0].legend(fontsize=12)

# 2. Compare Timeouts (Bar Chart)
modes = ['Standard Mode', 'Fast Mode']
timeouts = [std_metrics['timeouts'], fast_metrics['timeouts']]
bars1 = axes[1].bar(modes, timeouts, color=['gray', 'green'], alpha=0.8)
axes[1].set_title("Timeouts Comparison", fontsize=14)
axes[1].set_ylabel("Number of Timeouts")
for bar in bars1:
    yval = bar.get_height()
    axes[1].text(bar.get_x() + bar.get_width()/2, yval + 0.1, int(yval), ha='center', va='bottom', fontweight='bold')

# 3. Compare Transfer Time (Bar Chart)
times = [std_metrics['time'], fast_metrics['time']]
bars2 = axes[2].bar(modes, times, color=['gray', 'purple'], alpha=0.8)
axes[2].set_title("Transfer Time Comparison", fontsize=14)
axes[2].set_ylabel("Time (seconds)")
for bar in bars2:
    yval = bar.get_height()
    axes[2].text(bar.get_x() + bar.get_width()/2, yval + 0.1, f"{yval:.2f}s", ha='center', va='bottom', fontweight='bold')

plt.tight_layout(rect=[0, 0, 1, 0.95])
output_img = f"{plot_dir}/bonus_analysis.png"
plt.savefig(output_img)
print(f"\nSaved Bonus Analysis plot to {output_img}")