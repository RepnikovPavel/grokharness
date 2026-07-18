"""Valid Python — syntax + stdlib imports + exec."""
import math
import sys

# module-level side effects so ghar python --exec exercises real code
_assert_sqrt = math.sqrt(9)
assert _assert_sqrt == 3.0
print("hello_ghar", file=sys.stdout)


def main() -> int:
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
