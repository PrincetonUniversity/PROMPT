#!/usr/bin/env python3
import os
import shlex
import subprocess
import random
import argparse
import time
import contextlib

SLAMP_INSTALL_DIR = os.environ.get("SLAMP_INSTALL_DIR")
CONSUMER_BINARY = f"{SLAMP_INSTALL_DIR}/bin/consumer_custom"
SLAMP_LIB_PATH = f"{SLAMP_INSTALL_DIR}/lib/libSLAMP.so"
MALLOC_LIB = f"-L{SLAMP_INSTALL_DIR}/lib -lmalloc_hook"

# TODO: a better way to pass the arguments
PROFILEARGS = os.environ.get("PROFILEARGS", "")
RUNTIME_LTO = os.environ.get("RUNTIME_LTO")
LINKING_OPTS = os.environ.get("LINKING_OPTS", "")
DEFAULT_LIBS = os.environ.get("DEFAULT_LIBS", "")

GREEN = "\033[0;32m"
RED = "\033[0;31m"
NC = "\033[0m"


@contextlib.contextmanager
def get_shared_mem_queue():
    queue_name = None
    try:
        slamp_queue_id = random.randint(0, 999)

        while os.path.exists(f"/dev/shm/slamp_queue_{slamp_queue_id}"):
            slamp_queue_id = random.randint(0, 999)

        queue_name = f"/dev/shm/slamp_queue_{slamp_queue_id}"
        yield slamp_queue_id
    finally:
        if queue_name is not None and os.path.exists(queue_name):
            os.remove(queue_name)


def get_named_bc(bc_file):
    """Run the namer, so the bitcode has named metadata."""
    named_bc = os.path.basename(bc_file).replace(".bc", ".named.bc")
    print("Running namer pass")
    subprocess.run(
        [
            "opt",
            args.bc_file,
            "-o",
            named_bc,
            "-load",
            SLAMP_LIB_PATH,
            "-prompt-metadata-namer",
        ],
        check=True,
    )
    return named_bc


def compile_frontend(bc_file, module, target_fcn, target_loop, compile_output):
    slamp_hooks = f"{SLAMP_INSTALL_DIR}/runtime/libslamp_hooks_custom_{module}.a"

    if not os.path.exists(slamp_hooks):
        raise RuntimeError(f"{slamp_hooks} does not exist")

    prelink_bc = bc_file.replace(".bc", ".slamp.prelink.bc")
    prelink_obj = bc_file.replace(".bc", ".slamp.prelink.o")

    exe = bc_file.replace(".bc", ".slamp.exe")

    instrument_cmd = (
        f"opt -load {SLAMP_LIB_PATH} -slamp-insts -o {prelink_bc} {bc_file}"
    )
    if target_loop is None:
        instrument_cmd += " -slamp-target-loop-enabled=0"
    else:
        instrument_cmd += (
            f" -slamp-target-loop-enabled=1 -slamp-target-loop={target_loop}"
        )
    if target_fcn is not None:
        instrument_cmd += f" -slamp-target-fn={target_fcn}"

    print(f"{GREEN}Instrumenting{NC}: {instrument_cmd}")
    subprocess.run(
        shlex.split(instrument_cmd),
        check=True,
        stdout=compile_output,
        stderr=compile_output,
    )

    compile_cmd = f"clang -c -o {prelink_obj} {prelink_bc}"
    if RUNTIME_LTO == "ON":
        compile_cmd += " -flto"

    print(f"{GREEN}Compiling{NC}:  {compile_cmd}")
    subprocess.run(
        shlex.split(compile_cmd),
        check=True,
        stdout=compile_output,
        stderr=compile_output,
    )

    link_cmd = (
        f"clang++ -no-pie -O2 {prelink_obj} {slamp_hooks} {MALLOC_LIB} -o {exe} -g"
        f"{LINKING_OPTS} -lunwind {DEFAULT_LIBS} -ldl -lutil -lpthread -lrt"
    )

    if RUNTIME_LTO == "ON":
        link_cmd += " -flto"

    print(f"{GREEN}Linking{NC}: {link_cmd}")
    subprocess.run(
        shlex.split(link_cmd), check=True, stdout=compile_output, stderr=compile_output
    )


def drive(exe, module_idx, threads, timeout=7200):
    with get_shared_mem_queue() as slamp_queue_id, open(
        f"consumer.log", "w"
    ) as consumer_log_fd, open(f"producer.log", "w") as producer_log_fd:
        print(f"{GREEN}Running PROMPT with queue id {slamp_queue_id}{NC}")

        env = os.environ.copy()
        env["SLAMP_QUEUE_ID"] = f"{slamp_queue_id}"
        # run the CONSUMER_BINARY in the background
        p_consumer = subprocess.Popen(
            [CONSUMER_BINARY, "--module", str(module_idx), "--threads", str(threads)],
            env=env,
            stdout=consumer_log_fd,
            stderr=consumer_log_fd,
        )

        # wait for 1 seconds for the CONSUMER_BINARY to start
        time.sleep(1)

        producer_cmd = [exe] + shlex.split(PROFILEARGS)
        p_producer = subprocess.Popen(
            producer_cmd,
            env=env,
            stdout=producer_log_fd,
            stderr=producer_log_fd,
        )

        start_time = time.monotonic()
        try:
            while True:
                if p_producer.poll() is not None:
                    break
                if p_consumer.poll() is not None:
                    break

                if time.monotonic() - start_time > timeout:
                    raise subprocess.TimeoutExpired(p_producer.args, timeout)

                time.sleep(0.1)

            # if p_consumer is still running, wait for it up to 120 seconds
            if p_consumer.poll() is None:
                p_consumer.wait(timeout=120)

            # if p_producer is still running, it's a problem
            if p_producer.poll() is None:
                raise RuntimeError("Producer is running after consumer finished")

        except subprocess.TimeoutExpired:
            # FIXME: this doesn't cover the case where the consumer time out after 10s
            print(f"{RED}PROMPT timed out after {timeout}s {NC}")
            # kill if any of the process is still running
            if p_producer.poll() is None:
                p_producer.kill()
            if p_consumer.poll() is None:
                p_consumer.kill()

        except RuntimeError as e:
            print(f"{RED}PROMPT failed due to {e} {NC}")
            if p_producer.poll() is None:
                p_producer.kill()
            if p_consumer.poll() is None:
                p_consumer.kill()

        if p_producer.returncode != 0 or p_consumer.returncode != 0:
            print(f"{RED}PROMPT failed{NC}")
            print(f"Consumer return code: {p_consumer.returncode}")
            print(f"Producer return code: {p_producer.returncode}")
            exit(-1)
        else:
            print(f"{GREEN}PROMPT finished successfully{NC}")

        end_time = time.monotonic()
        return end_time - start_time


if __name__ == "__main__":
    argparser = argparse.ArgumentParser()
    argparser.add_argument(
        "bc_file", help="The bitcode file to run", default=None, nargs="?"
    )
    argparser.add_argument(
        "-m",
        "--module",
        help="The module to run",
        default="dep",
        choices=["dep", "lv", "pt", "ol", "wp-dep", "dep-context", "privateer"],
    )

    argparser.add_argument("-t", "--threads", help="Number of threads", default=1)
    argparser.add_argument("--target-fcn", help="The target function to run")
    argparser.add_argument("--target-loop", help="The target loop to run")
    argparser.add_argument("--skip-build", help="Skip build", action="store_true")
    argparser.add_argument("--skip-run", help="Skip run", action="store_true")
    argparser.add_argument("--output", help="Output file")
    argparser.add_argument("--timeout", help="Timeout", default=72000)
    argparser.add_argument("--named", help="Provide the named bc", default=None)
    argparser.add_argument("--exe", help="The executable to run", default=None)
    argparser.add_argument(
        "--runtime-file", help="The file to store runtime", default="slamp.time"
    )
    argparser.add_argument("--stop-at-named", help="Get the named bitcode then exit", action="store_true")
    args = argparser.parse_args()

    # if no bc_file is provided, has to provide the executable or named bc file
    if args.bc_file is None and args.exe is None and args.named is None:
        raise RuntimeError("Either bc_file or exe has to be provided")

    if args.exe is not None and args.skip_build is False:
        raise RuntimeError("Cannot both build and provide exe, turn on --skip-build")

    # TODO: check for target_fcn and target_loop for module that requires them
    if not args.skip_build:
        # check if the bitcode file exists
        if args.named and not os.path.exists(args.named):
            raise RuntimeError(f"{args.named} does not exist")
        if args.bc_file and not os.path.exists(args.bc_file):
            raise RuntimeError(f"{args.bc_file} does not exist")

        # TODO: this is optional, if existing named bitcode is provided, we can skip this step
        if args.named is not None:
          named_bc = args.named
        else:
          named_bc = get_named_bc(args.bc_file)
        # exit after getting the named bitcode
        if(args.stop_at_named):
          exit(1)
        with open("compile.log", "w") as compile_output:
            compile_frontend(
                named_bc, args.module, args.target_fcn, args.target_loop, compile_output
            )
        exe = named_bc.replace(".bc", ".slamp.exe")
    else:
        print(f"{GREEN}Skip build{NC}")
        exe = args.exe or args.bc_file.replace(".bc", ".slamp.exe")
        print(f"{GREEN}Using {exe} as the executable{NC}")

    # enum AvailableModules {
    #   DEPENDENCE_MODULE = 0,
    #   POINTS_TO_MODULE = 1,
    #   LOADED_VALUE_MODULE = 2,
    #   OBJECT_LIFETIME_MODULE = 3,
    #   WHOLE_PROGRAM_DEPENDENCE_MODULE = 4,
    #   NUM_MODULES = 5
    # };
    module_to_index = {
        "dep": 0,
        "dep-context": 6,
        "pt": 1,
        "lv": 2,
        "ol": 3,
        "wp-dep": 4,
        "privateer": 5,
    }
    module_index = module_to_index[args.module]
    if not args.skip_run:
        if not os.path.exists(exe):
            raise RuntimeError(f"{exe} does not exist")
        # get the relative path to the executable
        exe = os.path.abspath(exe)
        run_time = drive(exe, module_index, args.threads, timeout=args.timeout)

        print(f"{GREEN}Run time{NC}: {run_time}s")

        # write run_time to a file
        with open(args.runtime_file, "w") as run_time_fd:
            run_time_fd.write(str(run_time))

    # FIXME: not all modules generate deplog.txt
    slamp_output = args.output or "benchmark.result.slamp.profile"
    if os.path.exists("deplog.txt"):
        subprocess.run(["mv", "deplog.txt", slamp_output], check=True)
