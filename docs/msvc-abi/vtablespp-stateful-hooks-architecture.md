# vtablespp: Stateful Vtable Hooks

## Scope and status

This document explains the Windows x64 stateful slot-hook implementation discussed in the development conversation: a generated trampoline, a registry keyed by `(vtable address, slot index)`, and context recovery using `_AddressOfReturnAddress()` plus 0x28 bytes. It documents the latest code shown in that conversation, including the subsequent replacement of `std::unordered_map` with `std::map`.

The uploaded repository snapshot contains an older implementation that dispatches through an object registry. It is useful for understanding supporting components, but is **not** the source of truth for the latest generated trampoline. Supporting component descriptions below refer to that snapshot. [file:6]

The developer reported that all 38 tests passed after the changes. This is a reported test result, not an independent verification or a guarantee of ABI completeness.

**Important correction to earlier advice:** the implementation is not yet a general-purpose, production-safe hook engine. The current trampoline has concrete limitations concerning stack arguments, exception unwinding, shared-vtable semantics, and concurrent removal. These are correctness issues, not merely optional optimizations.

## Mental model

A vtable hook replaces one function pointer in a virtual function table. Calls that actually load that entry are redirected to a replacement entry point. It does not modify the original function's machine code.

The latest implementation follows this path:

```text
Caller
  -> selected vtable entry
  -> generated trampoline
  -> invoke(object, arguments...)
  -> stored callback(context, object, arguments...)
  -> optional call to saved original entry
  -> return through invoke and trampoline
```

The implementation is ABI-specific. C++ does not provide a portable facility for indexing, modifying, or invoking raw vtable entries.

## Main components

| Component | Responsibility |
|---|---|
| `vtable_view` | Non-owning view of table entries and an explicit or inferred entry count |
| `call_slot<Signature>` | Loads a raw entry and calls it using the supplied signature |
| `stateful_slot_hook<Signature>` | Owns callback state, installation metadata, and generated code |
| `slot_context<Signature>` | Gives the callback access to the saved original entry |
| `context_storage` | Holds the original entry, trampoline address, and erased callback |
| Slot registry | Detects conflicting installations within its registry scope |

In the repository snapshot, `vtable_view` does not own the object or table, and its explicit size is supplied by the caller. `call_slot` casts an entry to a function pointer matching the requested signature; it does not discover that signature. [file:6]

## Installation lifecycle

The latest installation code performs these operations:

1. Reject installation if the hook instance already reports itself installed.
2. Read the requested table entry and reject an absent or null entry.
3. Reject an entry matching the known pure-call handler.
4. Check whether the registry already contains the table/slot key.
5. Allocate callback storage and save the current entry as the original.
6. Allocate writable memory for a trampoline and emit machine code.
7. Change the trampoline memory to executable/read-only and flush the instruction cache.
8. Write the trampoline address into the selected vtable entry.
9. Insert the registry entry and save installation metadata in the hook object.

The intended uniqueness rule is one active hook per physical table entry. Different slot indices may be hooked independently.

**The current code does not make this sequence transactional.** The registry check, table write, and registry insertion occur in separate critical sections. Concurrent installers can both pass the check. Registry insertion can also allocate and throw after the entry has already been patched. Installation needs reservation, rollback, and a single coordinated ownership protocol.

## Callback type erasure

The callback is stored behind `callable_base`, whose virtual `call` method has the hook's declared signature. `callable_impl<F>` stores the concrete callable and forwards calls to it.

This supports captured lambda state without putting the lambda's concrete type in the public hook type. However, the shown `callable_impl(F&&)` constructor does not accept every lvalue callback passed through `install`: after `std::decay_t`, an lvalue may no longer bind to that constructor. A forwarding constructor or an appropriate by-value constructor is needed for a complete callable interface.

Callback state is shared by calls through that hook. If multiple threads enter concurrently, the callback must synchronize its own mutable state.

## Trampoline instructions

The latest emitted sequence is conceptually:

```asm
mov  rax, context_address
mov  r11, dispatcher_address
sub  rsp, 28h
mov  [rsp + 20h], rax
call r11
add  rsp, 28h
ret
```

The trampoline leaves the ordinary integer/pointer argument registers unchanged. For simple signatures matching the assumptions below, `invoke` sees the same object pointer and register arguments.

`rax` carries the context temporarily; the context is then stored in a dedicated stack location before the call. `r11` carries the dispatcher address. This is generated Windows x64 code, not portable C++.

## Exact stack layout

Let `S` be `rsp` at trampoline entry, after the original caller has called the vtable entry.

```text
At trampoline entry:
  [S + 00h]  return address to the original caller
  [S + 08h]  caller-provided home space begins
  [S + 28h]  original fifth ABI argument, if present

After sub rsp, 28h:
  rsp = S - 28h
  context is written at [rsp + 20h] = [S - 08h]

At invoke entry, after call r11:
  rsp = S - 30h
  [rsp + 00h] return address inside the trampoline
  [rsp + 08h ... rsp + 27h] invoke home space
  [rsp + 28h] context pointer
```

Thus, for the emitted sequence, context is 0x28 **bytes** above the dispatcher's return-address slot. On x64, `return_address_slot + 5` advances five 8-byte pointers, or 40 bytes.

The current dispatcher uses this relationship:

```cpp
void** ra_slot = reinterpret_cast<void**>(_AddressOfReturnAddress());
void* raw_context = nullptr;
std::memcpy(&raw_context, ra_slot + 5, sizeof(raw_context));
```

This is a private trampoline/dispatcher convention. It is not a standard C++ parameter-passing mechanism, and the compiler has not been told that this location is reserved metadata.

## Why stack arguments break

The context location is also the standard position of the dispatcher's fifth ABI argument. These uses conflict if the dispatcher needs a real argument there.

For example:

```cpp
virtual int calculate(int a, int b, int c, int d);
```

With an ordinary scalar return, `this` is ABI argument one, and the four explicit integers are arguments two through five. The fourth explicit integer therefore needs a stack argument slot.

The trampoline creates a new call frame but does not copy incoming stack arguments into the new outgoing argument area. Instead, it writes its context into the first such location. `invoke` consequently does not receive the original fourth integer correctly.

**Current practical restriction:** ordinary scalar/void-return signatures with no more than three explicit arguments, and only after validating their exact ABI behavior. This is not a guarantee for arbitrary types fitting that argument count.

Aggregate returns may introduce hidden return-storage arguments. Aggregates passed by value, vector types, special calling conventions, and variadic functions require separate ABI analysis. Do not infer support from template instantiation or a successful simple test.

## Calling the original

`slot_context` stores both the saved original entry and the object pointer received by the dispatcher.

```cpp
return context(args...);                       // use stored incoming object
return context.call_original_raw(object, args...); // use explicit object
```

Calling the saved entry bypasses the patched vtable slot. Calling the same virtual slot again instead can re-enter the hook recursively.

The saved entry is whatever was present during installation. It is not necessarily the ultimate method body: it can be an adjustor thunk or a pre-existing replacement installed outside this registry. The code does not implement managed hook chaining.

## Multiple inheritance

A secondary base subobject can have an address different from the complete object. Calls through its vtable pass the secondary subobject address as `this`.

A saved secondary entry may be an adjustor thunk that transforms that address before transferring control to the implementation. Calling the saved entry with the original incoming subobject address preserves that adjustment. Calling a resolved final method directly requires separately supplying the correct adjusted object address.

Context recovery through the trampoline does not depend on comparing the incoming object pointer with the complete object's address. That avoids the specific secondary-base lookup failure caused by the earlier object-only registry.

Do not generalize that slot zero is always a destructor. Actual slot numbering depends on the class and compiler ABI. Inspect the concrete layout; calling a deleting destructor as `int(void*)` is an invalid call and may damage the object.

## Shared-vtable semantics

**The latest implementation patches a shared table, not an individual object's private table.** Every object using that table entry can reach the same trampoline and callback.

Although `install` receives an object reference and stores `object_`, the shown `invoke` does not use it to filter calls. Therefore, this version does not provide per-object callback selection or an automatic original-call fallback for other objects sharing the table.

A truly per-object interface needs an explicit design, such as a private cloned vtable with correctly preserved ABI metadata, or dispatch-time filtering by the correct base-subobject address. Neither should be assumed to exist in the current code.

## Registry boundaries

The shown registry is static inside `stateful_slot_hook<Return(Args...)>`. Consequently, different signature specializations have different registries and mutexes.

The same physical table slot can therefore evade conflict detection if installed through a different signature specialization. A process-wide ownership registry shared across signatures is needed to enforce physical-slot uniqueness. Separately built modules can require additional coordination as well.

Using `std::map` avoids needing a custom hash for the pair key. Container choice does not fix transactionality, lifetime management, or cross-specialization ownership.

## Concurrency and lifetime

`installed_` being atomic does not make the whole class thread-safe. Installation, removal, moves, metadata access, and active dispatch are not one synchronized protocol.

The most dangerous removal sequence is:

```text
Thread A enters or is about to enter the trampoline.
Thread B restores the table slot and frees the trampoline and context.
Thread A executes freed code, returns into freed code, or reads freed state.
```

A `shared_ptr` member does not solve this: the generated code embeds a raw context pointer, and active calls do not acquire ownership. Self-uninstall from inside a callback is also unsafe because the callback must eventually return through the trampoline.

For now, externally serialize installation and removal, and ensure no thread is executing or can enter the affected call path during removal. A robust solution needs a retirement/quiescence protocol; merely counting callbacks after dispatcher entry does not cover callers already holding an old entry pointer.

## Removal failures

The current `uninstall` clears the installed flag and registry state before verifying restoration, ignores the `write_memory` result, and frees resources unconditionally.

If restoration fails, the vtable can still point to freed executable memory. This is a critical correctness bug. A failed restore must retain the live resources and ownership state and report failure through a defined policy.

Removal should also verify that the slot still contains this hook's replacement. Blind restoration can overwrite a later replacement installed by another system.

## Exception unwinding

The generated trampoline adjusts `rsp` and calls another function. It is not a leaf function, but the shown implementation does not register Windows x64 unwind metadata for it.

An exception escaping the callback must unwind across this generated frame. Without appropriate function-table and unwind information, that path is not reliably supported. One passing exception test does not establish correctness.

Treat exception escape across the trampoline as unsupported until proper unwind metadata is generated and registered, for example through the Windows dynamic function-table facilities. This also matters for stack walking and diagnostics.

Catching exceptions inside the dispatcher and applying an explicit non-throwing policy can avoid some escape paths, but changes API behavior and is not a substitute for complete unwind support.

## Platform and execution policy

The emitted bytes target Windows x64 with the expected Microsoft calling convention. They do not support x86, ARM64, System V AMD64, or arbitrary calling conventions.

Executable-memory policies may reject dynamic code allocation or executable protection changes. Control-flow protections and toolchain instrumentation require explicit compatibility testing; successful `VirtualProtect` alone is not proof of compatibility with every protected process configuration.

Add compile-time platform guards so unsupported targets fail explicitly rather than generating invalid code.

## Dispatch and optimization

A slot hook only intercepts calls that actually pass through that slot. Devirtualized, inlined, explicitly qualified, or otherwise direct calls can bypass it.

The repository's `call_slot` helper explicitly loads an entry, which makes it useful for targeted tests. Its supplied signature and object pointer must still match the actual entry ABI. [file:6]

Passing a debug test therefore does not establish that every source-level virtual call in an optimized application will be intercepted.

## Inspection limitations

The snapshot's automatic vtable sizing scans entries while they appear to point to executable code. It has no authoritative table-length field and should be treated as a heuristic; an explicit, validated size is preferable for mutation-sensitive operations. [file:6]

The slot classifier recognizes selected instruction patterns, not arbitrary machine-code semantics. In the supplied implementation, an unrecognized readable entry can be labeled `direct` with no supporting evidence; it does **not** always become `unknown`. Classification is diagnostic evidence, not proof that a function can safely be invoked with a chosen signature. [file:6]

RTTI parsing is tied to the implemented MSVC layout, and symbol lookup relies on DbgHelp and available symbol data. Missing PDB information does not guarantee a mangled name will still be available; symbol resolution can fail entirely. [file:6]

## Recommended support contract

Until the correctness issues above are fixed, document this as an experimental implementation with these preconditions:

- Windows x64 with a validated Microsoft-compatible compiler configuration.
- Explicitly verified slot indices and signatures.
- Simple register-argument signatures; no stack-argument support claim.
- No unvalidated aggregate-return, variadic, or special-calling-convention signatures.
- Shared-vtable interception, not per-object isolation.
- Externally serialized installation and removal.
- No active or pending calls during removal.
- No callback-triggered uninstall or destruction of the active hook.
- No exception escaping across the generated trampoline.
- No competing hook system modifying the same entry.
- Object, module, original entry, and callback dependencies remain valid for the relevant calls.

These are operational restrictions, not a proof that every remaining ABI corner case has been eliminated.

## Hardening priorities

1. Fix restoration failure handling and preserve resources on failure.
2. Make registry ownership global across signatures and installation transactional.
3. Implement safe trampoline/context retirement under concurrent calls.
4. Register valid unwind information before claiming exception propagation support.
5. Replace the ad hoc context slot with a deliberate signature-aware bridge, including stack-argument forwarding and hidden arguments.
6. Define and test shared-table versus per-object semantics explicitly.
7. Add compile-time signature/platform restrictions for unsupported cases.
8. Validate optimized builds and protected-process/toolchain configurations.

Possible bridge designs include compiler-generated indexed dispatch stubs with a bounded registry, or a dedicated ABI-specific assembly bridge. Neither is automatically correct: the design must preserve argument locations, return conventions, unwindability, and lifetime guarantees.

## Test expansion

Add focused tests rather than relying only on the current passing suite:

- Zero through six explicit integer/reference arguments, exposing the first stack-argument boundary.
- Mixed integer and floating-point argument positions.
- Scalar, void, reference, and separately validated aggregate returns.
- Two instances sharing a table, with asserted callback-selection semantics.
- Multiple independent slots with identical signatures.
- Conflicting installations through different template signatures.
- Failure injection for allocation, registry insertion, patching, and restoration.
- Concurrent installation attempts and controlled retirement stress tests.
- Callback reentrancy and rejection or safe handling of self-uninstall.
- Unwinding across generated code after metadata support is implemented.
- Debug and Release builds, with optimization and link-time optimization settings varied.
- Secondary-base calls with nonzero subobject offsets.

Use isolated processes for tests that intentionally exercise unsupported ABI behavior or freed-code hazards.

## Reading the passing tests

The reported 38 passing tests demonstrate that the tested scenarios completed successfully in the tested build. They do not prove arbitrary signature support, safe concurrent teardown, correct unwinding, or per-object isolation.

The implementation's useful core idea is explicit: save a slot, redirect it through an owned callback context, and preserve the original entry for optional delegation. The remaining engineering work is to make that mechanism's ABI and lifetime contract precise and enforceable.
