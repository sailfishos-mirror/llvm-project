#!/usr/bin/env python3
"""
Performance regression testing for AMDGPU static simulator warm cycle counts.

Usage:
    ./perf_test.py                    # Run tests against baseline
    ./perf_test.py --update           # Update baseline with current results
    ./perf_test.py --update-on-pass   # Update baseline only if all tests pass
    ./perf_test.py --verbose          # Show detailed output
    ./perf_test.py --migration-target # Use note.txt warm cycles as baseline
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple, Dict


@dataclass
class TestResult:
    name: str
    warm_cycles: Optional[int]
    baseline_cycles: Optional[int]
    passed: bool
    error: Optional[str] = None

    @property
    def delta(self) -> Optional[int]:
        if self.warm_cycles is not None and self.baseline_cycles is not None:
            return self.warm_cycles - self.baseline_cycles
        return None

    @property
    def delta_percent(self) -> Optional[float]:
        if self.delta is not None and self.baseline_cycles and self.baseline_cycles > 0:
            return (self.delta / self.baseline_cycles) * 100
        return None


SCRIPT_DIR = Path(__file__).parent.resolve()
BASELINE_FILE = SCRIPT_DIR / "baseline.json"

# Pattern to match innermost loop warm cycles
# Format: ;=== Block (loop): Cold=XXXXcyc Warm=XXXXcyc Trip=XX Scaled=XXXXcyc [header] ===
WARM_PATTERN = re.compile(r"Block \(loop\):.*Warm=(\d+)cyc")

# Pattern to extract warm cycles from note.txt migration target
# Format: "XX% xdl util, YYY warm cycles"
NOTE_WARM_PATTERN = re.compile(r"(\d+)\s*warm cycles")


def get_llc_path() -> Path:
    """Find llc binary, preferring build directory relative to repo."""
    # Try relative path from repo root
    repo_root = SCRIPT_DIR.parents[4]  # llvm/lib/Target/AMDGPU/PerfCorpus -> repo root
    build_llc = repo_root / "build" / "bin" / "llc"
    if build_llc.exists():
        return build_llc

    # Try environment variable
    if "LLC_PATH" in os.environ:
        return Path(os.environ["LLC_PATH"])

    # Try PATH
    try:
        result = subprocess.run(["which", "llc"], capture_output=True, text=True)
        if result.returncode == 0:
            return Path(result.stdout.strip())
    except Exception:
        pass

    raise RuntimeError(
        "Cannot find llc. Set LLC_PATH environment variable or ensure build/bin/llc exists."
    )


def find_test_files() -> List[Path]:
    """Find all .ll files in the MI450 corpus."""
    mi450_dir = SCRIPT_DIR / "MI450"
    if not mi450_dir.exists():
        raise RuntimeError(f"MI450 directory not found: {mi450_dir}")
    return sorted(mi450_dir.rglob("*.ll"))


def get_test_name(ll_file: Path) -> str:
    """Generate a unique test name from file path."""
    rel_path = ll_file.relative_to(SCRIPT_DIR / "MI450")
    return str(rel_path.with_suffix(""))


def run_llc(llc_path: Path, ll_file: Path) -> Tuple[Optional[int], Optional[str]]:
    """Run llc and extract the warm cycle count for the innermost loop."""
    cmd = [
        str(llc_path),
        "-mtriple=amdgcn-amd-amdhsa",
        "-mcpu=gfx1250",
        str(ll_file),
        "--amdgpu-sched-strategy=coexec",
        "--amdgpu-enable-static-simulator=1",
        "--fp-contract=fast",
        "--enable-post-misched=0",
        "-o", "-",
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if result.returncode != 0:
            return None, f"llc failed: {result.stderr[:200]}"

        # Find all warm cycle counts and return the one for the innermost loop
        # The innermost loop is typically the last one encountered before "Inner Loop Header"
        output = result.stdout
        warm_cycles = None

        # Find all matches
        matches = list(WARM_PATTERN.finditer(output))
        if not matches:
            return None, "No warm cycle annotation found in output"

        # Use the first loop block (typically the main hot loop)
        warm_cycles = int(matches[0].group(1))
        return warm_cycles, None

    except subprocess.TimeoutExpired:
        return None, "llc timed out after 120 seconds"
    except Exception as e:
        return None, str(e)


def load_baseline() -> Dict[str, int]:
    """Load baseline warm cycle counts."""
    if not BASELINE_FILE.exists():
        return {}
    with open(BASELINE_FILE) as f:
        return json.load(f)


def load_migration_targets() -> Dict[str, int]:
    """Load migration targets from note.txt files in test directories."""
    targets = {}
    mi450_dir = SCRIPT_DIR / "MI450"
    if not mi450_dir.exists():
        return targets

    for ll_file in mi450_dir.rglob("*.ll"):
        test_name = get_test_name(ll_file)
        note_file = ll_file.parent / "note.txt"
        if note_file.exists():
            try:
                content = note_file.read_text()
                match = NOTE_WARM_PATTERN.search(content)
                if match:
                    targets[test_name] = int(match.group(1))
            except Exception:
                pass
    return targets


def save_baseline(baseline: Dict[str, int]) -> None:
    """Save baseline warm cycle counts."""
    with open(BASELINE_FILE, "w") as f:
        json.dump(baseline, f, indent=2, sort_keys=True)
        f.write("\n")


def run_tests(verbose: bool = False, migration_target: bool = False) -> List[TestResult]:
    """Run all tests and return results."""
    llc_path = get_llc_path()
    if migration_target:
        baseline = load_migration_targets()
    else:
        baseline = load_baseline()
    ll_files = find_test_files()

    if not ll_files:
        print("No test files found!")
        return []

    results = []

    for ll_file in ll_files:
        test_name = get_test_name(ll_file)
        if verbose:
            print(f"Running: {test_name}...", end=" ", flush=True)

        warm_cycles, error = run_llc(llc_path, ll_file)
        baseline_cycles = baseline.get(test_name)

        if error:
            passed = False
        elif baseline_cycles is None:
            # No baseline/target - pass (target of 0 or more means any result is acceptable)
            passed = True
        else:
            # Pass if cycles improved or stayed the same
            passed = warm_cycles <= baseline_cycles

        result = TestResult(
            name=test_name,
            warm_cycles=warm_cycles,
            baseline_cycles=baseline_cycles,
            passed=passed,
            error=error,
        )
        results.append(result)

        if verbose:
            if error:
                print(f"ERROR: {error}")
            elif baseline_cycles is None:
                if migration_target:
                    print(f"NO TARGET ({warm_cycles} cycles)")
                else:
                    print(f"NEW ({warm_cycles} cycles)")
            elif passed:
                delta = result.delta
                if delta == 0:
                    print(f"PASS ({warm_cycles} cycles, unchanged)")
                else:
                    print(f"PASS ({warm_cycles} cycles, {delta:+d} = {result.delta_percent:+.2f}%)")
            else:
                print(f"FAIL ({warm_cycles} cycles, baseline {baseline_cycles}, +{result.delta} = +{result.delta_percent:.2f}%)")

    return results


def print_summary(results: List[TestResult], migration_target: bool = False) -> None:
    """Print test summary."""
    passed = sum(1 for r in results if r.passed)
    failed = sum(1 for r in results if not r.passed)
    no_baseline = sum(1 for r in results if r.baseline_cycles is None and r.error is None)
    errors = sum(1 for r in results if r.error)

    print("\n" + "=" * 60)
    print(f"Results: {passed} passed, {failed} failed", end="")
    if no_baseline:
        label = "no target" if migration_target else "new"
        print(f", {no_baseline} {label}", end="")
    if errors:
        print(f", {errors} errors", end="")
    print()

    if failed > 0:
        label = "target" if migration_target else "baseline"
        print(f"\nFailed tests (exceeded {label}):")
        for r in results:
            if not r.passed and not r.error:
                print(f"  {r.name}: {r.warm_cycles} > {r.baseline_cycles} (+{r.delta} cycles, +{r.delta_percent:.2f}%)")

    if errors > 0:
        print("\nErrors:")
        for r in results:
            if r.error:
                print(f"  {r.name}: {r.error}")


def update_baseline(results: List[TestResult]) -> None:
    """Update baseline with current results."""
    baseline = {}
    for r in results:
        if r.warm_cycles is not None:
            baseline[r.name] = r.warm_cycles
    save_baseline(baseline)
    print(f"Updated baseline with {len(baseline)} entries.")


def main():
    parser = argparse.ArgumentParser(description="Run performance regression tests")
    parser.add_argument("--update", action="store_true",
                        help="Update baseline with current results")
    parser.add_argument("--update-on-pass", action="store_true",
                        help="Update baseline only if all tests pass")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show detailed output for each test")
    parser.add_argument("--list", action="store_true",
                        help="List all test files without running")
    parser.add_argument("--migration-target", action="store_true",
                        help="Use warm cycles from note.txt files as baseline instead of baseline.json")
    args = parser.parse_args()

    if args.list:
        for f in find_test_files():
            print(get_test_name(f))
        return 0

    print(f"Running performance tests...")
    print(f"LLC: {get_llc_path()}")
    if args.migration_target:
        print(f"Baseline: migration targets from note.txt files")
    else:
        print(f"Baseline: {BASELINE_FILE}")
    print()

    results = run_tests(verbose=args.verbose, migration_target=args.migration_target)
    print_summary(results, migration_target=args.migration_target)

    all_passed = all(r.passed for r in results)

    if args.update:
        update_baseline(results)
    elif args.update_on_pass and all_passed:
        update_baseline(results)

    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
