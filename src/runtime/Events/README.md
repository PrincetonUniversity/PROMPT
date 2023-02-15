## Function

This part handles the events.

## Procedure

- SLAMP specifies all possible events
    - It specifies the event API through a YAML file
    - It provides a frontend header template with "stubs"
        - A stub is `PRODUCE(A, B, C)` and should be replaced by queue specific produce statements
    - It provides a backend driver template with "stubs"
        - A stub is `CONSUME(A, B, C)` and should be replaced by queue specific consume statements
- Each module specifies: the events, and the values used in the events, in a YAML file
- Each queue implementation specify the protocol with a Python file
    - When I see a produce or consume stub, how to replace with produce and consume
        - It only replaces the variables that the profiling module requires
        - E.g., for event `load` and requires `inst_id, value`, with custom queue, it should replace `PRODUCE(load, A, C)` with `custom_produce_8_32_64(load, A, C)`
- Generate frontend and backend code
    - Does optimization in Python => determine the protocol (how big is the packet)
    - Create a ".c" file to link with the frontend
    - Create a backend loop
