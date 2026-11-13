# Stateful vtable hooks

This document explains how vtable hooks and stateful hooks work in `vtablespp`.

## Basic concepts

A virtual call in C++ usually goes through a vtable:

```text
object -> vptr -> vtable -> function pointer
```

For example, given this class:

```cpp
struct Calculator {
    virtual ~Calculator() = default;
    virtual int add(int& a, int& b) {
        return a + b;
    }
};
```

MSVC commonly creates a vtable like this:

```text
vtable -> scalar deleting destructor
vtable -> Calculator::add[1]
```

Therefore, `Calculator::add` is located in slot `1`.

## Basic vtable hook

`vtablespp::slot_hook` replaces the pointer in a vtable slot with a regular function.

```cpp
using hook_type = vtablespp::slot_hook<int(int&, int&)>;

hook_type hook;
hook.install(table, 1, &hooked_add);
```

After installation:

```text
before:
vtable -> Calculator::add[1]

after:
vtable -> hooked_add[1]
```

The hook function must have an ABI-compatible signature:

```cpp
int hooked_add(void* object, int& a, int& b);
```

The first argument is the address of the object, passed as `void*`. The remaining arguments must match the arguments of the virtual method.

### Changing arguments

Only arguments passed by reference or pointer can be modified:

```cpp
virtual int add(int& a, int& b);
```

A hook can modify them before calling the original method:

```cpp
int hooked_add(void* object, int& a, int& b) {
    a *= 10;
    b *= 10;

    return original_add(object, a, b);
}
```

If the method takes arguments by value:

```cpp
virtual int add(int a, int b);
```

then the hook receives copies of the values. In this case, it is impossible to modify the arguments used by the calling code.

### Restoration

`uninstall()` writes the original pointer back into the vtable slot:

```cpp
hook.uninstall();
```

After that, calls go to the original method again.

## The stateful lambda problem

A stateful lambda captures variables:

```cpp
int multiplier = 10;

auto hook = [multiplier](void* object, int& a, int& b) {
    a *= multiplier;
    return 0;
};
```

This lambda is effectively an object of a class with state:

```cpp
struct HookState {
    int multiplier;
};
```

A vtable stores only a function address:

```cpp
int (*)(void*, int&, int&)
```

Therefore, a captureful lambda cannot be written directly into a vtable: the address of its code must be accompanied by its captured state.

## Solution: thunk and context

`vtablespp::stateful_slot_hook` solves this problem using two components:

1. A static thunk whose address can be stored in a vtable.
2. A context that stores the lambda state and the original vtable entry.

```text
vtable slot -> thunk_entry
thunk_entry -> context by object address
context -> user lambda
context -> original vtable entry
```

### What happens during install

```cpp
vtablespp::stateful_slot_hook<int(int&, int&)> hook;

hook.install(
    table,
    1,
    calculator,
    [multiplier = 10](
        vtablespp::slot_context<int(int&, int&)>& context,
        void* object,
        int& a,
        int& b
    ) {
        a *= multiplier;
        b *= multiplier;

        return context.call_original_raw(object, a, b);
    }
);
```

`install()` performs the following actions:

1. Checks that the slot exists and is not a pure-call entry.
2. Saves the current vtable entry as the original entry.
3. Creates a context for the object address.
4. Stores the original entry and the user lambda in the context.
5. Writes the address of `thunk_entry` into the vtable slot.

```text
before:
vtable -> Calculator::add[1]

after:
vtable -> stateful_slot_hook<int(int&, int&)>::thunk_entry[1]
```

### What happens during a call

```text
calculator.add(a, b)
        |
        v
vtable[1]
        |
        v
thunk_entry(object, a, b)
        |
        v
find context by object
        |
        v
user lambda
        |
        v
context.call_original_raw(object, a, b)
        |
        v
Calculator::add
```

The thunk receives:

```cpp
void* object
int& a
int& b
```

It looks up a context by the `object` address. If a context is found, the thunk calls the user lambda.

The lambda can:

- modify `a` and `b`;
- execute code before the original method;
- call the original method;
- execute code after the original method;
- modify the return value.

### Calling the original

The original entry is stored in the context:

```cpp
const void* original_entry;
```

`call_original_raw()` interprets it as:

```cpp
int (*)(void*, int&, int&)
```

and calls it with the same `object` and arguments:

```cpp
return context.call_original_raw(object, a, b);
```

This is important: `object` is passed as `void*` because that is the form in which the vtable receives it.

## Context registry

Each instantiation of `stateful_slot_hook<Signature>` has its own registry:

```cpp
std::unordered_map<void*, std::shared_ptr<context_storage>>
```

The key is the object address:

```text
object address -> context
```

The context contains:

```cpp
struct context_storage {
    const void* original_entry;
    callback_type hook;
};
```

Therefore, different objects can use different lambdas:

```text
calculator_a -> lambda with multiplier = 10
calculator_b -> lambda with multiplier = 100
```

However, the vtable remains shared:

```text
vtable -> thunk_entry[1]
```

The thunk selects the appropriate lambda by the address of the specific object.

## Limitations

### One hook per object and signature

Currently, two stateful hooks with the same signature cannot be installed for the same object:

```cpp
hook_a.install(table, 1, calculator, lambda_a);
hook_b.install(table, 1, calculator, lambda_b); // returns false
```

The reason is that the thunk finds a context only by the object address. Supporting multiple hooks requires extending the registry key to `(object address, vtable address, slot index)` and changing how the context is selected.

### The vtable is shared by the class

Replacing a vtable slot affects every object of that class:

```text
Calculator a;
Calculator b;

hook.install(table, 1, a, lambda);
```

Now both objects call `thunk_entry`. For `a`, the thunk finds a context and calls the lambda. For `b`, no context currently exists, so an exception is thrown.

### No fallback yet

The current thunk throws an exception when no context is found:

```cpp
throw std::runtime_error(
    "stateful hook context was not found"
);
```

The next step is to store the original entry using this key:

```text
vtable address + slot index -> original entry
```

Then the thunk can call the original method for objects without a context.

### Object lifetime

A context is tied to the address of an object. Therefore, before destroying an object, call:

```cpp
hook.uninstall();
```

Otherwise, a dangling pointer remains in the registry. Reuse of the same memory address can then lead to undefined behavior.

### Thread safety

The registry is protected by a `std::mutex`, but this does not make the entire hook fully thread-safe:

- installing and uninstalling a hook modifies the vtable;
- the vtable may be used by multiple threads;
- the user lambda must also be thread-safe if the method can be called concurrently.

## Current test

`tests/stateful_slot_hook_test.cpp` verifies:

- installation of a stateful hook;
- invocation of the user lambda;
- capture of lambda state;
- modification of reference arguments;
- invocation of the original method;
- restoration of the original entry after `uninstall()`;
- rejection of a second hook installation for the same object.