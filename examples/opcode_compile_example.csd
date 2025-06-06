<CsoundSynthesizer>
<CsOptions>
-odac 
</CsOptions>
<CsInstruments>
0dbfs = 1

SCode = {{
 #include <csdl.h>
 
 typedef struct dataspace {
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
    csound->AppendOpcode(csound, "amp",sizeof(DATASPACE),0, "a","ak",
                         (SUBR) init, (SUBR) perf, NULL);                     
    return 0;
 };
}}

ksmps = 64
SCscode = {{
    instr 1
     a1 oscili 0dbfs,A4
     a2 amp a1,0.5
     out a2
    endin
}}


ires,ihandle c_module_compile SCode, "module_init", "-v"
if ires == 0 then
 ires compilestr SCscode
endif

</CsInstruments>
<CsScore>
i1 0 5
</CsScore>
</CsoundSynthesizer>

