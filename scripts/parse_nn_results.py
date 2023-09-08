import re
import sys

layers = [4, 4, 6, "N/A"]
hidden_layers = [16, 64, 4, "N/A"]
size = [6024, 70152, 776, 24]
with open(sys.argv[1], "r") as f:
    lines = f.readlines()
    csv_data = [
        "Model,FeatureSize,HiddenLayers,Size(B),Recall,Accuracy,Precision,F1-Score"]

    for i in range(0, len(lines), 6):
        model_line = lines[i]
        layers_num = layers[i//6]
        hidden = hidden_layers[i//6]
        modelsize = size[i//6]
        features = re.findall(r"\d+", model_line)[0]
        recall = lines[i+1].split(": ")[1].strip("\n")
        accuracy = lines[i+2].split(": ")[1].strip("\n")
        precision = lines[i+3].split(": ")[1].strip("\n")
        f1_score = lines[i+4].split(": ")[1].strip("\n")

        csv_data.append(
            f"model,{features},{layers_num},{hidden},{modelsize},{recall},{accuracy},{precision},{f1_score}")

    with open("./plots/table2.csv", "w") as f:
        f.write("\n".join(csv_data))

    print("Data has been written to ./plots/table2.csv")
