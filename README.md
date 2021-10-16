Csound JIT Module Compiler
============

This experimental opcode builds on the initial work by Michael Goggins, and is based on the
llvm/clang interpreter example code. It provides a just-in-time C/C++
module compiler, which can be used to add new opcodes to Csound on-the-fly.
The module compiler syntax is

```
ires module_compile Scode
```

where `Scode` is a C/C++-language module containing the opcodes to be added to the system,
provided as a string. This uses the C API for opcodes, and it should contain an entry point declared as

```
extern "C" int module_init(CSOUND *csound); 
```

where the new opcodes can be added to the system using

```
int csound::AppendOpcode(CSOUND *, const char *opname,
                                int dsblksiz, int flags, int thread,
                                const char *outypes, const char *intypes,
                                int (*iopadr)(CSOUND *, void *),
                                int (*kopadr)(CSOUND *, void *),
                                int (*aopadr)(CSOUND *, void *));
```


Building
------

The opcodes require LLVM and Clang libraries, version >= 13.0.0, to be installed from
https://github.com/llvm/llvm-project, as well as Csound and CMake. With these in place,
the following steps apply

```
mkdir build
cd build
cmake ..
make 
```

and to install it in the default location, 

```
make install
```

Example
------

The following is a simple example (from ` examples/opcode_compile_example.csd`) creating a simple gain opcode and
adding it to Csound. Note that since the opcode compiler adds the opcode to the system from the Csound
code itself, it is only available to instruments in subsequent compilations.

The orchestra code is composed of two opcode calls:

- to `module_compile` adding the new opcode
- to `compilestr` compiling the Csound code that uses this opcode.

The C code is given as a string to `opcode_compile` using the `{{ }}` multiline
string.

```
SCode = {{
 #include <csdl.h>
 struct DATASPACE {
    OPDS h;
    MYFLT *out, *in, *gain;
 };

 static int init(CSOUND *csound, DATASPACE *p) {
       return OK;
 }

 static int perf(CSOUND *csound, DATASPACE *p) {
    MYFLT *out = p->out, *in = p->in, g = *p->gain;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early  = p->h.insdshead->ksmps_no_end;
    uint32_t n, nsmps = CS_KSMPS;

    if (UNLIKELY(offset)) memset(out,0, offset*sizeof(MYFLT));
    if (UNLIKELY(early)) {
      nsmps -= early;
      memset(&out[nsmps],0, early*sizeof(MYFLT));
    }

    for(n=offset; n < nsmps; n++) out[n] = in[n]*g;
    return OK;
 }

 extern "C" int module_init(CSOUND *csound) {
    csound->AppendOpcode(csound, "amp",sizeof(DATASPACE),0,3,"a","ak",
                         (SUBR) init, (SUBR) perf, NULL);
    return 0;
 };
 }}

ires= module_compile(SCode, "module_init")
```

This compiles the new opcode (`amp`) using the JIT compiler and executes the `module_init` function to add
the opcode to the Csound instance. Now we can use it in our Csound instruments.

```
SCscode = {{
    instr 1
     a1 oscili 0dbfs,A4
     a2 amp a1, 0.5
     out a2
    endin
    }}

ires = compilestr(SCscode)
```

Both C and C++ can be used to create opcodes now. A C++ example is provided in the examples directory. 

Module function calls
------------------

Calling the module entry point after compilation is optional. If no
entry point, or an empty string is given, nothing is called. In this
case, we can use the module compiler to provide general-purpose code
to use at i- or perf-time.

A function with the signature

```
int func(CSOUND *, const OPDS &, MYFLT*[], MYFLT*[])
```

may be invoked at a later time using one of two opcodes:

```
ir1[,ir2, ...]  module_fcall Sfunc[,...] 
```

and

```
xr1[,xr2, ...]  module_fcallk Sfunc[,...] 
```

The former runs at i-time only and the latter is called at perf-time,
on every k-cycle. The opcodes have up to 32 outputs and 256 inputs
whose types may be determined by the user. The `jit_example.csd`
demonstrates the use of these functions.

Victor Lazzarini  
October 2021
