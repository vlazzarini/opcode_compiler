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
    csound->AppendOpcode(csound, "amp",sizeof(DATASPACE),0,3,"a","ak",
                         (SUBR) init, (SUBR) perf, NULL);                     
    return 0;
 };
}}

ksmps = 64
SCscode = {{

    opcode Amp,a,ak
      a1,k1 xin
      a2 = a1*k1
      xout a2
    endop

    instr 1
     a1 oscili 0dbfs,A4
     a2 amp a1,0.5
     out a2
    endin

     instr 2
     a1 oscili 0dbfs,A4
     a2 Amp a1,0.1
     out a2
    endin

}}


ires,ihandle c_module_compile SCode, "module_init"
if ires == 0 then
 ires compilestr SCscode
endif

</CsInstruments>
<CsScore>
i1 0 5
</CsScore>
</CsoundSynthesizer>
<bsbPanel>
 <label>Widgets</label>
 <objectName/>
 <x>100</x>
 <y>100</y>
 <width>320</width>
 <height>240</height>
 <visible>true</visible>
 <uuid/>
 <bgcolor mode="nobackground">
  <r>255</r>
  <g>255</g>
  <b>255</b>
 </bgcolor>
</bsbPanel>
<bsbPresets>
</bsbPresets>
