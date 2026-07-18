"""Hallucinated torch.nn.functional leaf — must fail strict-attrs (no parent fallback)."""
import torch


def run_op():
    # real parent module, fake leaf — previously accepted via parts[:3] fallback
    x = torch.randn(2, 4)
    return torch.nn.functional.super_magic_activation(x)
