"""Real APIs but wrong shapes — forward must fail."""
import torch
import torch.nn as nn


class Model(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc = nn.Linear(16, 4)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x is (2,3,8,8); Linear expects last dim 16 → RuntimeError
        return self.fc(x)


model = Model()
