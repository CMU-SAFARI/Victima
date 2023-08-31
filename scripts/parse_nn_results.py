import re
import sys

with open(sys.argv[1], "r") as f:
    lines = f.readlines()
    csv_data = [
        "Model,Dim of Input,Dim of Output,Recall,Accuracy,Precision,F1-Score"]

    for i in range(0, len(lines), 6):
        model_line = lines[i]
        dim_input = re.search(r"dim of input: (\d+)", model_line).group(1)
        dim_output = re.search(r"dim of output: (\d+)", model_line).group(1)
        recall = lines[i+1].split(": ")[1]
        accuracy = lines[i+2].split(": ")[1]
        precision = lines[i+3].split(": ")[1]
        f1_score = lines[i+4].split(": ")[1]
        csv_data.append(
            f"model,{dim_input},{dim_output},{recall},{accuracy},{precision},{f1_score}")

    with open("./plots/table2.csv", "w") as f:
        f.write("\n".join(csv_data))

    print("Data has been written to ./plots/table2.csv")
