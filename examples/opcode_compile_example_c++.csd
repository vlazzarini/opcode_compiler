<CsoundSynthesizer>
<CsOptions>
-odac 
</CsOptions>
<CsInstruments>
0dbfs = 1

SCode = {{
 #include <plugin.h>
 
 struct DelayLine : csnd::Plugin<1, 3> {
  static constexpr char const *otypes = "a";
  static constexpr char const *itypes = "aki";
  csnd::AuxMem<MYFLT> delay;
  csnd::AuxMem<MYFLT>::iterator iter;

  int init() {
    if(inargs[1] > 10000)
      return csound->init_error("delay time too long\\n");
    
    delay.allocate(csound, csound->sr() * inargs[2]);
    iter = delay.begin();
    return OK;
  }

  int aperf() {
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

 extern "C" int module_init(CSOUND *csound) { 
    csnd::plugin<DelayLine>((csnd::Csound *)csound, "delayline", csnd::thread::ia);                     
    return 0;
 };
}}

SCscode = {{
    instr 1
     a1 diskin "fox.wav"
     a2 delayline a1,0.5,0.5
     out a2
    endin
}}


ires,ihandle cxx_module_compile SCode, "module_init"
ires compilestr SCscode

</CsInstruments>
<CsScore>
i1 0 5
</CsScore>
</CsoundSynthesizer>
