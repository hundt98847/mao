## PREFNTA - Simple Prefetch Insertion ##

### Description ###
This pass inserts a prefetch instruction right in front of every memory load and store operation. All x86 prefetch types are supported (nta, t0, t1, t2). The key idea for this path is in the SBO space (Mars, Hundt, CGO '09), where we basically try to avoid cache pollution by prefixing any load with a prefetchnta.

This technique yielded interesting performance results. As expected almost all benchmarks degraded, however, typically by less than 2x, most often in the 10% to 30% range. This was surprising. The larger degradations point to benchmarks that can benefit from memory hierarchy optimizations, and/or altered prefetch settings on the system. One benchmark actually improved by 6% on AMD/Opteron, independently of the type of prefetch being inserted. We suspect that a 2nd order effect kicked in, very likely some micro-architectural event.

We ran experiments with the various prefetch types and offsets to a given load, but couldn't materialize performance gains. We suspect the number of prefetches is just too high for our systems.
### Options ###
```
  offset    : (int)    Offset added to prefetch addresses
  ptype     : (int)    Type of prefetch (0: nta, ..., 3: t2)
```

### Source ###
```
  MaoPrefetchNta.cc
```