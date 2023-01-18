## PROMPT: A Fast and Extensible Memory Profiling Framework

### TODOs

#### Decoupling
- [x] Compile with NOELLE and SCAF
- [ ] Compile with NOELLE and optional SCAF
- [x] Seperate all profiling modules out
- [ ] Convert producer library to be configurable

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

### Preprocessing

- Single LLVM bitcode file (is it required?)
- Metadata ID for functions, basic blocks, and instructions

### Modules





