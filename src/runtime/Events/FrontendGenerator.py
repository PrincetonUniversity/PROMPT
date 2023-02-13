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
    for name, event in api["events"].items():
        if "fn_name" not in event or "return_t" not in event or "parameters" not in event:
            print("Event is missing a field")
        # check that parameters is a dict
        if not isinstance(event["parameters"], dict):
            print("Parameters is not a dict")

    return api

# Import the queue protocol

if __name__ == "__main__":
    api = importAPI("api.yaml", "yaml")
    print(api)
    queue_protocol = PROMPTQueueProtocol.QueueProtocol(api)
    queue_protocol.generateAllProducerFunctions(is_decl=True)
