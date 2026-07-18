"""Real F.relu via alias — strict-attrs must pass."""
import torch
import torch.nn.functional as F


def run_op():
    x = torch.randn(2, 4)
    return F.relu(x)
