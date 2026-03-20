#!/usr/bin/env zsh
set -euo pipefail

ITERATIONS=2
TOOLS=("mlir-opt" "clang" "opt")
BUILD_DIR=build
TIMEFMT='%*U %*S %*E %M'

print_stats() {
    local label="$1"
    local unit="$2"
    shift 2
    local vals=("$@")

    echo ""
    echo "========== $label =========="
    printf "Values: "
    printf "%s " "${vals[@]}"
    echo ""

    printf "%s\n" "${vals[@]}" | awk -v unit="$unit" '
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
        printf "Mean:   %.2f%s\n", mean, unit
        printf "Median: %.2f%s\n", median, unit
        printf "Var:    %.4f\n", var
        printf "StdDev: %.2f%s\n", sqrt(var), unit
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

    typeset -A all_real all_user all_sys all_mem
    for tool in "${TOOLS[@]}"; do
        all_real[$tool]=""
        all_user[$tool]=""
        all_sys[$tool]=""
        all_mem[$tool]=""
    done

    for i in $(seq 1 $ITERATIONS); do
        echo "========== [$label_suffix] Iteration $i / $ITERATIONS =========="

        cmake -S llvm -B "$BUILD_DIR" \
            -G Ninja \
            -DCMAKE_BUILD_TYPE="$build_type" \
            -DLLVM_ENABLE_PROJECTS="mlir;clang" \
            $unity_flag > /dev/null

        for tool in "${TOOLS[@]}"; do
            # TIMEFMT='%*U %*S %*E %M' outputs: user_s sys_s real_s maxrss_kb
            time_output=$( { time ninja -C "$BUILD_DIR" "$tool" > /dev/null } 2>&1 )

            user=$(echo "$time_output" | awk '{print $1}')
            sys=$(echo "$time_output" | awk '{print $2}')
            real=$(echo "$time_output" | awk '{print $3}')
            mem=$(echo "$time_output" | awk '{printf "%.4f", $4 / 1048576}')

            all_real[$tool]+="$real "
            all_user[$tool]+="$user "
            all_sys[$tool]+="$sys "
            all_mem[$tool]+="$mem "
            echo ">>> $tool  real=${real}s  user=${user}s  sys=${sys}s  maxrss=${mem}GB"

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
        read -rA vals <<< "${all_real[$tool]}"
        print_stats "$tool real ($label_suffix)" "s" "${vals[@]}"
        read -rA vals <<< "${all_user[$tool]}"
        print_stats "$tool user ($label_suffix)" "s" "${vals[@]}"
        read -rA vals <<< "${all_sys[$tool]}"
        print_stats "$tool sys ($label_suffix)" "s" "${vals[@]}"
        read -rA vals <<< "${all_mem[$tool]}"
        print_stats "$tool maxrss ($label_suffix)" "GB" "${vals[@]}"
    done
done
done
