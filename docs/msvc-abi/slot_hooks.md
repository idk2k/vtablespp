
# How it works

# Stateful per-object hook

Why per object? Because thunk contains manager to handle it
Why stateful? Because we not change vtable function pointer to our hook function - instead we change it to thunk, which then handles all lambdas objects in global context.


calculator.add(a, b)
        |
        v
vtable[1] -> stateful_slot_hook<int(int&, int&)>::thunk
        |
        v
thunk находит context по адресу calculator
        |
        v
твоя lambda с multiplier = 10
        |
        v
context.call_original(...)
        |
        v
Calculator::add
