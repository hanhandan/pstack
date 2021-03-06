# pstack
Print stack traces of running processes. Uses its own ELF and DWARF parsing.

## installation
You'll need cmake and a C++ compiler. Build/install with cmake. Eg:
<pre>
cd pstack/
mkdir `hostname`
cd `hostname`
cmake ..
make
sudo make install
</pre>

## Overview
This is an implementation of pstack that uses the dwarf unwind tables to do its work.
I wrote it mostly out of curiosity, and maintain it because sometimes its useful.

It also includes "canal", which allows you to analyze objects in the address space,
identifying their types through the stored vptrs, and cross-referencing that with
the vtables found in the symbol table. This can give a quick-and-dirty histogram
of live objects by type for finding memory leaks.

## TODO
* Support GNU_ref_alt
   * dwz creates these
* Read memory contents for arg printing
   * Read content of target memory for displaying argument values
   * pretty-print basic types like numbes, pointers, etc.
* Support rela for object files
   * Can't do much with object files, but at least having the dump functions
     work would be nice.
* The manual page could do with some care and attention
* This actually works on ARM (i.e., raspi), but needs debug_frame. Apparently,
  ARM has its own magical sections for storing unwind information that it might
  be worth implementing.
