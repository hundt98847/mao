# Introduction #

Intel's optimization manual makes the interesting point that inc only modifies a subset of the flags, vs add sets them all. Internally, this creates a dependency, which would block other flag modifying instructions from being reordered.

To quote http://www.intel.com/Assets/en_US/PDF/manual/248966.pdf:

3.5.1.1  Use of the INC and DEC Instructions
The INC and DEC instructions modify only a subset of the bits in the flag register. This
creates a dependence on all previous writes of the flag register. This is especially
problematic when these instructions are on the critical path because they are used to
change an address for a load on which many other instructions depend.
Assembly/Compiler Coding Rule 33. (M impact, H generality) INC and DEC
instructions should be replaced with ADD or SUB instructions, because ADD and
SUB overwrite all flags, whereas INC and DEC do not, therefore creating false
dependencies on earlier instructions that set the flags.


In other words, using add/sub is better than using inc/dec.

inc/dec have an encoding that's typically a byte shorter, e.g.:
```
   5:   fe c4                   inc    %ah
   7:   fe c0                   inc    %al
   9:   66 ff c0                inc    %ax
   c:   ff c0                   inc    %eax
   e:   48 ff c0                inc    %rax
  11:   fe cc                   dec    %ah
  13:   fe c8                   dec    %al
  15:   66 ff c8                dec    %ax
  18:   ff c8                   dec    %eax
  1a:   48 ff c8                dec    %rax
```
vs
```
   5:   80 c4 01                add    $0x1,%ah
   8:   04 01                   add    $0x1,%al
   a:   66 83 c0 01             add    $0x1,%ax
   e:   83 c0 01                add    $0x1,%eax
  11:   48 83 c0 01             add    $0x1,%rax
  15:   80 ec 01                sub    $0x1,%ah
  18:   2c 01                   sub    $0x1,%al
  1a:   66 83 e8 01             sub    $0x1,%ax
  1e:   83 e8 01                sub    $0x1,%eax
  21:   48 83 e8 01             sub    $0x1,%rax
```

Because of that, some code might get lucky and hit some other effect.

As a quick experiment, I downloaded bzip2-1.0.6 and convert all add|sub 1,reg to inc's and deg's. There were 349 occurances. The resulting binary runs indeed about 1 percent slower.

The sources are in:
```
  plugins/MaoAdd2Inc.cc
```