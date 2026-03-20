#!/usr/bin/env bash
set -euo pipefail

ITERATIONS=2
TOOLS=("mlir-opt" "clang" "opt")
BUILD_DIR=build

print_stats() {
    local label="$1"
    shift
    local times=("$@")

    echo ""
    echo "========== $label =========="
    printf "Times: "
    printf "%s " "${times[@]}"
    echo ""

    printf "%s\n" "${times[@]}" | awk '
    {
        vals[NR] = $1
        sum += $1
        sumsq += $1 * $1
        n = NR
    }
    END {
        mean = sum / n
        var = sumsq / n - mean * mean

        for (i = 1; i <= n; i++)
            for (j = i + 1; j <= n; j++)
                if (vals[i] > vals[j]) {
                    t = vals[i]; vals[i] = vals[j]; vals[j] = t
                }

        if (n % 2 == 1)
            median = vals[int(n/2) + 1]
        else
            median = (vals[n/2] + vals[n/2 + 1]) / 2

        printf "N:      %d\n", n
        printf "Mean:   %.2fs\n", mean
        printf "Median: %.2fs\n", median
        printf "Var:    %.2f\n", var
        printf "StdDev: %.2fs\n", sqrt(var)
    }
    '
}

# Parse the output of `time` (real/user/sys lines) into seconds.
# Handles both "0m1.234s" and "1m2.345s" formats.
parse_time() {
    local line="$1"
    local mins secs
    mins=$(echo "$line" | sed -E 's/.*[[:space:]]([0-9]+)m([0-9.]+)s/\1/')
    secs=$(echo "$line" | sed -E 's/.*[[:space:]]([0-9]+)m([0-9.]+)s/\2/')
    echo "$mins * 60 + $secs" | bc
}

for build_type in Release Debug; do
for unity_mode in off on; do
    if [ "$unity_mode" = "off" ]; then
        unity_flag=""
        label_suffix="$build_type, no unity"
    else
        unity_flag="-DCMAKE_UNITY_BUILD=ON"
        label_suffix="$build_type, with unity"
    fi

    declare -A all_real all_user all_sys
    for tool in "${TOOLS[@]}"; do
        all_real[$tool]=""
        all_user[$tool]=""
        all_sys[$tool]=""
    done

    for i in $(seq 1 $ITERATIONS); do
        echo "========== [$label_suffix] Iteration $i / $ITERATIONS =========="

        cmake -S llvm -B "$BUILD_DIR" \
            -G Ninja \
            -DCMAKE_BUILD_TYPE="$build_type" \
            -DLLVM_ENABLE_PROJECTS="mlir;clang" \
            $unity_flag > /dev/null

        for tool in "${TOOLS[@]}"; do
            time_output=$( { time ninja -C "$BUILD_DIR" "$tool" > /dev/null; } 2>&1 )

            real=$(parse_time "$(echo "$time_output" | grep real)")
            user=$(parse_time "$(echo "$time_output" | grep user)")
            sys=$(parse_time "$(echo "$time_output" | grep sys)")

            all_real[$tool]+="$real "
            all_user[$tool]+="$user "
            all_sys[$tool]+="$sys "
            echo ">>> $tool  real=${real}s  user=${user}s  sys=${sys}s"

            # Clean build artifacts but keep the configured build dir
            ninja -C "$BUILD_DIR" clean > /dev/null
        done

        rm -rf "$BUILD_DIR"
    done

    echo ""
    echo "############################################"
    echo "# Results: $label_suffix"
    echo "############################################"
    for tool in "${TOOLS[@]}"; do
        read -ra times <<< "${all_real[$tool]}"
        print_stats "$tool real ($label_suffix)" "${times[@]}"
        read -ra times <<< "${all_user[$tool]}"
        print_stats "$tool user ($label_suffix)" "${times[@]}"
        read -ra times <<< "${all_sys[$tool]}"
        print_stats "$tool sys ($label_suffix)" "${times[@]}"
    done
done
done
