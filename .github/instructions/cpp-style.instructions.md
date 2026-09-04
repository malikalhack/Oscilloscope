---
description: "Use when writing or editing C or C++ code. Covers file headers, 80-char section banners, header guards, include order, Doxygen, naming, and MISRA-driven rules (single entry/exit, declarations at block top)."
applyTo: "**/*.{c,cc,cpp,cxx,h,hh,hpp,hxx}"
---

# C/C++ Style

Applies to C and C++ sources. Rules tagged **[MISRA]**
exist to satisfy static analysis (SonarQube, MISRA profile) — keep them even when
they differ from idiomatic modern C++.
80-character lines. Note: Breaking this rule is permitted if the number of characters exceeding this limit is no more than two. 

## File header

Every `.c`/`.h`/`.cpp`/`.hpp` starts with this block (ISO 8601 dates, no
`@copyright`; a file-level `@brief` may go inside):

```c
/**
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
/********************* Application Programming Interface *********************/
/******************************************************************************/
```

Between functions use the thin separator:

```c
/*----------------------------------------------------------------------------*/
```

## Header guard

Use the `//!` style on `#endif` (not `/* NAME */`):

```c
#ifndef FILENAME_H_
#define FILENAME_H_

/* ... */

#endif //! FILENAME_H_
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

## Functions

One-line `/** @fn name */` immediately before a free-function **definition**.
For a class member definition, use its qualified declaration, for example
`/** @fn bool ClassName::method(int value) */`, so Doxygen resolves the member.
Use a K&R brace (opening brace on the signature line). If a condition does not
fit on one line, put the first sub-expression on the next line, align
sub-expressions, and place the closing parenthesis with the opening brace on
their own lines:

```c
/** @fn acroAddTask */
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

`else` always starts on a new line after the closing brace.

## Declarations at block top [MISRA]

All local variables are declared at the top of the function block, before any
statement. Never mix declarations with code.

```c
int foo(void) {
    int ret_val = ERROR;   /* declare + init here */
    uint32_t timeout;

    timeout = 100000UL;    /* first statement after all declarations */
    return ret_val;
}
```

## Single entry / single exit [MISRA]

One `return` at the bottom. Use `ret_val` as the result accumulator; set it on
success, leave it at the error default otherwise.

## Naming

| Category               | Convention        | Example                     |
|------------------------|-------------------|-----------------------------|
| Functions (public)     | `lowerCamelCase`  | `acroAddTask`, `acroRun`    |
| Functions (static)     | `lowerCamelCase`  | `createId`, `getTimeDiff`   |
| Macros / constants     | `UPPER_SNAKE_CASE`| `IWDG_KEY_RELOAD`           |
| Types (`typedef`)      | `PascalCase_t`    | `AcroTick_t`, `AcroParam_t` |
| Struct typedef         | `SPascalCase_t`   | `SAcroProcess_t`            |
| Union typedef          | `UPascalCase_t`   | `UDataWord_t`               |
| Enum typedef           | `EPascalCase_t`   | `EAcroStatus_t`             |
| Enum values            | `eCamelCase`      | `eSoftware`, `eWatchDog`    |
| Module-level statics   | `lower_snake_case`| `system_clock_hz`           |

Pointer and reference declarators stay next to the variable name, not the type:

```cpp
int *value_ptr;
const char *name;
std::string &value_ref;
const std::vector<int> &values;
```
The exception is the return types of functions:
```cpp
static libusb_device* findDeviceByInfo(
    libusb_device **deviceList,
    const SUsbDeviceInfo &deviceInfo,
    const ssize_t deviceCount
);
```

The structure variables are placed taking into account alignment and the minimum amount of padding.

## Conditional compilation

Always put the macro name in the closing comment:

```c
#if (SOME_OPTION == 1)
/* ... */
#endif /* SOME_OPTION */
```

## Preferences

Prefer a `switch` statement over a long `if`/`else if` chain.

Follow DRY: do not duplicate implementation logic. Reuse an existing helper or
abstraction when suitable; otherwise extract the smallest clearly named shared
helper that removes meaningful repetition.

Follow KISS: prefer the simplest design that fully satisfies the current
requirements. Do not add layers, generic abstractions, configuration, or
extension points without a concrete present need.

## Doxygen API documentation

Public **declarations** (in `.h`) carry a full Doxygen block; **definitions**
(in `.c`) carry only the one-line `/** @fn name */` tag.

- `@brief` — one sentence, no trailing period.
- `@param[in]` / `@param[out]` — one line per parameter.
- `@returns` — describe the return value. Use `@returns`, not `@return`.
- `@retval name desc` — one line per specific return code.
