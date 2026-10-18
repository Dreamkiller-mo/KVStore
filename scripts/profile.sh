#!/usr/bin/env bash
#
# Capture a Google Benchmark run with perf and generate a CPU flame graph.
#
# Usage:
#   ./scripts/profile.sh [benchmark-filter] [frequency-hz] [duration-seconds]
#   BENCH=./path/to/kvstore_bench ./scripts/profile.sh 'BM_Put' 99 10
#
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: profile.sh [benchmark-filter] [frequency-hz] [duration-seconds]

Profile the kvstore_bench target and write perf data, a text report, and a
flame graph to ./perf-out (relative to the repository root).

Arguments:
  benchmark-filter  Google Benchmark filter (default: BM_Get/65536$)
  frequency-hz      perf sampling frequency, a positive integer (default: 99)
  duration-seconds  Maximum recording time; 0 records until the benchmark
                    exits (default: 10)

Environment:
  BENCH             Benchmark executable path
  OUT_DIR           Output directory
  FLAMEGRAPH_DIR    FlameGraph scripts directory (default: thirdparty/FlameGraph)
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if (( $# > 3 )); then
    usage >&2
    exit 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
DEFAULT_FILTER='BM_Get/65536$'

BENCH="${BENCH:-$PROJECT_ROOT/build/benchmark/benchmark/kvstore_bench}"
FILTER="${1:-$DEFAULT_FILTER}"
FREQ="${2:-99}"
DURATION="${3:-10}"
OUT_DIR="${OUT_DIR:-$PROJECT_ROOT/perf-out}"
FLAMEGRAPH_DIR="${FLAMEGRAPH_DIR:-$PROJECT_ROOT/thirdparty/FlameGraph}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'
info() { printf "${GREEN}[INFO]${NC} %s\n" "$*"; }
warn() { printf "${YELLOW}[WARN]${NC} %s\n" "$*" >&2; }
die() { printf "${RED}[ERROR]${NC} %s\n" "$*" >&2; exit 1; }

[[ -x "$BENCH" ]] || die "Benchmark executable not found or not executable: $BENCH (build it with: cmake --build --preset benchmark)"
command -v perf >/dev/null 2>&1 || die "perf is not installed (on Ubuntu, install linux-tools for your kernel)"
command -v timeout >/dev/null 2>&1 || die "timeout is not installed"
command -v perl >/dev/null 2>&1 || die "Perl is required by the FlameGraph scripts"

[[ "$FREQ" =~ ^[1-9][0-9]*$ ]] || die "Sampling frequency must be a positive integer: $FREQ"
[[ "$DURATION" =~ ^(0|[1-9][0-9]*)$ ]] || die "Duration must be a non-negative integer: $DURATION"

if [[ ! -d "$FLAMEGRAPH_DIR" ]]; then
    command -v git >/dev/null 2>&1 || die "FlameGraph directory not found and git is not installed: $FLAMEGRAPH_DIR"
    mkdir -p "$(dirname -- "$FLAMEGRAPH_DIR")"
    info "FlameGraph scripts not found; cloning them into $FLAMEGRAPH_DIR"
    git clone --depth=1 https://github.com/brendangregg/FlameGraph.git "$FLAMEGRAPH_DIR" ||
        die "Could not clone FlameGraph into: $FLAMEGRAPH_DIR"
fi

STACK_COLLAPSE="$FLAMEGRAPH_DIR/stackcollapse-perf.pl"
FLAMEGRAPH="$FLAMEGRAPH_DIR/flamegraph.pl"
[[ -x "$STACK_COLLAPSE" ]] || die "FlameGraph script is missing or not executable: $STACK_COLLAPSE"
[[ -x "$FLAMEGRAPH" ]] || die "FlameGraph script is missing or not executable: $FLAMEGRAPH"

mkdir -p "$OUT_DIR"
PERF_DATA="$OUT_DIR/perf.data"
REPORT_TXT="$OUT_DIR/report.txt"
FLAME_SVG="$OUT_DIR/flamegraph.svg"
REPORT_TMP="$REPORT_TXT.tmp.$$"
FLAME_TMP="$FLAME_SVG.tmp.$$"
trap 'rm -f "$REPORT_TMP" "$FLAME_TMP"' EXIT

info "Benchmark : $BENCH"
info "Filter    : $FILTER"
info "Frequency : ${FREQ} Hz"
info "Duration  : ${DURATION}s (0 means no time limit)"
info "Output    : $OUT_DIR"

if [[ -r /proc/sys/kernel/perf_event_paranoid ]]; then
    PARANOID="$(</proc/sys/kernel/perf_event_paranoid)"
    if [[ "$PARANOID" =~ ^-?[0-9]+$ ]] && (( PARANOID > 1 )); then
        warn "perf_event_paranoid=$PARANOID may block perf sampling. If recording is denied, ask an administrator to adjust perf permissions."
    fi
fi

info "Starting perf recording..."
rm -f "$PERF_DATA"
if [[ "$DURATION" == "0" ]]; then
    perf record -F "$FREQ" -o "$PERF_DATA" --call-graph dwarf \
        -- "$BENCH" "--benchmark_filter=$FILTER" ||
        die "perf recording failed"
else
    if timeout "${DURATION}s" perf record -F "$FREQ" -o "$PERF_DATA" --call-graph dwarf \
        -- "$BENCH" "--benchmark_filter=$FILTER"; then
        :
    else
        STATUS=$?
        if (( STATUS == 124 )); then
            warn "Recording reached the ${DURATION}s time limit."
        else
            die "perf recording failed with exit code $STATUS"
        fi
    fi
fi
[[ -s "$PERF_DATA" ]] || die "perf.data is empty; no samples were recorded"

info "Generating text report..."
perf report -i "$PERF_DATA" --stdio --no-children --sort=dso,symbol > "$REPORT_TMP"
mv -f "$REPORT_TMP" "$REPORT_TXT"

info "Generating flame graph..."
perf script -i "$PERF_DATA" |
    "$STACK_COLLAPSE" |
    "$FLAMEGRAPH" --title="kvstore_bench $FILTER" > "$FLAME_TMP"
[[ -s "$FLAME_TMP" ]] || die "Flame graph output is empty"
mv -f "$FLAME_TMP" "$FLAME_SVG"

printf '\n'
info "Done. Generated files:"
printf '  Raw samples : %s\n' "$PERF_DATA"
printf '  Text report : %s\n' "$REPORT_TXT"
printf '  Flame graph : %s\n' "$FLAME_SVG"
printf '\nOpen the SVG in a browser, or inspect the report with:\n'
printf '  perf report -i "%s"\n' "$PERF_DATA"
