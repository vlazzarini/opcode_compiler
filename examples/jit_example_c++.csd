<CsoundSynthesizer>
<CsOptions>
-odac 
</CsOptions>
<CsInstruments>
0dbfs = 1

 SCode1 = {{
  #include <csdl.h>
  extern "C" 
    int func(CSOUND *csound, const OPDS &h, MYFLT *out[], MYFLT *in[]) {
     csound->Message(csound, "there are %d inputs and %d outputs\\n", INCOUNT-2, OUTCOUNT);
     *out[0] =  *in[0];
     return OK;
   }
  }}

 SCode2 = {{
  #include <csdl.h>
  extern "C" 
   int func(CSOUND *csound, const OPDS &h, MYFLT *out[], MYFLT *in[]) {
     *out[0] =  *in[0];
     return OK;
   }
}}

 gires, gihandle1 cxx_module_compile SCode1
 gires, gihandle2 cxx_module_compile SCode2
 
instr 1
 if gires == 0 then
  i1 cxx_module_fcall gihandle1,"func", 1
  print i1
  k1 cxx_module_fcallk gihandle2,"func", 1
  printk2 k1
 endif 
endin 

</CsInstruments>
<CsScore>
i1 0 1
i1 1 1
</CsScore>
</CsoundSynthesizer>
