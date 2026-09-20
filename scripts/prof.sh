#!/usr/bin/env bash
# =============================================================================
#  AgentKV 性能采集脚本 —— callgrind 采集 + KCachegrind 查看
#
#  一句话:  bash scripts/prof.sh put
#
#  完整帮助: bash scripts/prof.sh -h
#
#  设计要点(踩过的坑都写在这里了):
#   1. 默认用 --toggle-collect 把统计区间卡在内存引擎的 Put/Get 调用上.
#      原因: BM_Put/BM_Get 一进来就会做 Data() 的静态初始化(生成 100 万对
#      key/value), BM_Get 还会先做 100 万次 Put 预热. 这些都属于 setup,
#      不卡区间的话 KCachegrind 里看到的基本全是 setup, 不是你想测的循环.
#      (Valgrind 手册: 指定 --toggle-collect 会隐式把采集状态设为 off,
#       所以不需要再配 --instr-atstart=no.)
#   2. 采集完会校验结果非空; 万一通配符没命中, 自动退回不限区间的模式重跑.
#   3. Google Benchmark 的 benchmark::Initialize()/BENCHMARK_MAIN() 会调用
#      MaybeReenterWithoutASLR(), 它用 execv 重启自己来关掉 ASLR. 而 valgrind
#      默认不跟踪 client 的 exec(即 --trace-children=no), 这一下 exec 会把
#      valgrind 的监管进程整个替换掉: 程序以原生速度跑完, 不写任何 profile
#      (只留下一个 0 字节的空文件), 也没有退出摘要. 所以脚本默认用
#      `setarch -R` 提前把 ASLR 关掉, 让那个函数一进来就 return;
#      万一没有 setarch, 就退化成给 valgrind 加 --trace-children=yes 跟进 exec.
#   4. Google Benchmark 的 --benchmark_min_time 是"墙钟时间", valgrind 下
#      迭代次数会少几十倍. 想测满 100 万条时的行为, 用 `get`(setup 已把表
#      填满), 或者把 MINTIME 调大.
#
#  输出:  build/prof/cg_<目标>.out   <- 拖进 KCachegrind
#         build/prof/cg_<目标>.txt   <- callgrind_annotate 文本摘要
# =============================================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PRESET="${PRESET:-relwithdebinfo}"
MINTIME="${MINTIME:-2s}"
JOBS="${JOBS:-}"
if [ -z "${JOBS}" ]; then
    JOBS="$(nproc 2>/dev/null || echo 4)"
fi

BUILD_DIR="${ROOT}/build/${PRESET}"
OUT_DIR="${ROOT}/build/prof"

PLAIN=0
CACHE=0
ASM=0
GUI=1
REBUILD=0
NOBUILD=0
TARGETS=()

usage() {
    cat <<'EOF'
AgentKV profiling helper —— callgrind 采集 + KCachegrind 查看

用法:
  bash scripts/prof.sh [put|get|mixed|all] [开关]

采集目标(默认 put, 可多个):
  put      BM_Put              只测写
  get      BM_Get              只测读(表已被 setup 填满 100 万条, 最适合看缓存)
  mixed    BM_MixedReadWrite   8:2 混合读写
  all      三个依次采集

开关:
  --rebuild   采集前强制重新配置并构建
  --no-build  完全不构建(源码比二进制新的话会警告, 但仍然照跑)
  --cache     打开 cache 模拟, KCachegrind 里会多出 Dr/D1mr/DLmr/Dw/D1mw/DLmw 列
  --asm       指令级采集, KCachegrind 能逐条汇编看(更慢, 输出更大)
  --plain     不限区间, 统计整个进程(默认只统计内存引擎内的调用)
  --no-gui    只生成文件并打印文本摘要, 不启动 KCachegrind
  -h, --help  显示这份帮助

默认行为:
  脚本会先比较 src/ include/ tests/ 里最新源码和二进制的时间戳, 发现源码更新
  就自动重新构建 —— 否则你 profile 的是旧代码, 结论全是错的(踩过一次).
  想跳过这个检查就用 --no-build.

环境变量:
  PRESET=relwithdebinfo   使用的 CMake preset
  MINTIME=2s              传给 Google Benchmark 的 --benchmark_min_time(墙钟时间)
  JOBS=8                  并行编译任务数

例子:
  bash scripts/prof.sh                       # 采 BM_Put 并用 KCachegrind 打开
  bash scripts/prof.sh get --cache           # 采 BM_Get 并带缓存模拟(看缓存命中)
  MINTIME=20s bash scripts/prof.sh put       # 采久一点, 让哈希表涨到接近满载
  bash scripts/prof.sh all --no-gui          # 三个都采, 只看文本摘要

提示:
  * KCachegrind 需要图形环境, WSLg 自带; 窗口没弹出来就试
        QT_QPA_PLATFORM=xcb kcachegrind build/prof/cg_put.out
  * 每轮开头要等一会儿才出结果, 这是正常的:
      - 生成测试数据(Data() 静态初始化, 100 万对 key/value): 十几秒
      - 采 get 时还要先做 100 万次 Put 预热: 再十几到几十秒
    这两段都被 toggle 排除在统计之外, 但代码还是要真跑一遍, 所以要等.
EOF
}

# ------------------------------------------------------------------ 参数解析
while [ "$#" -gt 0 ]; do
    case "$1" in
        put|get|mixed|all) TARGETS+=("$1") ;;
        --plain)   PLAIN=1 ;;
        --cache)   CACHE=1 ;;
        --asm)     ASM=1 ;;
        --no-gui)  GUI=0 ;;
        --rebuild) REBUILD=1 ;;
        --no-build) NOBUILD=1 ;;
        -h|--help) usage; exit 0 ;;
        *)
            echo "未知参数: $1" >&2
            echo >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

if [ "${#TARGETS[@]}" -eq 0 ]; then
    TARGETS=(put)
fi

# ------------------------------------------------------------------ 依赖检查
if ! command -v valgrind >/dev/null 2>&1; then
    echo "错误: 找不到 valgrind.  安装: sudo apt install valgrind" >&2
    exit 1
fi
if ! command -v callgrind_annotate >/dev/null 2>&1; then
    echo "错误: 找不到 callgrind_annotate (通常随 valgrind 一起装)." >&2
    exit 1
fi

# 用来绕开 Google Benchmark 的 ASLR 重入(见文件头说明 3)
USE_SETARCH=0
if command -v setarch >/dev/null 2>&1; then
    USE_SETARCH=1
fi

# ------------------------------------------------------------------ 构建
build_project() {
    (
        cd "${ROOT}" || exit 1
        echo "==> 配置 (preset: ${PRESET})"
        cmake --preset="${PRESET}" || exit 1
        echo "==> 构建 (jobs: ${JOBS})"
        cmake --build --preset="${PRESET}" --parallel "${JOBS}" || exit 1
    )
}

find_binary() {
    local c
    for c in "${BUILD_DIR}/tests/agent_kv_test" "${BUILD_DIR}/agent_kv_test"; do
        if [ -x "${c}" ]; then
            printf '%s\n' "${c}"
            return 0
        fi
    done
    return 1
}

is_stale() {
    # 有比二进制更新的源码 -> 说明该重建了
    local bin="$1" newest src_ts bin_ts
    newest="$(find "${ROOT}/src" "${ROOT}/include" "${ROOT}/tests" -type f \
                \( -name '*.cc' -o -name '*.cpp' -o -name '*.c' \
                   -o -name '*.h' -o -name '*.hpp' -o -name 'CMakeLists.txt' \) \
                -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -n 1)"
    [ -n "${newest}" ] || return 1
    src_ts="${newest%% *}"
    src_ts="${src_ts%.*}"
    bin_ts="$(stat -c '%Y' "${bin}" 2>/dev/null)" || return 1
    [ -n "${bin_ts}" ] || return 1
    [ "${src_ts}" -gt "${bin_ts}" ]
}

BIN="$(find_binary)" || true

NEED_BUILD=0
STALE=0
if [ -n "${BIN}" ] && is_stale "${BIN}"; then
    STALE=1
fi

if [ "${REBUILD}" -eq 1 ]; then
    NEED_BUILD=1
elif [ "${STALE}" -eq 1 ]; then
    if [ "${NOBUILD}" -eq 0 ]; then
        echo "!! 源码比二进制新, 先自动重新构建 (想跳过用 --no-build)"
        NEED_BUILD=1
    else
        echo "!! 警告: 源码比二进制新, 但指定了 --no-build —— 接下来 profile 的是旧代码!" >&2
    fi
fi

if [ "${NEED_BUILD}" -eq 1 ]; then
    build_project || { echo "构建失败" >&2; exit 1; }
    BIN="$(find_binary)" || true
fi

if [ -z "${BIN}" ]; then
    echo "错误: 没找到 agent_kv_test (期望在 ${BUILD_DIR}/tests/ 下)" >&2
    echo "      先构建:  cmake --preset=${PRESET} && cmake --build --preset=${PRESET}" >&2
    echo "      或者加 --rebuild:  bash scripts/prof.sh ${TARGETS[0]} --rebuild" >&2
    exit 1
fi

mkdir -p "${OUT_DIR}" || exit 1

# ------------------------------------------------------------------ 采集
run_callgrind() {
    # $1 = 输出文件; 其余参数 = toggle 匹配串(可以没有)
    local out="$1"
    local p
    local vargs
    shift
    vargs=(--tool=callgrind "--callgrind-out-file=${out}")

    if [ "${USE_SETARCH}" -eq 0 ]; then
        # 没有 setarch 时, 让 valgrind 跟进 benchmark 那次 execv 重入
        vargs+=(--trace-children=yes)
    fi
    if [ "${PLAIN}" -eq 0 ]; then
        for p in "$@"; do
            vargs+=("--toggle-collect=${p}")
        done
    fi
    if [ "${CACHE}" -eq 1 ]; then
        vargs+=(--cache-sim=yes --branch-sim=yes)
    fi
    if [ "${ASM}" -eq 1 ]; then
        vargs+=(--dump-instr=yes --collect-jumps=yes)
    fi

    echo "==> valgrind ${vargs[*]}"
    echo "    ${BIN} --benchmark_filter=${FILTER} --benchmark_min_time=${MINTIME}"
    if [ "${USE_SETARCH}" -eq 1 ]; then
        # 提前关掉 ASLR -> MaybeReenterWithoutASLR 直接返回, 不会 execv
        setarch -R valgrind "${vargs[@]}" "${BIN}" \
            --benchmark_filter="${FILTER}" \
            --benchmark_min_time="${MINTIME}"
    else
        valgrind "${vargs[@]}" "${BIN}" \
            --benchmark_filter="${FILTER}" \
            --benchmark_min_time="${MINTIME}"
    fi
}

has_data() {
    local out="$1"
    [ -s "${out}" ] || return 1
    callgrind_annotate --threshold=99 "${out}" 2>/dev/null \
        | grep -qE '^[[:space:]]*[0-9][0-9,]*[[:space:]]'
}

launch_gui() {
    local out="$1"
    if ! command -v kcachegrind >/dev/null 2>&1; then
        echo "提示: 没装 kcachegrind.  安装: sudo apt install kcachegrind"
        echo "      手动打开: kcachegrind ${out}"
        return 0
    fi
    if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
        echo "提示: 当前没有图形环境(DISPLAY / WAYLAND_DISPLAY 都没设置)."
        echo "      手动打开: kcachegrind ${out}"
        return 0
    fi
    echo "==> 打开 KCachegrind: ${out}"
    nohup kcachegrind "${out}" >/dev/null 2>&1 &
    echo "    (窗口没弹出来的话试: QT_QPA_PLATFORM=xcb kcachegrind ${out})"
}

# ------------------------------------------------------------------ 主循环
for t in "${TARGETS[@]}"; do
    case "${t}" in
        put)
            FILTER="BM_Put"
            TOGGLES=('*MemoryKVStore*Put*')
            ;;
        get)
            FILTER="BM_Get"
            TOGGLES=('*MemoryKVStore*Get*')
            ;;
        mixed)
            FILTER="BM_MixedReadWrite"
            TOGGLES=('*MemoryKVStore*Put*' '*MemoryKVStore*Get*')
            ;;
    esac

    OUT="${OUT_DIR}/cg_${t}.out"
    TXT="${OUT_DIR}/cg_${t}.txt"

    echo
    echo "==================== ${t}  (filter: ${FILTER}) ===================="
    rm -f "${OUT}"

    run_callgrind "${OUT}" "${TOGGLES[@]}" || true

    if [ "${PLAIN}" -eq 0 ] && ! has_data "${OUT}"; then
        echo "!! 区间限定没有命中任何调用, 自动改用 --plain 重跑一次" >&2
        PLAIN=1
        rm -f "${OUT}"
        run_callgrind "${OUT}" || true
    fi

    if ! has_data "${OUT}"; then
        echo "!! 采集结果为空: ${OUT}" >&2
        echo "   最常见原因: valgrind 没真正插桩(benchmark 的 ASLR 重入把 valgrind exec 掉了)." >&2
        echo "   本脚本已用 setarch -R 规避 (USE_SETARCH=${USE_SETARCH}); 若仍为空, 自查:" >&2
        echo "     setarch -R valgrind --version" >&2
        echo "     setarch -R valgrind --tool=callgrind /bin/echo hi   # 应产出非空文件" >&2
        continue
    fi

    callgrind_annotate --auto=yes --inclusive=yes --threshold=90 "${OUT}" \
        > "${TXT}" 2>/dev/null || true

    echo
    echo "---- ${t} 热点摘要 (完整内容: ${TXT}) ----"
    head -n 45 "${TXT}"
    echo "---- 摘要结束 ----"

    if [ "${GUI}" -eq 1 ]; then
        launch_gui "${OUT}"
    fi
done

echo
echo "生成的 profile 文件:"
find "${OUT_DIR}" -maxdepth 1 -name 'cg_*.out' 2>/dev/null | sort | sed 's/^/  /'
echo "随时可以手动打开:  kcachegrind ${OUT_DIR}/cg_put.out"
