# vtablespp: Generalized Architecture Overview

## Purpose

`vtablespp` is a Windows x64 library for inspecting and hooking C++ virtual function tables (vtables). It provides:

- Reading and classifying vtable slots
- Calling a specific vtable slot with an explicit signature
- Installing a stateful hook on one vtable slot
- Calling the original implementation from inside a hook
- Resolving RTTI and symbol information for diagnostics

The implementation is intentionally low-level. It treats vtables as raw arrays of function pointers and uses generated machine code to preserve the original calling convention while adding hook behavior.

This document generalizes the architecture discussed during development: vtable views, slot classification, `call_slot`, stateful hooks, generated trampolines, context recovery, callbacks, and known limitations.

## Core idea

A C++ virtual call normally follows this path:

```text
object.method(arguments)
  -> object's vtable
  -> selected slot
  -> virtual function implementation
```

A hook changes one entry in that path:

```text
object.method(arguments)
  -> object's vtable
  -> selected slot
  -> trampoline
  -> invoke(object, arguments...)
  -> user callback
  -> optional original implementation
  -> return
```

The hook does not modify the original function's machine code. It replaces one function pointer in the vtable.

## Main components

| Component | Responsibility |
|---|---|
| `vtable_view` | Non-owning view over vtable entries |
| `call_slot<Signature>` | Loads and calls one raw vtable entry |
| `slot_classifier` | Describes what kind of code a slot contains |
| `rtti_reader` | Reads MSVC RTTI metadata from a vtable |
| `symbol_resolver` | Resolves addresses to symbol information |
| `slot_hook` | Stateless replacement of a vtable entry |
| `stateful_slot_hook` | Stateful hook with a user callback and saved original entry |
| `slot_context` | Callback-facing view of the original entry |
| `context_storage` | Runtime state for one installed hook |
| Trampoline | Generated executable stub placed into a vtable slot |
| `invoke` | C++ dispatcher called by the trampoline |

## Vtable model

A vtable is an array of function pointers. Each polymorphic object contains a pointer to its primary vtable. With multiple inheritance, an object can have several vtables: one for the primary base and additional tables for secondary bases.

```text
MultiDerived object
  +----------------+
  | primary vtable | -> Left-related entries
  +----------------+
  | secondary vtable | -> Right-related entries
  +----------------+
```

`vtable_view::from_object` obtains the primary table. `vtable_view::from_base<Right>` obtains the table associated with the `Right` base subobject.

The library does not assume that slot 0 is always a destructor or that slot 1 is always the first user-defined virtual method. Actual slot numbering depends on the class declaration, compiler, and ABI.

## Calling a slot

`call_slot<Signature>` is the direct-call utility:

```cpp
const int result = vtablespp::call_slot<int(int, int)>(
    calculator,
    1,
    a,
    b
);
```

It performs these steps:

1. Obtains the object's vtable address.
2. Reads the requested slot.
3. Rejects a null entry.
4. Rejects an entry matching the known pure-call handler.
5. Casts the raw entry to the requested function-pointer signature.
6. Calls it with the supplied object and arguments.

The signature must match the actual entry ABI. The library cannot infer it from the slot alone.

```cpp
using function_type = Return (*)(void*, Args...);
```

The first parameter is the object pointer. Subsequent parameters are the virtual function's arguments.

## Slot classification

`slot_classifier::inspect(index)` returns a `slot_info` describing a vtable entry.

Possible classifications include:

| Kind | Meaning |
|---|---|
| `direct` | Readable code that does not match a known thunk pattern |
| `this_adjustor` | Adjusts `this` before transferring control |
| `return_adjustor` | Adjusts a returned pointer or reference |
| `purecall` | The compiler's pure-virtual-call handler |
| `unknown` | Null, out of range, or unrecognized |

Classification is diagnostic evidence, not proof that a function can safely be invoked with an arbitrary signature. A readable code entry may be reported as `direct` even when its exact ABI has not been verified.

### Thunks

In multiple inheritance, a secondary vtable entry may be an adjustor thunk. It receives the secondary subobject address, adjusts it, and transfers control to the final implementation.

```text
Right subobject address
  -> this-adjustor thunk
  -> adjusted complete-object address
  -> MultiDerived::method
```

The classifier attempts to recognize common adjustment patterns and resolve the final target. It does not guarantee recognition of every compiler-generated pattern.

## Stateful hooks

A stateful hook stores:

- The original vtable entry
- The user callback
- The generated trampoline address
- Installation metadata

The public callback type is:

```cpp
Return callback(slot_context<Return(Args...)>& context, void* object, Args... args)
```

The callback can:

- Inspect arguments
- Modify arguments
- Call the original implementation
- Skip the original implementation
- Return a replacement result

Example:

```cpp
stateful_slot_hook<int(int, int)> hook;

hook.install(
    table,
    1,
    calculator,
    [](auto& context, void* object, int& a, int& b) {
        return context.call_original_raw(object, a, b) * 10;
    }
);
```

## Installation flow

Installation performs the following conceptual steps:

```text
1. Validate slot and entry
2. Reject pure-call entries
3. Check registry for conflicting installation
4. Save original entry
5. Create context storage
6. Store user callback
7. Generate trampoline containing context address
8. Patch vtable slot with trampoline address
9. Record installation
```

The registry is keyed by the physical vtable address and slot index:

```text
(vtable_address, slot_index)
```

This allows independent hooks on different slots while preventing two hooks from owning the same physical slot.

## Context and callback

The context is the bridge between low-level generated code and user-level C++ logic.

```cpp
struct context_storage {
    void* original_entry;
    void* trampoline;
    std::unique_ptr<callable_base> hook;
};
```

The callback is type-erased behind `callable_base`. This allows lambdas with captured state without exposing the concrete lambda type through the hook template.

The callback receives:

| Parameter | Meaning |
|---|---|
| `context` | Access to the saved original entry |
| `object` | The `this` pointer passed through the hooked slot |
| `args...` | The original virtual function's arguments |

The object may be a secondary-base subobject address. It is not necessarily the address of the complete derived object.

## Calling the original

The callback can delegate to the saved original entry:

```cpp
return context.call_original_raw(object, a, b);
```

This calls the entry saved during installation. It bypasses the patched vtable slot, so it does not recursively enter the same hook.

Calling the virtual method normally, such as `object.add(a, b)`, goes through the vtable again and re-enters the hook.

The saved original entry is not necessarily the ultimate method body. It may be an adjustor thunk or a replacement installed by another system.

## Trampoline

A trampoline is a small block of generated executable code placed into a vtable slot. It preserves the normal calling convention while carrying hook context into the C++ dispatcher.

Conceptually, the latest trampoline performs:

```asm
mov  rax, context_address
mov  r11, dispatcher_address
sub  rsp, 28h
mov  [rsp + 20h], rax
call r11
add  rsp, 28h
ret
```

It does not contain user logic. It is a low-level bridge from the vtable into `invoke`.

### Why it exists

A vtable entry must retain the original virtual function's ABI:

```cpp
Return (*)(void* object, Args... args)
```

But `invoke` needs the hook context as well. The trampoline resolves this mismatch:

```text
External ABI:  invoke(object, args...)
Internal need: invoke(context, object, args...)
```

It preserves the external ABI and smuggles the context through a reserved stack location.

## How context reaches invoke

The object is passed normally through the first argument register:

```text
rcx = object
```

The context travels through a different path:

```text
trampoline machine code
  -> trampoline stack
  -> invoke stack
  -> context recovery
```

At installation, the context address is embedded in the trampoline's machine code:

```asm
mov rax, context_address
```

Before calling `invoke`, the trampoline writes it to a known stack location:

```asm
sub rsp, 28h
mov [rsp + 20h], rax
call r11
```

At `invoke` entry:

```text
[rsp + 00h]  return address into the trampoline
[rsp + 08h]  invoke home space
[rsp + 28h]  context pointer
```

`invoke` recovers it using:

```cpp
void** ra_slot = reinterpret_cast<void**>(_AddressOfReturnAddress());

void* raw_context = nullptr;
std::memcpy(&raw_context, ra_slot + 5, sizeof(raw_context));
```

On x64, `ra_slot + 5` advances five 8-byte pointers, or 40 bytes (`0x28`).

## invoke

`invoke` is the C++ dispatcher called by the trampoline:

```cpp
static Return invoke(void* object, Args... args)
```

It receives the object and arguments in the same way as the original virtual function. It then:

1. Obtains its return-address slot.
2. Reads the context pointer from the stack.
3. Interprets it as `context_storage*`.
4. Builds a `slot_context` around the saved original entry.
5. Calls the user callback.
6. Returns the callback result.

```text
object:       normal ABI argument
context:      recovered from trampoline-reserved stack location
callback:     user-defined hook behavior
```

## Uninstallation

Uninstallation restores the original vtable entry, removes the registry record, frees the trampoline, and destroys callback storage.

```text
1. Restore original vtable entry
2. Remove registry entry
3. Free trampoline
4. Destroy context and callback
```

The hook object's destructor calls `uninstall`, providing basic RAII behavior.

## Multiple inheritance

A secondary base can have a different address from the complete object:

```cpp
MultiDerived object;
Right& right_ref = object;
```

Calls through `right_ref` pass the `Right` subobject address as `this`. The saved original entry may be an adjustor thunk that transforms that address before reaching the final implementation.

Calling the saved entry with the original incoming subobject address preserves that adjustment:

```cpp
context.call_original_raw(object, args...);
```

Calling a resolved final method directly would require supplying the correctly adjusted object address separately.

## Shared-vtable semantics

The current design patches a shared vtable, not a private per-object table. Every object using that table entry can reach the same trampoline and callback.

Although `install` receives an object reference, the dispatcher does not use it to filter calls. Therefore, this is shared-table interception, not per-object isolation.

A per-object interface would require an explicit design, such as a private cloned vtable with correctly preserved ABI metadata, or dispatch-time filtering by the correct base-subobject address.

## Registry boundaries

The registry is static inside `stateful_slot_hook<Return(Args...)>`. Different signature specializations therefore have different registries and mutexes.

The same physical table slot can evade conflict detection if installed through a different signature specialization. A process-wide ownership registry shared across signatures is needed to enforce physical-slot uniqueness.

Using `std::map` avoids needing a custom hash for the pair key. Container choice does not fix transactionality, lifetime management, or cross-specialization ownership.

## Known limitations

### Signature support

The trampoline preserves register arguments but does not forward incoming stack arguments into the new call frame. The context is written into the dispatcher's fifth argument stack position.

This creates a conflict for signatures that genuinely need a stack argument:

```cpp
virtual int calculate(int a, int b, int c, int d);
```

With a scalar return, `this` is argument one and the four explicit integers are arguments two through five. The fourth explicit integer normally uses a stack-argument slot, which the trampoline overwrites with its context.

Current practical support is therefore limited to simple signatures whose arguments fit in the register argument area and whose return convention does not introduce a hidden argument. Aggregate returns, by-value aggregates, vector types, special calling conventions, and variadic functions require separate ABI analysis.

### Platform support

The generated trampoline targets Windows x64 and the Microsoft calling convention. It does not support x86, ARM64, System V AMD64, or arbitrary calling conventions without a separate implementation.

### Exception unwinding

The generated trampoline adjusts the stack and calls another function. The shown implementation does not register Windows x64 unwind metadata for it.

An exception escaping the callback must unwind across this generated frame. Without appropriate function-table and unwind information, that path is not reliably supported.

### Concurrency and lifetime

Installation, removal, moves, metadata access, and active dispatch are not one synchronized protocol. The atomic installed flag does not make the whole class thread-safe.

The dangerous removal sequence is:

```text
Thread A enters or is about to enter the trampoline.
Thread B restores the table slot and frees the trampoline and context.
Thread A executes freed code or reads freed state.
```

A callback must not uninstall or destroy the hook that invoked it. The callback must eventually return through the trampoline.

### Removal failures

If restoration of the original vtable entry fails, the current code may still free the trampoline and context. That can leave the vtable pointing to freed executable memory.

A failed restore must retain live resources and report failure through a defined policy.

### Hook chaining

The implementation does not support managed chains of multiple hooks on the same slot. `call_original_raw` calls the entry saved during installation, bypassing later replacements.

### Inspection limits

Automatic vtable sizing is heuristic: it scans entries while they appear to point to executable code. It has no authoritative table-length field.

The slot classifier recognizes selected instruction patterns, not arbitrary machine-code semantics. RTTI parsing is tied to the implemented MSVC layout, and symbol lookup relies on DbgHelp and available symbol data.

## Hardening priorities

1. Fix restoration failure handling and preserve resources on failure.
2. Make registry ownership global across signatures and installation transactional.
3. Implement safe trampoline/context retirement under concurrent calls.
4. Register valid unwind information before claiming exception propagation support.
5. Replace the ad hoc context slot with a deliberate signature-aware bridge, including stack-argument forwarding and hidden arguments.
6. Define and test shared-table versus per-object semantics explicitly.
7. Add compile-time signature/platform restrictions for unsupported cases.
8. Validate optimized builds and protected-process/toolchain configurations.

## Test coverage

The reported 38 passing tests demonstrate that the tested scenarios completed successfully in the tested build. They do not prove arbitrary signature support, safe concurrent teardown, correct unwinding, or per-object isolation.

Useful additional tests include:

- Zero through six explicit integer/reference arguments
- Mixed integer and floating-point argument positions
- Scalar, void, reference, and separately validated aggregate returns
- Two instances sharing a table, with asserted callback-selection semantics
- Multiple independent slots with identical signatures
- Conflicting installations through different template signatures
- Failure injection for allocation, registry insertion, patching, and restoration
- Concurrent installation attempts and controlled retirement stress tests
- Callback reentrancy and rejection or safe handling of self-uninstall
- Unwinding across generated code after metadata support is implemented
- Debug and Release builds, with optimization and link-time optimization settings varied
- Secondary-base calls with nonzero subobject offsets

## One-paragraph summary

`vtablespp` treats a C++ vtable as a raw array of function pointers. `call_slot` invokes one entry with an explicitly supplied ABI signature. A stateful hook saves the original entry, stores a user callback, generates a small x64 trampoline that carries the hook context, and replaces one vtable slot with that trampoline. When the virtual function is called, the trampoline preserves the normal calling convention, passes the object and arguments to `invoke`, and smuggles the context through a reserved stack location. `invoke` recovers the context, runs the callback, and optionally delegates to the saved original entry. The design is powerful for targeted Windows x64 vtable interception, but its current form has important limitations around stack arguments, shared vtables, exception unwinding, concurrent teardown, and cross-signature registry ownership.
