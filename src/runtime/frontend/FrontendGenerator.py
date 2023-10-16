# Take the specification of the queue and the events
# and generate the frontend code.
import os
import json
import yaml
import argparse
import PROMPTQueueProtocol
from pprint import pprint


# Import the API from a configuration file
def importAPI(api_file, api_format="yaml"):
    with open(api_file, "r") as f:
        if api_format == "json":
            api = json.load(f)
        elif api_format == "yaml":
            api = yaml.load(f, Loader=yaml.FullLoader)
        else:
            raise Exception("Unknown API format: %s" % api_format)

    # validate the structure
    # check it has "events"
    if "events" not in api:
        raise Exception("No events in API specification")

    # api["events"] is a list
    # a list of events with [fn_name, return_t , [parameters]]
    for name, parameters in api["events"].items():
        # parameter can be empty
        if parameters is None:
            continue

        # check that parameters is a dict
        if not isinstance(parameters, dict):
            raise Exception("Parameters for event %s is not a dict" % name)

        for param_name, param_size in parameters.items():
            # check that param_size is an int and a multiple of 8
            if not isinstance(param_size, int):
                raise Exception(
                    "Parameter %s for event %s is not an int" % (param_name, name)
                )

            if param_size % 8 != 0:
                raise Exception(
                    "Parameter %s for event %s is not a multiple of 8"
                    % (param_name, name)
                )

    return api


def importModuleSpec(api, module_event_file, spec_format="yaml"):
    with open(module_event_file, "r") as f:
        if spec_format == "json":
            mod_events = json.load(f)
        elif spec_format == "yaml":
            mod_events = yaml.load(f, Loader=yaml.FullLoader)
        else:
            raise Exception("Unknown API format: %s" % spec_format)

    # validate the structure
    # check it has "events"
    if "events" not in mod_events:
        raise Exception("No events in module specification")

    # check that the events are in the API, and that the parameters is a subset
    for name, parameters in mod_events["events"].items():
        api_parameters = api["events"][name]
        # is a subset
        for p in parameters:
            if p not in api_parameters:
                raise Exception(
                    "Parameter %s for event %s is not in the API" % (p, name)
                )

    return mod_events


# Import the queue protocol

if __name__ == "__main__":
    # Parse the arg "--module" or "-m", options are "dep, ol, pt, and lv"
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-m",
        "--module",
        help="Module to generate frontend for",
        choices=["wp-dep", "dep", "dep-context", "ol", "pt", "lv", "privateer"],
    )
    parser.add_argument("--config-dir", help="Config directory", required=True)
    parser.add_argument("-o", "--output", help="Output file")
    parser.add_argument(
        "-t", "--template", help="Template file", default="custom_produce.h"
    )
    args = parser.parse_args()
    module_to_yaml = {
        "wp-dep": "WholeProgramDepModEvents.yaml",
        "dep": "DepModEvents.yaml",
        "dep-context": "DepModule_TrackContext_Events.yaml",
        "ol": "ObjectLifetimeModEvents.yaml",
        "pt": "PointsToModEvents.yaml",
        "lv": "LoadedValueModEvents.yaml",
        "privateer": "PrivateerProfilerEvents.yaml",
    }

    # check if config dir is valid
    if not os.path.isdir(args.config_dir):
        print(f"Config directory {args.config_dir} does not exist")
        exit(1)

    # check if template file exists
    if not os.path.isfile(args.template):
        print(f"Template file {args.template} does not exist")
        exit(1)

    api = importAPI(os.path.join(args.config_dir, "api.yaml"), "yaml")
    queue_protocol = PROMPTQueueProtocol.QueueProtocol(api)

    mod_spec = importModuleSpec(
        api, os.path.join(args.config_dir, module_to_yaml[args.module]), "yaml"
    )

    pprint(mod_spec)
    lines = queue_protocol.generateAllProducerFunctions(mod_spec["events"])

    output = args.output or "slamp_produce.h"

    with open(args.template, "r") as f:
        template = f.readlines()

    with open(output, "w") as f:
        f.writelines(template)
        f.writelines("\n".join(lines))
