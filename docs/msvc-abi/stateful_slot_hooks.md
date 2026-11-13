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
5. Saves the original entry as the fallback for objects without a context.
6. Writes the address of `thunk_entry` into the vtable slot.

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
        +--> context found -> user lambda
        |
        +--> context not found -> fallback original entry
```

The thunk receives:

```cpp
void* object
int& a
int& b
```

It looks up a context by the `object` address.

If a context is found, the thunk calls the user lambda.

If no context is found, the thunk calls the fallback original entry.

## Fallback behavior

A stateful hook changes a vtable slot that is shared by every object of the class. Therefore, the thunk must know what to do for objects that do not have their own hook context.

`stateful_slot_hook` stores three static fallback values:

```cpp
static const void* fallback_original_entry;
static const void* fallback_vtable_address;
static std::size_t fallback_slot_index;
```

- `fallback_original_entry` stores the original vtable function pointer.
- `fallback_vtable_address` stores the address of the hooked vtable.
- `fallback_slot_index` stores the hooked slot index.

During `install()`, the original entry is saved both in the object-specific context and in the static fallback fields:

```cpp
storage->original_entry = *entry;
storage->hook = std::forward<Callback>(callback);

fallback_original_entry = *entry;
fallback_vtable_address = table.vtable_address();
fallback_slot_index = index;
```

When the thunk cannot find a context for an object, it calls the fallback original entry:

```text
unhooked object
        |
        v
thunk_entry
        |
        v
context not found
        |
        v
fallback_original_entry
        |
        v
original virtual method
```

This allows one object to have a stateful hook while all other objects of the same class continue using the original method.

### Example

```cpp
Calculator hooked_calculator;
Calculator unhooked_calculator;

hook.install(
    table,
    1,
    hooked_calculator,
    lambda
);
```

The behavior is:

```text
hooked_calculator -> context found -> lambda
unhooked_calculator -> context not found -> original Calculator::add
```

### Fallback limitation

The fallback stores one original entry per hook signature. Therefore, this sequence:

```cpp
hook_a.install(table_a, 1, object_a, lambda_a);
hook_b.install(table_b, 1, object_b, lambda_b);
```

overwrites the fallback created by `hook_a` if both hooks use the same signature.

For the common case of one target class, one vtable, and one slot, this is sufficient. Supporting several simultaneous fallbacks requires a per-object vtable copy or a registry keyed by `(vtable address, slot index)`.

## Calling the original

The original entry is stored in the object-specific context:

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

Both objects now call `thunk_entry`. For `a`, the thunk finds a context and calls the lambda. For `b`, no context exists, so the thunk calls the fallback original entry.

### One active fallback per signature

The current fallback stores one original entry per `stateful_slot_hook<Signature>` instantiation. Installing another hook with the same signature for a different vtable or slot overwrites the previous fallback.

### Object lifetime

A context is tied to the address of an object. Therefore, before destroying an object, call:

```cpp
hook.uninstall();
```

Otherwise, a dangling pointer remains in the registry. Reuse of the same memory address can then lead to undefined behavior.

### Thread safety

The registry is protected by a `std::mutex`, but this does not make the entire hook fully thread-safe:

-