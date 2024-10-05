import os
import matplotlib.pyplot as plt
import statistics

def plot_histogram(hops_dict, path):
    hops_val = list(hops_dict.keys())
    counts = list(hops_dict.values())

    # Plot the histogram
    plt.bar(hops_val, counts, color='blue', alpha=0.7)
    plt.xlabel('Hops counts')
    plt.ylabel('Frequency')
    plt.title('Histogram of Hops Counts')
    plt.savefig(path)

if not os.path.exists(folder_path):
    print(f"'{folder_path}' not found")

hops = {}
sends = 0
for file_name in [f for f in os.listdir(folder_path) if os.path.isfile(os.path.join(folder_path, f))]:
    file_path = os.path.join(folder_path, file_name)
    # print(file_name)
    with open(file_path, 'rb') as file:
        for line_number, line_bytes in enumerate(file):
            line = line_bytes.decode('utf-8', errors='replace')
            if "| hops: " in line:
                try:
                    i = int(line.split("hops: ")[1])+1
                    hops[i] = hops.get(i, 0) + 1
                except:
                    pass
            if "Initial Sender" in line:
                sends+=1
print(sends)
print(hops)
plot_histogram(hops, path="./histogram.png")
# Create a list with repeated values based on frequencies
data_list = [key for key, value in hops.items() for _ in range(value)]

# Calculate the median
median_value = statistics.median(data_list)
mean_value = statistics.mean(data_list)
# mean_value = sum(key*value for key, value in hops.items()) / sum(hops.values())
print(f"mean is: {mean_value}")
print(f"median is: {median_value}")
