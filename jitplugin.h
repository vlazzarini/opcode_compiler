#ifndef __JITPLUGIN__
#define __JITPLUGIN__
#include <plugin.h>

struct BasePlugin : OPDS {
   MYFLT **out;
   MYFLT **in;
   CSOUND *csound;

   BasePlugin(OPDS h) : OPDS(h) { };
   virtual ~BasePlugin() { };
   virtual int init() = 0;
   virtual int perf() = 0;
 };


struct JITPlugin : BasePlugin {
  JITPlugin(OPDS h) : BasePlugin(h) { }; 
  int init() { return OK;}  
  int perf() { return OK;} 
 };

#endif // __JITPLUGIN__
