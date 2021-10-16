<CsoundSynthesizer>
<CsOptions>
-odac 
</CsOptions>
<CsInstruments>
0dbfs = 1

 SCode = {{
  #include <csdl.h>
  extern "C" int func(CSOUND *csound, const OPDS &h, MYFLT *out[], MYFLT *in[]) {
     csound->Message(csound, "there are %d inputs and %d outputs\\n", INCOUNT-1, OUTCOUNT);
     *out[0] =  *in[0];
    return OK;
  };
}}

 ires = module_compile(SCode)

instr 1
 ires module_fcall "func", 1
 print ires
endin 

</CsInstruments>
<CsScore>
i1 0 1
i1 1 1
</CsScore>
</CsoundSynthesizer>
