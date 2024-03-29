## PROMPT: A Fast and Extensible Memory Profiling Framework

### Build

Need to use LLVM/Clang as the compiler for the support of LLVM.
Need to make sure LLVMgold is installed.

```
cd PROMPT
export PATH=${llvm install bin directory}:$PATH
make build # By default install to PROMPT/build
make install # By default install to PROMPT/install
```

The .rc file is generated in PROMPT/install

### Use PROMPT

#### Preprocessing

1. Generate a single LLVM bitcode file
2. (No need to do it manually if specifying a pair of function and basic block in `slamp-driver`) Annoteate Metadata ID for functions, basic blocks, and instructions with `-metadata-namer` pass
3. (optional) Run loop profile to generate `loopProf.out`

#### Run PROMPT

1. Source PROMPT.rc generated in the install directory
2. If the program requires command line arguments, export `PROFILEARGS`
3. Run `slamp-driver` with one or three command line arguments, (1) the bitcode file name, and optionally (2) function name, and (3) basic block name. When only providing one argument, `slamp-driver` will look for loop profile output `loopProf.out` and use it to get all hot loops to profile.

Example:
```bash
source PROMPT/install/PROMPT.rc
# Or slamp-driver benchmark.bc if there is proper loop profile
PROFILEARGS="aminos 1234 123" slamp-driver benchmark.plain.bc md for.cond219
```

### Add a New Profiling Module

- Event `src/runtime/Events/configs`
  - Add a new config file in the events
- Frontend `src/runtime/frontend`
  - Add in the CMakeLists.txt
  - Modify the `FrontendGenerator.py` to add a new module
- Logic `src/runtime/ProfilingModules`
  - Implement and add header `.cpp` and `.h` file
  - Register the `.cpp` file in the `CMakeLists.txt`
- Backend
  - Currently in `SLAMPcustom/consumer/consumer.cpp`
    - Add a new loop to process the events, import the header and instantiate the module
  - Will be moved to `src/runtime/backend`

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
- [x] Make LTO optional
- [x] Convert producer library to be configurable

#### Implementation
- [ ] Replace malloc hook (removed in glibc 2.35)
- [ ] External function handling: currently disabled in `SLAMP.cpp` by not doing the replacements
- [ ] External function handling with correct report of allocation events
- [ ] Multithreaded profiling?
- [ ] Multiple loops at the same time
- [ ] Package the components better
    - The queue
    - The container
- [ ] Parallel Containers
    - Replace the vector with a lower-latency memory region
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

