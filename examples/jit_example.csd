<CsoundSynthesizer>
<CsOptions>
-odac 
</CsOptions>
<CsInstruments>
0dbfs = 1

 SCode = {{
  #include <csdl.h>
    int func(CSOUND *csound, OPDS h, MYFLT *out[], MYFLT *in[]) {
     csound->Message(csound, "there are %d inputs and %d outputs\\n", INCOUNT-2, OUTCOUNT);
     *out[0] =  *in[0];
     return OK;
   }
  
   int funck(CSOUND *csound, OPDS h, MYFLT *out[], MYFLT *in[]) {
     *out[0] =  *in[0];
     return OK;
  }
}}

 gires,gihandle c_module_compile SCode

instr 1
 if gires == 0 then
  i1 c_module_fcall gihandle,"func", 1
  print i1
  k1 c_module_fcallk gihandle,"funck", 1
  printk2 k1
 endif 
endin 

</CsInstruments>
<CsScore>
i1 0 1
i1 1 1
</CsScore>
</CsoundSynthesizer>
