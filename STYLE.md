# Style Guide

Naming conventions:

| Item | Convention|
| - | - |
|`class`|`PascalCase`|
|`struct`|`PascalCase`|
|`alias`|`PascalCaseType`|
|Concept|`PascalCase`|
|Variable|`snake_case`|
|Function|`snake_case`|
|Method|`snake_case`|
|Macro|`SHOUT_CASE`|

Metatypes must end in `Type`.
Concepts must begin with either `Is` or `Not`.
Sufficiently 'elementary' types may be `snake_case`, provided there is an explanation for this decision.

Use `auto` where possible.
Use `#pragma once`.