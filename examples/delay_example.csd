<CsoundSynthesizer>
<CsOptions>
-odac 
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
     a1 diskin "fox.wav"
     a2 cxx_opcode_ia gihandle,"delayline",a1,0.5,0.5
     out a2
endin


</CsInstruments>
<CsScore>
i1 0 10
</CsScore>
</CsoundSynthesizer>
