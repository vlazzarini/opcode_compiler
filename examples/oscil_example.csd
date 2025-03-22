<CsoundSynthesizer>
<CsOptions>
-odac --opcode-lib=./libmodule_compiler.dylib 
</CsOptions>
<CsInstruments>

0dbfs = 1
SCode = {{
  #include <cstdint>
  #include <csdl.h>
  #include <cmath>
  #include <cstdio>

  struct Oscil {
     double ph;
     MYFLT sr;
     Oscil(double eph, MYFLT esr) : ph(eph), sr(esr) { };

     MYFLT operator()(MYFLT fr) {
        MYFLT s = std::cos(2*M_PI*ph);
        ph += fr/sr;
        while(ph < 0.) ph += 1.;
        while(ph >= 1.) ph -= 1.;
        return s;
      }
  };

  union flptr {
    double fl;
    Oscil *ptr;
   };

  extern "C" {
    int init(CSOUND *csound, const OPDS &h, MYFLT *out[], MYFLT *in[]) {
     auto o = new Oscil(0., h.insdshead->esr);
     flptr cvt;
     cvt.ptr = o;
     *out[0] = cvt.fl;
     return OK;
    }

    int perf(CSOUND *csound, const OPDS &h, MYFLT *out[], MYFLT *in[]) {
     uint32_t offset = h.insdshead->ksmps_offset;
     uint32_t early  = h.insdshead->ksmps_no_end;
     uint32_t n, nsmps = h.insdshead->ksmps;
     MYFLT *sig = out[0];
     MYFLT a = *in[1], f = *in[2];
     flptr cvt;
     cvt.fl = *in[0];
     Oscil &oscil = *cvt.ptr;

    if (UNLIKELY(offset)) memset(sig,0, offset*sizeof(MYFLT));
    if (UNLIKELY(early)) {
      nsmps -= early;
      memset(&sig[nsmps],0, early*sizeof(MYFLT));
    }

    for(n=offset; n < nsmps; n++) sig[n] = a*oscil(f);
     return OK;
    }

   int deinit(CSOUND *csound, const OPDS &h, MYFLT *out[], MYFLT *in[]) {
     flptr cvt;
     cvt.fl = *in[0];
     auto oscil = cvt.ptr;
     delete oscil;
     *out[0] = 0.f;
     return OK;
   }
  }
 }}

 gires, gihandle cxx_module_compile SCode

instr 1
 if gires == 0 then
  iosc cxx_module_fcall gihandle,"init"
  a2 = 0
  a1 cxx_module_fcallk gihandle,"perf",iosc,p4,p5,a2
  kend lastcycle
  if kend != 0 then
   k1 cxx_module_fcallk gihandle,"deinit",iosc
  endif
  out linen(a1,0.1,p3,0.1)
 else
  prints "failed to compile\n";
 endif
 
endin 

</CsInstruments>
<CsScore>
i1 0 2 0.2 440
;i1 0 2 0.1 660
</CsScore>
</CsoundSynthesizer>
