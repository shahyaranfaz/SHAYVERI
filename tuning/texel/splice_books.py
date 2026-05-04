import random

def read_file(path):
    with open(path) as f:
        return [line.strip() for line in f if line.strip()]

a = read_file("dataset.txt")              # your data
b = read_file("ethereal_converted.txt")  # converted ethereal

merged = a + b
random.shuffle(merged)

with open("texel_dataset.txt", "w") as f:
    f.write("\n".join(merged))