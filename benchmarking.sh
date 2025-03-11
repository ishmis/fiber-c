## Benchmarking Script for fiber-c
## All wasm programs are pre-compiled with the release version of wasmtime before benchmarks are run
## Currently supported comparisons: Unnamed v. Named WasmFX Handlers & Named WasmFX v. Named Asyncify
## NOTE: this requires hyperfine and its plotting scripts

make clean
make all -j

# Tooling paths
HYPERFINE_HIST_PLOT_PATH="../hyperfine/scripts/plot_histogram.py"
RELEASE_WASMTIME_PATH="../wasmfxtime/target/release/wasmtime"
RELEASE_WASMTIME_COMPILE="$RELEASE_WASMTIME_PATH compile -W=exceptions,function-references,stack-switching"

# Bench + Plotting params

# Named v. Unnamed WasmFX
NAMED_UNNAMED_ITERSUM_INPUT=2000
NAMED_UNNAMED_ASYNCIFY_CMPR_ITERSUM_RUNS=100
NAMED_UNNAMED_ASYNCIFY_CMPR_ITERSUM_TMIN=0.004
NAMED_UNNAMED_ASYNCIFY_CMPR_ITERSUM_TMAX=0.008

NAMED_UNNAMED_TREESUM_INPUT=25
NAMED_UNNAMED_TREESUM_RUNS=1
NAMED_UNNAMED_TREESUM_TMIN=1.0
NAMED_UNNAMED_TREESUM_TMAX=6.0

NAMED_UNNAMED_SIEVE_INPUT=100
NAMED_UNNAMED_SIEVE_RUNS=100
NAMED_UNNAMED_SIEVE_TMIN=0.004
NAMED_UNNAMED_SIEVE_TMAX=0.01

# Named Asyc v. WasmFX
ASYNCIFY_CMPR_ITERSUM_INPUT=2000
ASYNCIFY_CMPR_ITERSUM_RUNS=100
ASYNCIFY_CMPR_ITERSUM_TMIN=0.004
ASYNCIFY_CMPR_ITERSUM_TMAX=0.008

ASYNCIFY_CMPR_TREESUM_INPUT=25
ASYNCIFY_CMPR_TREESUM_RUNS=1
ASYNCIFY_CMPR_TREESUM_TMIN=1.0
ASYNCIFY_CMPR_TREESUM_TMAX=6.0

ASYNCIFY_CMPR_SIEVE_INPUT=100
ASYNCIFY_CMPR_SIEVE_RUNS=100
ASYNCIFY_CMPR_SIEVE_TMIN=0.004
ASYNCIFY_CMPR_SIEVE_TMAX=0.01

# Other configs
CLEAR_JSON=true # if false, saves all produced JSON from benchmarking

wtimec() {
    $RELEASE_WASMTIME_COMPILE $1
}

### Compile all examples

## Hello Examples
# no-prompt
wtimec "hello_asyncify.wasm"
wtimec "hello_wasmfx.wasm"

# prompt
wtimec "hello_prompt_asyncify.wasm"
wtimec "hello_prompt_wasmfx.wasm"

# forward prompt
wtimec "hello_forward_prompt_asyncify.wasm"
wtimec "hello_forward_prompt_wasmfx.wasm"

## Itersum Examples
# no-prompt
wtimec "itersum_asyncify.wasm"
wtimec "itersum_wasmfx.wasm"

# prompt
wtimec "itersum_prompt_asyncify.wasm"
wtimec "itersum_prompt_wasmfx.wasm"

## Treesum Examples
# no-prompt
wtimec "treesum_asyncify.wasm"
wtimec "treesum_wasmfx.wasm"

# prompt
wtimec "treesum_prompt_asyncify.wasm"
wtimec "treesum_prompt_wasmfx.wasm"

## Sieve Examples
# no-prompt
wtimec "sieve_asyncify.wasm"
wtimec "sieve_wasmfx.wasm"

# prompt
wtimec "sieve_prompt_asyncify.wasm"
wtimec "sieve_prompt_wasmfx.wasm"

# clear all wasm files
rm *.wasm

### Benchmark pre-compiled

bench_named_unnamed() {
    hyperfine -w 20 -r $6 \
        "../wasmfxtime/target/release/wasmtime -W=exceptions,function-references,stack-switching --allow-precompiled $1_wasmfx.cwasm $2" \
        "../wasmfxtime/target/release/wasmtime -W=exceptions,function-references,stack-switching --allow-precompiled $1_prompt_wasmfx.cwasm $2" \
        --export-json $1_named_unnamed.json

    python3.11 $HYPERFINE_HIST_PLOT_PATH \
        --title="$3" \
        --type="bar" \
        --legend="upper right" \
        --labels="unnamed,named" \
        --t-min=$4 \
        --t-max=$5 \
        $1_named_unnamed.json
}

## Named WasmFX v. Unnamed WasmFX

# hello
bench_named_unnamed "hello" "" "Hello Unnamed v. Named Handlers" "0.004" "0.008" "100"

# itersum
bench_named_unnamed "itersum" \
    "$NAMED_UNNAMED_ITERSUM_INPUT" \
    "Itersum Unnamed v. Named Handlers (input: $NAMED_UNNAMED_ITERSUM_INPUT)" \
    "$NAMED_UNNAMED_ITERSUM_TMIN" \
    "$NAMED_UNNAMED_ITERSUM_TMAX" \
    "$NAMED_UNNAMED_ITERSUM_RUNS"

# treesum
bench_named_unnamed "treesum" \
    "$NAMED_UNNAMED_TREESUM_INPUT" \
    "Treesum Unnamed v. Named Handlers (input: $NAMED_UNNAMED_TREESUM_INPUT)" \
    "$NAMED_UNNAMED_TREESUM_TMIN" \
    "$NAMED_UNNAMED_TREESUM_TMAX" \
    "$NAMED_UNNAMED_TREESUM_RUNS"

# sieve
bench_named_unnamed "sieve" \
    "$NAMED_UNNAMED_SIEVE_INPUT" \
    "Sieve Unnamed v. Named Handlers (input: $NAMED_UNNAMED_SIEVE_INPUT)" \
    "$NAMED_UNNAMED_SIEVE_TMIN" \
    "$NAMED_UNNAMED_SIEVE_TMAX" \
    "$NAMED_UNNAMED_SIEVE_RUNS"

bench_prompt_wasmfx_asyncify() {
    hyperfine -w 20 -r $6 \
        "../wasmfxtime/target/release/wasmtime -W=exceptions,function-references,stack-switching --allow-precompiled $1_prompt_asyncify.cwasm $2" \
        "../wasmfxtime/target/release/wasmtime -W=exceptions,function-references,stack-switching --allow-precompiled $1_prompt_wasmfx.cwasm $2" \
        --export-json $1_prompt_asyncify_wasmfx.json

    python3.11 $HYPERFINE_HIST_PLOT_PATH \
        --title="$3" \
        --type="bar" \
        --legend="upper right" \
        --labels="asyncify,wasmfx" \
        --t-min=$4 \
        --t-max=$5 \
        $1_prompt_asyncify_wasmfx.json
}

## Named WasmFX v. Asyncify

# hello
bench_prompt_wasmfx_asyncify "hello" "" "Hello Prompt Asyncify v. WasmFX" "0.004" "0.008" "100"

# itersum
bench_prompt_wasmfx_asyncify "itersum" \
    "$ASYNCIFY_CMPR_ITERSUM_INPUT" \
    "Itersum Prompt Asyncify v. WasmFX (input: $ASYNCIFY_CMPR_ITERSUM_INPUT)" \
    "$ASYNCIFY_CMPR_ITERSUM_TMIN" \
    "$ASYNCIFY_CMPR_ITERSUM_TMAX" \
    "$ASYNCIFY_CMPR_ITERSUM_RUNS"

# treesum
bench_prompt_wasmfx_asyncify "treesum" \
    "$ASYNCIFY_CMPR_TREESUM_INPUT" \
    "Treesum Prompt Asyncify v. WasmFX (input: $ASYNCIFY_CMPR_TREESUM_INPUT)" \
    "$ASYNCIFY_CMPR_TREESUM_TMIN" \
    "$ASYNCIFY_CMPR_TREESUM_TMAX" \
    "$ASYNCIFY_CMPR_TREESUM_RUNS"

# sieve
bench_prompt_wasmfx_asyncify "sieve" \
    "$ASYNCIFY_CMPR_SIEVE_INPUT" \
    "Sieve Prompt Asyncify v. Non-Pooling WasmFX (input: $ASYNCIFY_CMPR_SIEVE_INPUT)" \
    "$ASYNCIFY_CMPR_SIEVE_TMIN" \
    "$ASYNCIFY_CMPR_SIEVE_TMAX" \
    "$ASYNCIFY_CMPR_SIEVE_RUNS"

# clear all JSON files if CLEAR_JSON
if [ "$CLEAR_JSON" = true ]; then
    rm *.json
fi

# clear all precompiled files
rm *.cwasm
