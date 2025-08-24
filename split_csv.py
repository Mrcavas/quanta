import sys
from os import path

assert len(sys.argv) > 1, "дай аргумент на инпут"

input_path = path.abspath(sys.argv[1])
directory = path.dirname(input_path)
out_acc_path = path.join(directory, "acc_data.csv")
out_mag_path = path.join(directory, "mag_data.csv")

with open(input_path) as input_file:
    with open(out_acc_path, "w") as acc_file:
        with open(out_mag_path, "w") as mag_file:
            for line in input_file.readlines():
                ax, ay, az, mx, my, mz = line.split(", ")
                acc_file.write(f"{ax}, {ay}, {az}\n")
                mag_file.write(f"{mx}, {my}, {mz}")

print("done")

