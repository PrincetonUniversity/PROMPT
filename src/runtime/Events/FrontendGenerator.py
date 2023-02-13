# Take the specification of the queue and the events
# and generate the frontend code.
import json
import yaml
import PROMPTQueueProtocol

# Import the API from a configuration file
def importAPI(api_file, api_format="json"):

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
        print("No events in API")
        return None

    # api["events"] is a list
    # a list of events with [fn_name, return_t , [parameters]]
    for name, parameters in api["events"].items():
        # parameter can be empty
        if parameters is None:
            continue

        # check that parameters is a dict
        if not isinstance(parameters, dict):
            print("Parameters for %s is not a dict" % name)
            return None

        for param_name, param_size in parameters.items():
            # check that param_size is an int and a multiple of 8
            if not isinstance(param_size, int):
                print("Parameter %s for %s is not an int" % (param_name, name))
                return None

            if param_size % 8 != 0:
                print("Parameter %s for %s is not a multiple of 8" % (param_name, name))
                return None

    return api

# Import the queue protocol

if __name__ == "__main__":
    api = importAPI("api.yaml", "yaml")
    print(api)
    queue_protocol = PROMPTQueueProtocol.QueueProtocol(api)
    queue_protocol.generateAllProducerFunctions(is_decl=True)
