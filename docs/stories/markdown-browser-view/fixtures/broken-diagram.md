# Broken diagram fixture

A diagram with a syntax error:

```mermaid
this is not a diagram
```

The next diagram still draws:

```mermaid
flowchart LR
  A[Start] --> B[End]
```
