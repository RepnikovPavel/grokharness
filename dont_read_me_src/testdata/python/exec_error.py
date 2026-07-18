"""Syntax ok, imports ok, runtime must fail under ghar python --exec."""
import sys

# Module-level so --exec (not __main__) still raises
raise RuntimeError("intentional_exec_error")
