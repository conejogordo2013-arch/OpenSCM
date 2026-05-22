# SCML Mega Bootstrap Manifest

This manifest enumerates the aggressive bootstrap surface added in this commit.  
Everything is represented in SCML-centric style and naming.

## Frontend
- Preprocessor directives catalog
- Macro operators catalog (`#`, `##`)
- Builtin macro catalog (`__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`, `__scml`)
- Keyword inventory (full reserved word set)
- Operator inventory and semantic buckets

## Semantics Catalog
- Type categories
- Value categories
- Storage duration categories
- Linkage categories
- Initialization categories
- Conversion categories
- Overload resolution categories
- Template advanced categories
- Object/model rules
- Coroutine internals
- Module internals

## Runtime-facing SCML style
- All declarations kept as SCML symbols/macros or SCML scripts.
- No host C++ syntax introduced as source-language requirement.
