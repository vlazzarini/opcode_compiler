Csound JIT Module Compilers
============

This experimental opcode library builds on the initial work by Michael Goggins, and is based on the
llvm/clang interpreter code provided in clang library sources. It includes opcodes for C/C++
compilation to LLVM IR, as well as to execute compiled code.

Operation principles
------------------

The principle of operation of these opcodes is as follows:

C/C++  -> LLVM bytecode -> execution

Starting with a C or C++ source code, the compiler produces an LLVM
bytecode that is stored in an execution object, the just-in-time (JIT)
compiler, which can be run immediately or at a later
time. Implementing this, separate opcodes are provided for compilaton
and function calls.

C/C++ compilation
---------------

The module compilers for C and C++ are

```
ires,ihandle c_module_compile Scode[, Sentry, Scflags, Sdylibs]
ires,ihandle cxx_module_compile Scode[, Sentry, Scflags, Sdylibs]
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

This function is executed at i-time immediately following any successful compilation.
The remaining optional parameters can be used to pass any C++ flags to
the compiler, and load any required dynamic libs.

The opcodes return the result of the function execution (if this is
executed) or zero on success, or a non-zero error as the first output.
The second output is a handle to the JIT compiler that can be passed
to other opcodes to execute code.

It is possible to have more than one JIT compiler at the same time.
Each instance of these opcodes creates a separate JIT object, that can
be used as a separate module during performance.

Example
------

The following is a simple C example (from `examples/opcode_compile_example.csd`) creating a simple gain opcode and
adding it to Csound. Note that since the opcode compiler adds the opcode to the system from the Csound
code itself, it is only available to instruments in subsequent compilations.

The orchestra code is composed of two opcode calls:

- to `c_module_compile` adding the new opcode
- to `compilestr` compiling the Csound code that uses this opcode.

New C opcodes are added to the system using the Csound API function

```
int csound::AppendOpcode(CSOUND *, const char *opname,
                                int dsblksiz, int flags, int thread,
                                const char *outypes, const char *intypes,
                                int (*iopadr)(CSOUND *, void *),
                                int (*kopadr)(CSOUND *, void *),
                                int (*aopadr)(CSOUND *, void *));
```

The C code is given as a string to `opcode_compile` using the `{{ }}` multiline
string. Note that this requires any backslashes (`\`) found in the
code to be escaped with another backslash (`\\`).

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
 }
 }}

ires,ihandle c_module_compile SCode, "module_init"
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

A more elaborare C++ example is also found in the examples
directory. In this particular use case, the handle provided by the
compiler opcode is not employed elsewhere.

Module function calls
------------------

Calling the module entry point after compilation is optional. If no
entry point, or an empty string, is given, no code is executed. In this
case, we can use the module compiler to provide general-purpose code
to be called at i- or perf-time.

A C function with the signature 

```
 int func(CSOUND *csound, OPDS h, MYFLT*out[], MYFLT *in[])
```

or a C++ function with the signature

```
extern "C" int func(CSOUND *csound, const OPDS &h, MYFLT*out[], MYFLT *in[])
```

may be invoked at a later time using 

```
ir1[,ir2, ...]  c_module_fcall ihandle, Sfunc[,...]  // C
ir1[,ir2, ...]  cxx_module_fcall ihandle, Sfunc[,...]  // C++
```

and/or

```
xr1[,xr2, ...]  c_module_fcallk ihandle, Sfunc[,...]  // C
xr1[,xr2, ...]  cxx_module_fcallk ihandle, Sfunc[,...]  // C++
```

These functions take a handle to a JIT compiler containing the code to be executed.
The former C or C++ code runs at i-time only and the latter is called at perf-time,
on every k-cycle, and the function name is passed as the string parameter `Sfunc`.
The opcodes can use up to 32 outputs and 256 inputs whose types may be
determined by the C or C++ code. These are available to the
function as the `MYFLT *` arrays `out` and `in`.  The
`jit_example.csd` demonstrates a C-language example in which one JIT
compiler object is used to provide two separate functions that are
invoked later at init and perf time. The `jit_example_c++.csd` example
demonstrate two C++ compilers providing two separate JIT objects
each one with a function that is called  at init and perf time.
The `c++_oscil_example.csd` shows a simple sinusoidal oscillator
written in C++ and implemented with these opcodes.

C++ opcode objects
----------

In addition to function calls, it is possible to construct and run C++
objects at i, k, or a rates (or a combination of these). For these,
the code needs to provide a class implementing the opcode processing,
and an entry function to instantiate objects of this class.

The class should be derived from the `JITPlugin` base class provided
by the `jitplugin.h` header file. The class should implement a
constructor calling the base class constructor and passing the OPDS
object to the base class constructor. It can then implement one or
both of the processing methods called at init and perf time.

```
 struct OpcodeObj : JITPlugin {
   OpcodeObj(OPDS h) : JITPlugin(h) {};  // constructor
   int init()  { return OK; }  // called at init-time
   int perf() { return OK; }   // called at perf time
 };
 ```

The entry point function is then used to instantiate and return the
class object in the following form

```
auto entry(OPDS h) {
 return new OpcodeObj(h);
}
```

Once the object is defined in the C++ code it can be run
by passing the entry point name and the JIT handle to the appropriate
`cxx_opcode_` opcode.

```
ires[,..]  cxx_opcode_i ihandle,Sentry[,...]  // i-time only
ksig[,..]  cxx_opcode_k ihandle,Sentry[,...] // perf-time only ksig
xsig[,..]  cxx_opcode_a ihandle,Sentry[,...] // perf-time only xsig
k/ivar[,..]  cxx_opcode_ik ihandle,Sentry[,...] // i-time and perf-time i/kvars
xvar[,..]  cxx_opcode_ia ihandle,Sentry[,...] // i-time and perf-time xvars
```

Supports for argument access etc are provide in a similar form to
that in CPOF. However, since the code is running fully within a C++
environment, classes can use the full range of supports from the
C++ standard libraries and other APIs. Objects are automatically
deleted at instrument deinit time, but any dynamically allocated
resources should be disposed off in a class destructor. A simple
example is given in the `opcode_class_c++.csd` code.

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
