<CsoundSynthesizer>
<CsOptions>
-n
</CsOptions>
<CsInstruments>
0dbfs = 1

SCode = {{
 #include "../jitplugin.h"
 #include <vector>

 struct DelayLine : JITPlugin {
   std::vector<MYFLT> delay;
   std::vector<MYFLT>::iterator iter;
   DelayLine(OPDS h) : JITPlugin(h), delay(0) { };

   int init() {
    if(inargs[1] > 10000)
      return csound->init_error("delay time too long\\n");
    delay.resize(csound->sr() * inargs[2]);
    iter = delay.begin();
    return OK;
  }

  int perf() {
    csnd::AudioSig in(this, inargs(0));
    csnd::AudioSig out(this, outargs(0));
    
    std::transform(in.begin(), in.end(), out.begin(), [this](MYFLT s) {
      MYFLT o = *iter;
      MYFLT g = inargs[1];
      *iter = s + g*o;
      if (++iter == delay.end())
        iter = delay.begin();
      return o;
    });
    return OK;
  }
};

 extern "C" {
   auto delayline(OPDS h) {
    return new DelayLine(h);
   }
}
}}

gires,gihandle cxx_module_compile SCode

instr 1
 prints "\n*********\nRunning JIT delayline C++ opcode\n**********\n\n"
 idt = 0.5
 kg = 0.5
 a1 diskin "fox.wav",1,0,1
 a2 cxx_opcode_ia gihandle,"delayline",a1,kg,idt
    out a2*0.5
endin

instr 2
 prints "\n*********\nRunning comb opcode\n**********\n\n"
 idt = 0.5
 kg = 0.5
 a1 diskin "fox.wav",1,0,1
 a2 comb a1,-3*idt/log10(kg),0.5
    out a2*0.5
endin

opcode DelayLine,a,aki
 ain, kg, idt xin
 ids = idt*sr
 adel[] init ids
 kp init 0
 aout init 0

 kn = 0
 while kn < ksmps do
  aout[kn] = adel[kp]
  adel[kp] = ain[kn] + adel[kp]*kg
  kp = kp != ids - 1 ? kp + 1 : 0
  kn += 1
 od

 xout aout
endop

instr 3
 prints "\n*********\nRunning DelayLine UDO\n**********\n\n"
 idt = 0.5
 kg = 0.5
 a1 diskin "fox.wav",1,0,1
 a2 DelayLine a1,kg,idt
    out a2*0.5
endin


</CsInstruments>
<CsScore>
i1 0 100
</CsScore>
</CsoundSynthesizer>

