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

### Use PROMPT

#### Preprocessing

1. Generate a single LLVM bitcode file
2. (No need to do it manually if specifying a pair of function and basic block in `slamp-driver`) Annoteate Metadata ID for functions, basic blocks, and instructions with `-metadata-namer` pass
3. (optional) Run loop profile to generate `loopProf.out`

#### Run PROMPT

1. Export the `SLAMP_INSTALL_DIR`, and optionally `PROFILEARGS`
2. Run `slamp-driver` with one or three command line arguments, (1) the bitcode file name, and optionally (2) function name, and (3) basic block name. When only providing one argument, `slamp-driver` will look for loop profile output `loopProf.out` and use it to get all hot loops to profile.

Example:
```bash
export SLAMP_INSTALL_DIR=~/PROMPT/install/
# Or ~/PROMPT/scripts/slamp-driver benchmark.bc if there is proper loop profile
~/PROMPT/scripts/slamp-driver benchmark.plain.bc md for.cond219
```

#### Make Sense of the Output

The default output is at result.slamp.profile.

When used as a memory dependence profiler, PROMPT generates a output file where each dependence consists of six numbers, following this format:
```
[loop id, source id, bare destination id, destination id, is loop carried, count]
```

where:

- loop id: the basic block metadata id of the loop
- source id: the instruction id of the source instruction (have to be an instruction in the loop, can be a function call)
- bare destination id: the instruction id where store happens (can be outside of the loop, not a function call)
- destination id: the instruction id of the source instruction (have to be an instruction in the loop, can be a function call)
- is loop carried: 0 for intra-iteration; 1 for loop-carried
- count: always 1 if counting is turned off, otherwise the dynamic count of the dependence

Note that the first dependence of a loop is always `[loop id, 0, 0, 0, 0, 0]`, showing that a loop has been profiled.

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





