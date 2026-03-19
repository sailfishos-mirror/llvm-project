#!/usr/bin/env bash
set -euo pipefail

ITERATIONS=10
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

for build_type in Release Debug; do
for unity_mode in off on; do
    if [ "$unity_mode" = "off" ]; then
        unity_flag=""
        label_suffix="$build_type, no unity"
    else
        unity_flag="-DCMAKE_UNITY_BUILD=ON"
        label_suffix="$build_type, with unity"
    fi

    declare -A all_times
    for tool in "${TOOLS[@]}"; do
        all_times[$tool]=""
    done

    for i in $(seq 1 $ITERATIONS); do
        echo "========== [$label_suffix] Iteration $i / $ITERATIONS =========="

        cmake -S llvm -B "$BUILD_DIR" \
            -G Ninja \
            -DCMAKE_BUILD_TYPE="$build_type" \
            -DLLVM_ENABLE_PROJECTS="mlir;clang" \
            $unity_flag > /dev/null

        for tool in "${TOOLS[@]}"; do
            start=$(date +%s.%N)
            ninja -C "$BUILD_DIR" "$tool" > /dev/null
            end=$(date +%s.%N)

            elapsed=$(echo "$end - $start" | bc)
            all_times[$tool]+="$elapsed "
            echo ">>> $tool took ${elapsed}s"

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
        read -ra times <<< "${all_times[$tool]}"
        print_stats "$tool ($label_suffix)" "${times[@]}"
    done
done
done
