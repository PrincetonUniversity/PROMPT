# Generate a protocol for the PROMPTQueue
# It specifies the code for producing and consuming events

class QueueProtocol:
    api = {}
    custom_fields = []
    queue_object_name = "queue"

    # Constructor
    def __init__(self, api, custom_fields=None):
        # check the api is valid and has events
        if api is None or "events" not in api:
            raise Exception("Invalid API")

        self.api = api
        self.custom_fields = custom_fields

    def generateProducerFunction(self, event, is_decl=False):
        if event not in self.api['events']:
            return None

        # get the event
        event_object = self.api['events'][event]
        fn_name = event_object['fn_name']
        return_t = event_object['return_t']
        parameters = event_object['parameters']

        # generate the function line
        # return_t fn_name(type1 name1, type2 name2, ...)
        function_line = "%s %s(" % (return_t, fn_name)
        for p_name, p_type in parameters.items():
            function_line += "%s %s, " % (p_type, p_name)

        # remove the last comma
        if len(parameters) > 0:
            function_line = function_line[:-2]

        if is_decl:
            function_line += ");"

        return function_line

    def generateAllProducerFunctions(self, is_decl=False):
        for event in self.api['events']:
            print(self.generateProducerFunction(event, is_decl))

    def generateConsumerLoop(self):
        raise Exception("Not implemented")
