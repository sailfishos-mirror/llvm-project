#!/usr/bin/env bash
set -euo pipefail

ITERATIONS=2
TOOLS=("mlir-opt" "clang" "opt")
BUILD_DIR=build

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

# Parse time output (e.g. "real 0m1.234s") into seconds.
parse_time() {
    local line="$1"
    local mins secs
    mins=$(echo "$line" | sed -E 's/.*[[:space:]]([0-9]+)m([0-9.]+)s/\1/')
    secs=$(echo "$line" | sed -E 's/.*[[:space:]]([0-9]+)m([0-9.]+)s/\2/')
    echo "$mins * 60 + $secs" | bc
}

# Parse max RSS from /usr/bin/time output and convert to GB.
# macOS `time -l`: bytes;  GNU `time -v`: KB.
parse_maxrss_gb() {
    local output="$1"
    if echo "$output" | grep -q "maximum resident set size"; then
        # macOS: value in bytes
        local bytes
        bytes=$(echo "$output" | grep "maximum resident set size" | awk '{print $1}')
        echo "scale=4; $bytes / 1073741824" | bc
    elif echo "$output" | grep -q "Maximum resident set size"; then
        # GNU time: value in KB
        local kb
        kb=$(echo "$output" | grep "Maximum resident set size" | awk '{print $NF}')
        echo "scale=4; $kb / 1048576" | bc
    else
        echo "0"
    fi
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

    declare -A all_real all_user all_sys all_mem
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
            time_output=$( { /usr/bin/time -l ninja -C "$BUILD_DIR" "$tool" > /dev/null; } 2>&1 )

            real=$(parse_time "$(echo "$time_output" | grep real)")
            user=$(parse_time "$(echo "$time_output" | grep user)")
            sys=$(parse_time "$(echo "$time_output" | grep sys)")
            mem=$(parse_maxrss_gb "$time_output")

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
        read -ra vals <<< "${all_real[$tool]}"
        print_stats "$tool real ($label_suffix)" "s" "${vals[@]}"
        read -ra vals <<< "${all_user[$tool]}"
        print_stats "$tool user ($label_suffix)" "s" "${vals[@]}"
        read -ra vals <<< "${all_sys[$tool]}"
        print_stats "$tool sys ($label_suffix)" "s" "${vals[@]}"
        read -ra vals <<< "${all_mem[$tool]}"
        print_stats "$tool maxrss ($label_suffix)" "GB" "${vals[@]}"
    done
done
done
