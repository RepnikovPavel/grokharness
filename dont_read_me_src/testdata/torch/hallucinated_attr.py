"""Agent invents a torch API that does not exist — strict-attrs must catch."""
import torch


def run_op():
    # torch.nn.SuperDuperLayer does not exist
    return torch.nn.SuperDuperLayer(16, 32)(torch.randn(2, 16))
