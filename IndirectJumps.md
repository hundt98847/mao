# Indirect jumps #

In MAO, the Control Flow Graph (CFG) should cover all possible transitions between basic blocks. For indirect jumps, only edges to targets in jump-tables should be added whenever such tables can be found.

Switch statements are translated to indirect jumps with jump tables in gcc. MAO use pattern matching to find such cases and adds the appropriate edges in the CFG.

_This is currently being developed and more info will be added here when the feature is implemented._