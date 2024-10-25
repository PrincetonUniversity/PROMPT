# PROMPT: A Fast and Extensible Memory Profiling Framework

PROMPT is a fast and extensible memory profiling framework that can be used to
gather memory dependences and other memory-related profiles in a program.

- Paper: https://dl.acm.org/doi/10.1145/3649827
- Artifact: https://zenodo.org/records/10783906

## Building PROMPT

### Dependencies

PROMPT currently requires LLVM 9 for building the preprocessing and
instrumentation passes.  It uses Clang for compiling itself and also requires
LLVM/Clang 9 for compiling programs for profiling. It uses LLVMgold for
link-time optimization.

### Build Commands

``` bash
cd PROMPT
export PATH=${llvm install bin directory}:$PATH
make build # By default install to PROMPT/build
make install # By default install to PROMPT/install
```

The `.rc` file is generated in the install directory. It sets up all the
environment variables for using PROMPT.

## Using PROMPT

### Preprocessing Program to Profile

Generate a single LLVM bitcode file (compile each source file into bitcode then
link together with `llvm-link`, or use other tools like
[Noelle](https://github.com/arcana-lab/noelle))

### Run PROMPT for a Specific Loop

1. Source PROMPT.rc generated in the install directory
2. If the program requires command line arguments, export `PROFILEARGS`
3. Run `prompt-driver` with  the bitcode file name and module name. And if the module targets one loop, also put function name, basic block name.

Example:

``` bash
source PROMPT/install/PROMPT.rc
PROFILEARGS="aminos 1234 123" prompt-driver benchmark.plain.bc md for.cond219
PROFILEARGS="$(PROFILEARGS)" prompt-driver  benchmark.plain.bc -m dep --target-loop for.cond219 --target-func md
```

See `prompt-driver --help` for more options.

### Run PROMPT with Loop Profile (decprecated, need CPF)

Run loop profile to generate `loopProf.out` (included in
[CPF](https://github.com/PrincetonUniversity/cpf))

When only providing one argument, `slamp-driver` will look for loop profile
output `loopProf.out` and use it to get all hot loops to profile.


## Add a New Profiling Module

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

### Make Sense of the Output

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


## FAQs

- Why LLVM 9?

PROMPT was developed and tested with LLVM 9. It was developed with many other
research compiler projects (NOELLE, CPF, SCAF, etc.) that were also using LLVM 9.
Since NOELLE and SCAF have been updated to LLVM 14, we have plans to update it
to support newer versions of LLVM (LLVM 14 and LLVM 18). Contributions are
welcome.

- What is SLAMP?

SLAMP was the original name for the single loop memory dependence profiler
before the project was extended and renamed to PROMPT. Not all names are
properly fixed after the name change so there are still reference to SLAMP. In
most context, SLAMP and PROMPT are interchangable.

- Is it still maintained?

Due to the capacity of the main author, the project is maintained in a best
effort basis.  However, we plan to improve automation and documentation for
PROMPT so it is easier to develop with PROMPT.  We also welcome contributions
from the community.

- Beyond LLVM Instrumentation

There is also some effort of supporting binary instrumentation with Pin in the
`Pin` branch.
