Csound JIT Module Compilers
============

This experimental opcode builds on the initial work by Michael Goggins, and is based on the
llvm/clang interpreter example code. It provides a just-in-time C and C++
module compilers, which can be used to add new opcodes to Csound or to
execute code on-the-fly.

The module compilers syntax for C and C++ are

```
ires c_module_compile Scode[, Sentry, Scflags, Sdylibs]
ires cxx_module_compile Scode[, Sentry, Scflags, Sdylibs]
```

where `Scode` is a C or C++-language module containing the opcodes to be added to the system,
provided as a string, and `Sentry` is the name of the entry point
function declared in C as

```
int entry(CSOUND *csound);
```

If C++ is used then we have

```
extern "C" int entry(CSOUND *csound);
```

The remaining optional parameters can be used to pass any C++ flags
to the compiler, and load any required dynamic libs.

Example
------

The following is a simple C example (from `examples/opcode_compile_example.csd`) creating a simple gain opcode and
adding it to Csound. Note that since the opcode compiler adds the opcode to the system from the Csound
code itself, it is only available to instruments in subsequent compilations.

The orchestra code is composed of two opcode calls:

- to `module_compile` adding the new opcode
- to `compilestr` compiling the Csound code that uses this opcode.

New opcodes are added to the system using the Csound API function

```
int csound::AppendOpcode(CSOUND *, const char *opname,
                                int dsblksiz, int flags, int thread,
                                const char *outypes, const char *intypes,
                                int (*iopadr)(CSOUND *, void *),
                                int (*kopadr)(CSOUND *, void *),
                                int (*aopadr)(CSOUND *, void *));
```

The C code is given as a string to `opcode_compile` using the `{{ }}` multiline
string.

```
SCode = {{
 #include <csdl.h>
 struct dataspace {
    OPDS h;
    MYFLT *out, *in, *gain;
 } DATASPACE;

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

 int module_init(CSOUND *csound) {
    csound->AppendOpcode(csound, "amp",sizeof(DATASPACE),0,3,"a","ak",
                         (SUBR) init, (SUBR) perf, NULL);
    return 0;
 };
 }}

ires= c_module_compile(SCode, "module_init")
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

A more elaborare C++ example is also found in the examples directory.

Module function calls
------------------

Calling the module entry point after compilation is optional. If no
entry point, or an empty string is given, nothing is called. In this
case, we can use the module compiler to provide general-purpose code
to use at i- or perf-time.

A C function with the signature 

```
 int func(CSOUND *csound,  OPDS h, MYFLT*out[], MYFLT *in[])
```

or a C++ function with the signature

```
extern "C" int func(CSOUND *csound, const OPDS &h, MYFLT*out[], MYFLT *in[])
```

may be invoked at a later time using 

```
ir1[,ir2, ...]  c_module_fcall Sfunc[,...]
ir1[,ir2, ...]  cxx_module_fcall Sfunc[,...] 
```

and/or

```
xr1[,xr2, ...]  c_module_fcallk Sfunc[,...]
xr1[,xr2, ...]  cxx_module_fcallk Sfunc[,...] 
```

The former C or C++ code runs at i-time only and the latter is called at perf-time,
on every k-cycle, and the function name is passed as the string parameter `Sfunc`.
The opcodes can use up to 32 outputs and 256 inputs
whose types may be determined by the C or C++ code. These are available to the
function as the `MYFLT *` arrays `out` and `in`.  The
`jit_example.csd` and `jit_example_c++.csd` examples demonstrate
the use of these functions.

Building the opcodes
---------

The opcodes require LLVM and Clang libraries, version >= 13.0.0, which
can be installed from
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

Victor Lazzarini  
October 2021
