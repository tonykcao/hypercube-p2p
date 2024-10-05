import os
import matplotlib.pyplot as plt
import statistics
import re

def plot_histogram(hops_dict, path):
    hops_val = list(hops_dict.keys())
    counts = list(hops_dict.values())

    # Plot the histogram
    plt.bar(hops_val, counts, color='blue', alpha=0.7)
    plt.xlabel('Hops counts')
    plt.ylabel('Frequency')
    plt.title('Histogram of Peak Counts')
    plt.savefig(path)

if not os.path.exists(folder_path):
    print(f"'{folder_path}' not found")

pattern = r"([0-9]+)([A-Za-z]+)([0-9]+)\.txt"
peaks = {}
for file_name in [f for f in os.listdir(folder_path) if os.path.isfile(os.path.join(folder_path, f))]:
    mat = re.match(pattern, file_name)
    if not mat:
        continue
    client_ID:int = mat.group(1)
    client_name:str = mat.group(2)
    file_path = os.path.join(folder_path, file_name)
    with open(file_path, 'rb') as file:
        conn_count  = 0
        peak = 0
        for line_number, line_bytes in enumerate(file):
            line = line_bytes.decode('utf-8', errors='replace')
            if "msg" in line or "$" in line:
                continue
            if "Neighbor [" in line:
                conn_count += 1
            if "new client" in line:
                conn_count += 1
            if "has failed" in line:
                conn_count -= 1
            peak = max(conn_count, peak)
    peaks[peak] = peaks.get(peak, 0) + 1
    print(f"Peak: {peak}, machine: {client_name}{client_ID}")

plot_histogram(peaks, path="./peaks_256.png")



# # Create a list with repeated values based on frequencies
# data_list = [key for key, value in hops.items() for _ in range(value)]

# # Calculate the median
# median_value = statistics.median(data_list)
# mean_value = statistics.mean(data_list)
# # mean_value = sum(key*value for key, value in hops.items()) / sum(hops.values())
# print(f"mean is: {mean_value}")
# print(f"median is: {median_value}")
