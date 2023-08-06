import pickle, torch
from torch.nn.utils import prune
from torchmetrics.classification import BinaryRecall, BinaryAccuracy, BinaryPrecision, BinaryF1Score
import copy
import os, sys
from models import *
x_dir = "data/dx_sum.pkl"
device = 'cpu' #torch.device('cuda' if torch.cuda.is_available() else 'cpu')

from torch.utils.data import Dataset

class DecisionTree(torch.nn.Module):
    def __init__(self):
        super(DecisionTree, self).__init__()
        self.thresholds = torch.Tensor([1,2])
        self.thresholds = self.thresholds / torch.Tensor([16, 8])
    def forward(self, x):
        y1 = torch.all(x >= self.thresholds, dim=1).reshape(-1,1).float()
        y2 = torch.all(x == torch.Tensor([1,1])/torch.Tensor([16, 8]), dim=1).reshape(-1,1).float()
        y = torch.logical_or(y1, y2).float()
        return y
    
class Dataset(Dataset):
    def __init__(self, x, y):
        self.x = x
        self.y = y
    
    def __getitem__(self, index):
        #print("index: ", index)
        return self.x[index], self.y[index]
    
    def __len__(self):
        return len(self.x)

with open(x_dir, 'rb') as f:
    x = pickle.load(f)

metrics_results = []
model_paths = ["data/sandy-sweep-1.pt",
               "data/expert-sweep-4.pt", 
               "data/serene-sweep-2.pt",
               "tree"]

m_recall = BinaryRecall().to(device)
m_acc = BinaryAccuracy().to(device)
m_prec = BinaryPrecision().to(device)
m_f1 = BinaryF1Score().to(device)

m_metrics = [m_recall, m_acc, m_prec, m_f1]

heads = ['VPN', 'PageSize', 'DRAMAccesses', 'PageTableWalks', 'LLPWCHits', 'L1TLBHits', 'L1TLBMisses', 'L2TLBHits', 'L2TLBMisses', 'L1CacheHits', 'L2CacheHits', 'NUCACacheHits', 'L1TLBEvictions', 'L2TLBEvictions', 'Accesses', 'PTWLatency']
feat_heads_1 = ['PageSize', 'DRAMAccesses', 'PageTableWalks', 'LLPWCHits',  'L1TLBMisses', 'L2TLBHits', 'L2TLBMisses',  'L2CacheHits', 'L1TLBEvictions', 'L2TLBEvictions', 'Accesses']#['DRAMAccesses', 'PageTableWalks', 'LLPWCHits','L1TLBMisses','L2TLBMisses', 'L2CacheHits', 'Accesses']    
feat_heads_2 = ['DRAMAccesses', 'PageTableWalks', 'LLPWCHits', 'L2TLBEvictions', 'Accesses']#['DRAMAccesses', 'PageTableWalks', 'LLPWCHits','L1TLBMisses','L2TLBMisses', 'L2CacheHits', 'Accesses']    
feat_heads_3 = ['DRAMAccesses', 'PageTableWalks']#['DRAMAccesses', 'PageTableWalks', 'LLPWCHits','L1TLBMisses','L2TLBMisses', 'L2CacheHits', 'Accesses']    

feat_headss = [feat_heads_1, feat_heads_2, feat_heads_3, feat_heads_3]

for i, model_path in enumerate(model_paths):
    

    with open(x_dir, 'rb') as f:
        x = pickle.load(f)

    x = torch.Tensor(x)
    p = x[:,-1]
    y_labels = (p >torch.quantile(p, 60/100)).float()

    feat_heads = feat_headss[i]
    # saturate values to be at max 7
    for j, feat_head in enumerate(feat_heads):
        feat_col = x[:,heads.index(feat_head)]
        if j == 0:
            x_feat = feat_col.unsqueeze(-1)
        else:
            x_feat = torch.cat((x_feat, feat_col.unsqueeze(-1)), dim=1)
    # x_feat[x_feat>15] = 15 # saturation value so that we keep only 3 bits per feature
    x = x_feat
    # print("x of shape: {}".format(x.shape))
    # print("max of x: {}".format(x.max(0).values))

    norm_x = torch.min(2**torch.ceil(torch.log2(x.max(0)[0])), 64.0*torch.ones_like(x.max(0)[0].float()))

    x_norm = torch.min(x / norm_x, torch.ones_like(x).float())
    y_labels = torch.Tensor(y_labels).unsqueeze(-1)

    x_norm = x_norm#.to(args.device)
    y_labels = y_labels.float()#.to(args.device).float()
    dataset = Dataset(x_norm, y_labels)#.to(args.device)

    train_size = int(0.7 * len(x))
    val_size = int(0.2 * len(x))
    test_size = len(x) - train_size - val_size

    # print("y_lavels shape: ", y_labels.shape)

    # print("make dataloaders")
    datasets = torch.utils.data.random_split(dataset, [train_size, val_size, test_size], generator=torch.Generator().manual_seed(42))

    x_test = datasets[-1].dataset.x
    y_test = datasets[-1].dataset.y
    

    metrics_result = []
    # print("-----------------model_path: ", model_path)
    if model_path == 'tree':
        model = DecisionTree()
        y_pred = model(x_test)
        for m_metric in m_metrics:
            metrics_result.append(m_metric(y_pred, y_test))
            
    else:
        with open(model_path, "rb") as f:
            model = torch.load(f, map_location=device)
                
        model.eval()
        with torch.no_grad():
            y_pred = model(x_test)
        for m_metric in m_metrics:
            metrics_result.append(m_metric(y_pred, y_test))

    metrics_results.append(metrics_result)
        
labels = ['Recall', 'Accuracy', 'Precision', 'F1-Score']

for i in range(len(metrics_results)):
    print("----------model with dim of input: {} and dim of output: {}".format(min(len(feat_headss[i]), 10), 1))
    for label in labels:
        print("{}: {}".format(label, metrics_results[i][labels.index(label)]))
    print("-----------------")

# make a csv file with the results
with open("data/results.csv", "w") as f:
    # make first column "NN1", "NN2", "NN3", "Comparator"
    f.write("NN1,NN2,NN3,Comparator\n")
    for i in range(len(metrics_results[0])):
        # first write what metric it is
        f.write("{},".format(labels[i]))
        # then write the results for each model
        for j in range(len(metrics_results)):
            f.write("{},".format(metrics_results[j][i]))
        f.write("\n")
        