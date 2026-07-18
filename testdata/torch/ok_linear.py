"""Minimal nn.Module with real torch APIs — forward must succeed."""
import torch
import torch.nn as nn


class Model(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc = nn.Linear(3 * 8 * 8, 4)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.fc(x.flatten(1))


# default input for ghar torch is (2,3,8,8)
model = Model()
