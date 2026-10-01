---
description: >-
    Use when writing or editing C or C++ code in library. Covers file headers,
    section banners, header guards, include order, Doxygen, naming, encoding,
    and MISRA-driven rules (single entry/exit, declarations at block top).
applyTo: "**/*.{c,cc,cpp,cxx,h,hh,hpp,hxx}"
---

# C/C++ Style

Applies to C and C++ sources. Rules tagged **[MISRA]**
exist to satisfy static analysis (SonarQube, MISRA profile) — keep them even when
they differ from idiomatic modern C++.
Keep lines within 80 characters where practical. Lines up to 85 characters are
permitted when splitting them would reduce clarity.

Do not exceed three nested levels of loops and conditional statements. Prefer
guard clauses, helper functions, or decomposition when another level would be
required.

## File header

Every `.c`/`.h`/`.cpp`/`.hpp` starts with this block. Use ISO 8601 dates; a
file-level `@brief` may go inside.

```c
/**
 * @file    filename.c
 * @version 1.0.0
 * @authors <author>
 * @date    YYYY-MM-DD
 * @date    @showdate "%Y-%m-%d"
 */
```

The file header may be extended with project-required metadata, such as a
copyright notice. Keep the required fields above and preserve the established
format used by neighboring files. For example:

```c
/**
 * @copyright @showdate "%Y " Copyright holder. All rights reserved.
 * @file    filename.c
 * @version 1.0.0
 * @authors <author>
 * @date    YYYY-MM-DD
 * @date    @showdate "%Y-%m-%d"
 */
```

## Section separators

80-char banners divide a file into sections — copy verbatim:

```c
/******************************** Included files ******************************/
/********************************* Definitions ********************************/
/****************************** Module variables ******************************/
/***************************** Private prototypes *****************************/
/****************************** Private functions *****************************/
/********************** Application Programming Interface *********************/
/****************************** Class declaration *****************************/
/******************************* Class definition *****************************/
/******************************************************************************/
```

Between functions use the thin separator:

```c
/*----------------------------------------------------------------------------*/
```

## Header guard

Use the `// !<MACRO>` style on `#endif` (not `/* NAME */`):

```c
#ifndef FILENAME_H
#define FILENAME_H

/* ... */

#endif // !FILENAME_H
```

## Include order

System / standard library headers first (angle-bracket form), then project
headers (quote form). Blank line between unrelated clusters.

Prefer `<stdint.h>` over `<cstdint>`. Use fixed-width integer types such as
`uint16_t` and `uint8_t` without the `std::` prefix.

Do not use `using namespace std;` at global scope. Keep standard-library types
that are not fixed-width integer types explicitly qualified, for example
`std::string` and `std::vector`.

## Module structure

A module containing one implementation file and one matching header may keep
both files in its module directory. When a module has, or is expected to gain,
multiple implementation or header files, place headers in `inc/` and source
files in `src/` below that module directory.

## Macro documentation

Every public or non-obvious `#define` gets a Doxygen block; align related values:

```c
/**
 * @def IWDG_KEY_RELOAD
 * @brief Write to IWDG_KR to reload the counter.
 */
#define IWDG_KEY_RELOAD         0x0000AAAAU
#define IWDG_KEY_ENABLE         0x0000CCCCU
```

Include guards do not require Doxygen documentation. Macros generated and
maintained by external tools, such as the Visual Studio resource editor's
`_APS_*` values, retain the tool-generated form and documentation.

## Functions

Use a K&R brace, with the opening brace on the signature line. If a condition
does not fit on one line, put the first sub-expression on the next line, align
sub-expressions, and place the closing parenthesis with the opening brace on
their own line:

```c
AcroStatus_t acroAddTask(AcroTaskHandle_t xTask) {
    if (
        (id   != INVALID_ID) &&
        (pIdx != FREE_SLOT)
    ) {
        /* body */
    }
    else {
        /* body */
    }
}
```

For a constructor with a multiline member initializer list, the opening brace
may be placed on the next line to visually separate initialization from the
constructor body:

```cpp
DeviceManager::DeviceManager()
    : transport_(CreateTransport()),
      logger_(CreateLogger())
{
    /* body */
}
```

`else` always starts on a new line after the closing brace.

## Pointer declarators

In variable and parameter declarations, attach the `*` to the pointer name,
not to the pointed-to type:

```cpp
MpiApi *mpi_obj;
uint8_t const *data;
```

The following cases are exceptions:

- In a `const * const` declaration, put one space on each side of `*`:
    `uint8_t const * const data`.
- When a function returns a pointer, attach `*` to the return type:
    `char const* get_name(void)`.

This rule applies to pointer declarators. It does not change the formatting of
dereference or multiplication operators, or syntax-required function-pointer
declarations such as `uint64_t (*time_function)(void)`.

## Line length and nesting

Keep source lines within 80 characters where practical. Lines from 81 through
85 characters, inclusive, are an undesirable but permitted deviation. Lines
longer than 85 characters are not permitted. Split long expressions and
signatures according to the formatting rules above; do not add nesting only to
satisfy the line limit.

Do not exceed three nested levels of loops and conditional statements. Prefer
guard clauses, helper functions, or decomposition into smaller functions when
another level would be required.

## DRY and KISS

Follow **DRY** (Don't Repeat Yourself): keep each rule, algorithm, constant,
and piece of domain knowledge in one authoritative place. Extract shared code
when duplication represents the same behavior and is likely to change for the
same reason. Do not abstract code merely because it looks similar; independent
behaviors may remain separate when combining them would add coupling or
conditional complexity.

Follow **KISS** (Keep It Simple): choose the simplest design that clearly and
correctly satisfies the current requirements. Prefer direct control flow,
existing project patterns, and small focused functions. Do not add speculative
extension points, generic frameworks, layers, or configuration for requirements
that do not yet exist.

When DRY and KISS compete, remove meaningful duplication only when the shared
abstraction is simpler to understand and maintain than the duplicated code.

## Magic numbers

Do not use unexplained numeric literals in logic, conditions, sizes, offsets,
protocol fields, or bit masks. Replace them with a named constant or macro
whose name expresses the meaning and whose definition is placed near related
constants. Small universally obvious values such as `0`, `1`, and `-1` may be
used directly when their meaning is unambiguous in context. Document
non-obvious macros according to the macro documentation rules above.

## Declarations at block top [MISRA]

Declare local variables at the top of the smallest natural block in which they
are used. In a function with guard clauses, validate preconditions first, then
declare variables needed by the remaining body before its first statement.
This avoids constructing or declaring values on paths that return before using
them. Do not introduce an artificial scope solely to delay declarations, and do
not mix uninitialized declarations with statements in the same logical block.

A local variable may be declared later when it is initialized immediately by
the operation that produces its first meaningful value. This is permitted after
guard clauses or statements that prepare inputs for that operation, especially
when moving the declaration upward would require a dummy initialization or
would construct the variable on a path where it is not used.

```c
int foo(void) {
    uint32_t timeout;

    if (configuration == NULL) {
        return ERROR;
    }

    timeout = 100000UL;
    return runTask(configuration, timeout);
}
```

## Single entry / single exit [MISRA]

Prefer one `return` at the bottom. Use `ret_val` as the result accumulator; set
it on success and leave it at the error default otherwise.

Early returns are permitted for guard clauses that reject invalid parameters,
failed preconditions, or unrecoverable setup errors when a single-exit form
would exceed three nesting levels or obscure the main path. Keep guard clauses
at the start of the function and before acquiring resources that require
cleanup. After resource acquisition, use one cleanup path and one final return.

Priority when rules compete: preserve correctness and resource cleanup, keep
nesting at three levels or fewer, prefer a single exit, then target the
80-character line limit through formatting and decomposition. The permitted
81-to-85-character deviation does not justify additional nesting.

## Naming

| Category                    | Convention         | Example                     |
|-----------------------------|--------------------|-----------------------------|
| C functions                 | `snake_case`       | `get_device_info`           |
| C++ free functions          | `lowerCamelCase`   | `createId`, `getTimeDiff`   |
| Classes                     | `PascalCase`       | `DeviceManager`             |
| Public class methods        | `PascalCase`       | `CreateId`, `GetTimeDiff`   |
| Protected/private methods   | `_PascalCase`      | `_CreateId`, `_ReadStatus`  |
| Macros                      | `UPPER_SNAKE_CASE` | `IWDG_KEY_RELOAD`           |
| Constants                   | `kCamelCase`       | `kInvalidId`, `kTimeoutMs`  |
| Types (`typedef`)           | `PascalCase_t`     | `AcroTick_t`, `AcroParam_t` |
| Struct typedef              | `SPascalCase_t`    | `SAcroProcess_t`            |
| Union typedef               | `UPascalCase_t`    | `UDataWord_t`               |
| Enum typedef                | `EPascalCase_t`    | `EAcroStatus_t`             |
| Enum values                 | `eCamelCase`       | `eSoftware`, `eWatchDog`    |
| Module-level statics        | `lower_snake_case` | `system_clock_hz`           |

Compiler-, platform-, and standard-library-defined macros retain their
documented names even when they do not follow this table. For example, use the
MSVC predefined `_DEBUG` macro as provided. Do not use reserved identifiers for
project-defined macros; names beginning with an underscore followed by an
uppercase letter are reserved to the implementation.

## Text encoding

Store source files as UTF-8. All source text, comments, string literals, and
documentation must contain valid UTF-8 text. Legacy code-page encoded bytes and
invalid UTF-8 byte sequences are not permitted.

## Binary interface (Plain C)

Expose the binary library interface as Plain C so clients can load it from
arbitrary runtime environments. Requiring every client to use the same compiler
and build parameters is not acceptable; the application binary interface (ABI)
must instead remain interoperable across supported toolchains.

C++ compilers use implementation-specific name mangling and may produce
incompatible binary representations. Therefore, exported interface functions
must use C linkage (`extern "C"` when declared in C++), C-compatible fixed-width
types, and C-compatible data structures. Do not expose C++ classes, templates,
exceptions, overloaded functions, or standard-library types across the binary
boundary. C++ may be used internally behind this interface.

## Platform separation

Keep platform-independent application or kernel code free of MCU-specific
addresses, register definitions, and vendor-specific includes.

- `src/` contains application or platform-independent kernel code.
- `bsp/` contains board and MCU initialization, peripheral access, and device
    configuration.
- `port/` contains platform adaptations and integration hooks for libraries.

Use an explicit BSP or port API, or a weak hook overridden by the platform
implementation, to cross this boundary. Do not move hardware details into
platform-independent modules merely to avoid adding an interface.

## Conditional compilation

Always put the macro name in the closing comment:

```c
#if (SOME_OPTION == 1)
/* ... */
#endif /* SOME_OPTION */
```

## Preferences

Prefer a `switch` statement over a long `if`/`else if` chain.

## Magic numbers

Do not use unexplained numeric literals in logic, conditions, sizes, offsets,
protocol fields, or bit masks. Use a nearby named constant or macro instead.
Small obvious values such as `0`, `1`, and `-1` may be used when their meaning
is unambiguous. Document non-obvious macros as described above.

## Doxygen API documentation

Public **declarations** in headers carry the complete Doxygen documentation.
Definitions do not repeat that documentation and do not require an `@fn` tag.

Use `@fn` only when a documentation block is not immediately before the
corresponding declaration or definition. For an overloaded function, specify
the fully qualified function signature to avoid ambiguity.

- `@brief` — one grammatical sentence with normal terminal punctuation.
- `@param[in]` / `@param[out]` — one line per parameter.
- `@returns` — describe the return value. Use `@returns`, not `@return`.
- `@retval name desc` — one line per specific return code.
