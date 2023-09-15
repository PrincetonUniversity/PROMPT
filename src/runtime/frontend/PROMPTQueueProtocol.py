# Generate a protocol for the PROMPTQueue
# It specifies the code for producing and consuming events

from typing import Dict, List

class QueueProtocol:
    queue_object_name = "queue"

    # Constructor
    def __init__(self, api, custom_fields=None):
        # check the api is valid and has events
        if api is None or "events" not in api:
            raise Exception("Invalid API")

        self.api = api
        self.custom_fields = custom_fields

    def generateProducerFunction(self, event, values=None):
        if event not in self.api['events']:
            return None

        # get the event
        parameters = self.api['events'][event]

        define_str = "#define PRODUCE_%s" % event.upper()
        function_str = "produce_8"
        parameter_str = "("
        args_str = "(" + event.upper()

        # produce_<size>_<size>(<name>, <name>, ...)
        if parameters is not None:
            for p_name, p_size in parameters.items():
                parameter_str += "%s, " % p_name
                if values is not None:
                    if p_name not in values:
                        continue
                function_str += "_%d" % p_size
                args_str += ", %s" % p_name
            parameter_str = parameter_str[:-2] # remove the last comma

        parameter_str += ")"
        args_str += ")"

        return "%s%s %s%s" % (define_str, parameter_str, function_str, args_str)

    def generateAllProducerFunctions(self, mod_events=None):
        lines = []
        if mod_events is None:
            for event in self.api['events']:
                lines.append(self.generateProducerFunction(event))
        else:
            for event in mod_events:
                values = mod_events[event]
                lines.append(self.generateProducerFunction(event, values))
        return lines

    def generateConsumerLoop(self):
        raise Exception("Not implemented")
