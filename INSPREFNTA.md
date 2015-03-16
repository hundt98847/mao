## INSPREFNTA - Prefetchnta Insertion for Specified Instructions ##

### Description ###
This pass inserts a prefetchnta instruction right before a list of specified instructions. Instructions are specified in a file as a list of (file name, function, offset) tuple. File name part of this pass is broken at the moment, i.e, it simply ignores the file name, and looks for relevant function name, offset pairs. So as far as multiple files do not have functions with the same name, it should work just fine.

Experiments show quite interesting results. We were able to reduce last level cache misses by a significant percentage by inserting a very small number of prefetchnta instructions in a program. However, these added instructions have some overhead and the overall performance (time) could not be improved, but it does show some promise.

### Options ###
```
  instn_list: (string) Filename from which to read list of file name and function name and offset pairs.
```
### Source ###
```
  MaoInsertPrefNta.cc
```