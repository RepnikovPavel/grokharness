"""Hallucinated API via import alias F — must fail strict-attrs even if unused on hot path."""
import torch
import torch.nn.functional as F


def _dead_code_path(x):
    # AST still sees F.fake_ghar_op — must be static-rejected
    return F.fake_ghar_op(x)


def run_op():
    # clean real path so forward would succeed if attrs were skipped
    return torch.matmul(torch.randn(2, 3), torch.randn(3, 4))
