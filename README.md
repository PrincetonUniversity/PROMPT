## PROMPT: A Fast and Extensible Memory Profiling Framework

### Build
- Required software: NOELLE, SCAF (by default turned on in NOELLE)

```
cd PROMPT
export SCAF_INSTALL_DIR=${the directory where SCAF's include and lib are installed (could be the same as NOELLE_INSTALL_DIR)}
export NOELLE_INSTALL_DIR=${the directory where NOELLE's include and lib are installed}
export PATH=${llvm install bin directory}:$PATH
mkdir build
cd build
cmake ../src -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1  -DCMAKE_INSTALL_PREFIX=`pwd`/../install
make -j8
make install
```

### Run

Export the `SLAMP_HOOKS`, `CONSUMER_BINARY`, `SLAMP_LIB_PATH`, and optionally `PROFILEARGS`, and run `slamp-driver` with (1) the bitcode file name, (2) function name, and (3) basic block name.

Example:
`SLAMP_HOOKS=~/PROMPT/install/runtime/libslamp_hooks_custom.a CONSUMER_BINARY=~/PROMPT/install/bin/consumer_custom SLAMP_LIB_PATH=~/PROMPT/install/lib/libSLAMP.so PROFILEARGS="aminos 391519156 1000"  ~/PROMPT/scripts/slamp-driver benchmark.plain.bc md for.cond219`

#### Preprocessing

- Single LLVM bitcode file
- Metadata ID for functions, basic blocks, and instructions (as a part of `slamp-driver`)

### TODOs

#### Decoupling
- [x] Compile with NOELLE and SCAF
- [x] Compile with NOELLE and SCAF (without Speculation Modules)
- [x] Seperate all profiling modules out
- [ ] Convert producer library to be configurable
- [ ] Make LTO optional

#### Implementation
- [ ] Multithreaded profiling?
- [ ] Multiple loops at the same time
- [ ] Package the components better
    - The queue
    - The container
- [ ] Parallel Containers
    - Replace the vector with a lower-latency memory region
- [ ] External function handling
- [ ] 16 bytes load and store handling

#### Integration
- [ ] Replace SpecPriv profiler and LAMP with PROMPT
    - Check the problem with the failed and long-running benchmarks

#### Debug & Testing
- [ ] Check the slowdown problem with multiple backend running together
- [ ] Check the slowdown problem when multiple containers running together
- [ ] Test with newer version of LLVM
- [ ] Test with way bigger benchmarks

#### Documentation
- [ ] Preprocessing tasks
- [ ] Simple demo
- [ ] Get started doc
- [ ] Extending with new profiler doc

### Modules





