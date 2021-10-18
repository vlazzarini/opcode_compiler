<CsoundSynthesizer>
<CsOptions>
-odac 
</CsOptions>
<CsInstruments>
0dbfs = 1

SCode = {{
 #include "../jitplugin.h"

 struct TestInit : JITPlugin {
    TestInit(OPDS h) : JITPlugin(h) {}; 

    int init()  {
        csound->message("init()");
        outargs[0] = inargs[0];
        return OK;
    }  
 };

 struct TestAudio : JITPlugin {
    TestAudio(OPDS h) : JITPlugin(h) {}; 

    int perf()  {
        for(int n = offset; n < nsmps; n++) 
             outargs(0)[n] = inargs(0)[n];
        return OK;
    }  
 };

 struct TestControl : JITPlugin {
    TestControl(OPDS h) : JITPlugin(h) {}; 

    int perf()  {
        outargs[0] = inargs[0];
        return OK;
    }  
 };


 extern "C" {
   auto testControl(OPDS h) {
    return new TestControl(h);
   }
   auto testAudio(OPDS h) {
    return new TestAudio(h);
   }
   auto testInit(OPDS h) {
    return new TestInit(h);
   }
}
}}

gires,gihandle cxx_module_compile SCode

instr 1
     i1 = 1
     i2 cxx_opcode_i gihandle,"testInit", i1
     print i2
     k1 = 1
     k2 cxx_opcode_a gihandle,"testControl", k1
     printk2 k2
     a1 oscili 0dbfs/2,A4
     a2 cxx_opcode_a gihandle,"testAudio", linen(a1,0.1,p3,0.1)
     out a2
endin


</CsInstruments>
<CsScore>
i1 0 1
</CsScore>
</CsoundSynthesizer>
