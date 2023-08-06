import torch
import torch.nn as nn

class MLP(nn.Module):
    def __init__(self, in_feats, hid_feats, out_feats, n_layers):
        super(MLP, self).__init__()
        self.in_feats = in_feats
        self.hid_feats = hid_feats
        self.out_feats = out_feats
        self.n_layers = n_layers
        self.fc = nn.Sequential()

        if self.n_layers == 1:
            self.fc.append(nn.Linear(self.in_feats, self.out_feats))
            self.fc.append(nn.Sigmoid())
        else:
            for i in range(self.n_layers):
                if i == 0:
                    self.fc.append(nn.Linear(self.in_feats, self.hid_feats))
                    self.fc.append(nn.ReLU())
                elif i == self.n_layers - 1:
                    self.fc.append(nn.Linear(self.hid_feats, self.out_feats))
                    self.fc.append(nn.Sigmoid())
                else:
                    self.fc.append(nn.Linear(self.hid_feats, self.hid_feats))
                    self.fc.append(nn.ReLU())        
        
    def forward(self, x):
        x = self.fc(x)
        return x