# Comprehensive Guide to This Adjustment Thunks in C++ (MSVC x64)

## 1. Introduction: What is a This Adjustment Thunk?
A **This Adjustment Thunk** is a hidden, compiler-generated helper function. 

While a *Return Adjustment Thunk* modifies the **output** (return value) of a function, a *This Adjustment Thunk* modifies the **input** (the hidden `this` pointer passed in the `RCX` register).

It is automatically created by the compiler under multiple or virtual inheritance when:
1. A virtual method is invoked through a secondary base interface pointer (e.g., `Right*`).
2. The actual implementation belongs to a derived class (e.g., `Derived`), which expects `RCX` to point to the very beginning of the full `Derived` object.

---

## 2. Why the Adjustment Cannot Happen on the Caller Side

Just like with return thunks, a common design question arises: *If the compiler knows we are calling a method on a `Right*` pointer, why doesn't the caller just subtract 8 bytes from the pointer before jumping to the function?*

The answer is exactly the same: **Polymorphism**.

### The Dynamic Inbound Dilemma
Consider a generic function that triggers an operation on any object implementing the `Right` interface:

```cpp
void execute(Right* r) {
    r->left(); // The compiler generates a vftable call here!
}
```

When compiling `execute`, the compiler has no idea what `r` actually points to at runtime. The layout depends entirely on the concrete type:
1. If `r` points to a pure **`Right`** object: The method expects the original `Right*` address. No shift needed.
2. If `r` points to a **`Derived`** object: `Right` sits at offset `+8`. To execute `Derived::left()`, the `this` pointer must be shifted back by **`-8`** bytes so the method can access `Derived`'s own data fields safely.
3. If `r` points to **`AnotherDerived`**: `Right` might sit at offset `+16`. It needs a shift of **`-16`** bytes.

Because the caller code inside `execute` is shared and uniform, it cannot hardcode a specific pointer subtraction. Instead, it delegates the responsibility to the virtual table. The vftable slot for `Right` in `Derived` points to a **This Adjustment Thunk**, which knows exactly how many bytes to subtract before passing control to the real method.

---

## 3. Scenario A: Static Offset (Multiple Inheritance)

### The Anatomy of the Memory Layout
```cpp
struct Derived : Left, Right {
    void left() const override { ... } // Re-implemented here!
};
```
* `Left` resides at offset `+0` (Primary vftable).
* `Right` resides at offset `+8` (Secondary vftable).

When `execute(Right* r)` calls `r->left()`, it fetches the vftable from the `Right` sub-object and calls the slot. That slot contains the following static thunk:

### Assembly Breakdown (Fixed -8 Shift)
```assembly
virtual void Derived::left(void)const PROC
        ; [Bytecode: 48 83 E9 08] -> sub rcx, 8
        ; THE THIS-ADJUSTMENT OCCURS HERE:
        ; RCX originally holds the 'Right*' address. 
        ; By subtracting 8, RCX now points to the start of the 'Derived' object.
        sub     rcx, 8                             

        ; [Bytecode: E9 XX XX XX XX] -> jmp Derived::left
        ; Direct jump to the actual implementation. 
        ; Since it's a jump (jmp) and not a call, no new stack frame is created here, 
        ; and 'ret' inside Derived::left will return directly to the original caller.
        jmp     virtual void Derived::left(void)const             
virtual void Derived::left(void)const ENDP
```

*Note: Static "this" thunks are incredibly efficient. They typically consist of just two instructions (`sub` and `jmp`), meaning they don't even need to allocate a stack frame.*

---

## 4. Scenario B: Dynamic Offset (Virtual Inheritance)

### The Dynamic Shift Challenge
Under virtual inheritance (`struct Derived : virtual Right`), the base interface position is dynamic and determined at runtime via the `vbtable`. 

If a virtual method is called via a virtual base pointer, a hardcoded `sub rcx, 8` is impossible. The thunk must read the `vbtable` to compute the subtraction.

### Assembly Breakdown (Dynamic Lookup)
```assembly
v_inheritance_this_thunk.exe!Derived::left(void):
        ; 1. Read the vbtable to find the runtime offset
        mov     rax, qword ptr [rcx]       ; RCX is Right*. Read its vptr/vbptr.
        movsxd  rax, dword ptr [rax+4]     ; Fetch Index 1 from vbtable (holds offset to base)
        
        ; 2. Adjust RCX dynamically
        ; Since 'rax' contains how far Right is from Derived (a positive number),
        ; subtracting this offset moves RCX backward to the start of Derived.
        sub     rcx, rax                   
        
        ; 3. Jump to the real method
        jmp     Derived::left              
```

---

## 5. Visual Summary: Return vs. This Thunk Comparison

| Feature | Return Adjustment Thunk | This Adjustment Thunk |
| :--- | :--- | :--- |
| **Primary Target** | Modifies the **Output** (`RAX` register) | Modifies the **Input** (`RCX` register) |
| **Trigger Event** | Covariant return types (`Derived*` instead of `Base*`) | Invoking a method via a non-primary base pointer (`Right*`) |
| **Execution Order** | Executes **after** the real method finishes | Executes **before** the real method begins |
| **Control Flow** | Uses `call` to wrap the original function | Uses `jmp` to pass control forward seamlessly |

---

## 6. Conclusion
* **Static "This" Thunk:** Subtracts a fixed byte size (`sub rcx, X`) and performs a rapid `jmp` to the concrete method.
* **Dynamic "This" Thunk:** Extracts the runtime position from the `vbtable` dynamically before adjusting `rcx` and jumping.
