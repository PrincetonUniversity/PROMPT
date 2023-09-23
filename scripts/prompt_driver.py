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

            # if p_consumer is still running, wait for it up to 10 seconds
            if p_consumer.poll() is None:
                p_consumer.wait(timeout=10)

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
            print(f"Consumer return codeL {p_consumer.returncode}")
            print(f"Producer return codeL {p_producer.returncode}")
        else:
            print(f"{GREEN}PROMPT finished successfully{NC}")

        end_time = time.monotonic()
        return end_time - start_time


if __name__ == "__main__":
    argparser = argparse.ArgumentParser()
    argparser.add_argument("bc_file", help="The bitcode file to run")
    argparser.add_argument(
        "-m",
        "--module",
        help="The module to run",
        default="dep",
        choices=["dep", "lv", "pt", "wp-dep", "dep-context"],
    )
    argparser.add_argument("-t", "--threads", help="Number of threads", default=1)
    argparser.add_argument("--target-fcn", help="The target function to run")
    argparser.add_argument("--target-loop", help="The target loop to run")
    argparser.add_argument("--skip-build", help="Skip build", action="store_true")
    argparser.add_argument("--skip-run", help="Skip run", action="store_true")
    argparser.add_argument("--output", help="Output file")
    argparser.add_argument("--timeout", help="Timeout", default=7200)
    args = argparser.parse_args()

    # check if the bitcode file exists
    if not os.path.exists(args.bc_file):
        raise RuntimeError(f"{args.bc_file} does not exist")

    # TODO: this is optional, if existing named bitcode is provided, we can skip this step
    named_bc = get_named_bc(args.bc_file)

    # TODO: check for target_fcn and target_loop for module that requires them
    if not args.skip_build:
        with open("compile.log", "w") as compile_output:
            compile_frontend(
                named_bc, args.module, args.target_fcn, args.target_loop, compile_output
            )
    # enum AvailableModules {
    #   DEPENDENCE_MODULE = 0,
    #   POINTS_TO_MODULE = 1,
    #   LOADED_VALUE_MODULE = 2,
    #   OBJECT_LIFETIME_MODULE = 3,
    #   WHOLE_PROGRAM_DEPENDENCE_MODULE = 4,
    #   NUM_MODULES = 5
    # };

    # map module to the corresponding index
    module_to_index = {
        "dep": 0,
        "dep-context": 0,
        "pt": 1,
        "lv": 2,
        "lt": 3,
        "wp-dep": 4,
    }
    module_index = module_to_index[args.module]
    exe = "./" + named_bc.replace(".bc", ".slamp.exe")
    if not args.skip_run:
        if not os.path.exists(exe):
            raise RuntimeError(f"{exe} does not exist")
        run_time = drive(exe, module_index, args.threads, timeout=args.timeout)

        print(f"{GREEN}Run time{NC}: {run_time}s")

    # FIXME: not all modules generate deplog.txt
    slamp_output = args.output or "benchmark.result.slamp.profile"
    if os.path.exists("deplog.txt"):
        subprocess.run(["mv", "deplog.txt", slamp_output], check=True)
