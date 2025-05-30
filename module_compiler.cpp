/*
  opcode_compile.cpp:

  Copyright (C) 2021, Victor Lazzarini

  This plugin library is free software; you can redistribute it
  and/or modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later veCOUrsion.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with Csound; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA
*/

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Tool.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <csdl.h>
#include <vector>
#include <cstdlib>

using namespace clang;
using namespace clang::driver;

namespace llvm {
  namespace orc {
    class JIT {
    private:
      ExecutionSession ES;
      std::unique_ptr<TargetMachine> TM;
      const DataLayout DL;
      MangleAndInterner Mangle{ES, DL};
      JITDylib &MainJD{ES.createBareJITDylib("<main>")};
      RTDyldObjectLinkingLayer ObjectLayer{ES, createMemMgr};
      IRCompileLayer CompileLayer{ES, ObjectLayer,
          std::make_unique<SimpleCompiler>(*TM)};

      static std::unique_ptr<SectionMemoryManager> createMemMgr() {
        return std::make_unique<SectionMemoryManager>();
      }

      JIT(std::unique_ptr<TargetMachine> TM, DataLayout DL,
          std::unique_ptr<DynamicLibrarySearchGenerator> PSGen)
        : ES(cantFail(SelfExecutorProcessControl::Create())),
          TM(std::move(TM)), DL(std::move(DL)) {
        llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
        MainJD.addGenerator(std::move(PSGen));
      }

    public:
      ~JIT() {
        if (auto Err = ES.endSession())
          ES.reportError(std::move(Err));
      }

      static Expected<std::unique_ptr<JIT>> Create() {
        auto JTMB = JITTargetMachineBuilder::detectHost();
        if (!JTMB)
          return JTMB.takeError();
        auto TM = JTMB->createTargetMachine();
        if (!TM)
          return TM.takeError();
        auto DL = (*TM)->createDataLayout();
        auto PSGen = DynamicLibrarySearchGenerator::
          GetForCurrentProcess(DL.getGlobalPrefix());
        if (!PSGen)
          return PSGen.takeError();
        return
          std::unique_ptr<JIT>(new JIT(std::move(*TM),
                                       std::move(DL),
                                       std::move(*PSGen)));
      }

      const TargetMachine &getTargetMachine() const { return *TM; }

      Error addModule(ThreadSafeModule M) {
        return CompileLayer.add(MainJD, std::move(M));
      }

      Expected<JITEvaluatedSymbol> findSymbol(const StringRef &Name) {
        return ES.lookup({&MainJD}, Mangle(Name));
      }

      Expected<JITTargetAddress> getSymbolAddress(const StringRef &Name) {
        auto Sym = findSymbol(Name);
        if (!Sym)
          return Sym.takeError();
        return Sym->getAddress();
      }
    };
  }
}


std::vector<const char*> &parse_str(char *str,
                              std::vector<const char*> &argv) {
  int32_t i = 0, n = 0, end = strlen(str);
  int32_t argc = 0, offset = argv.size();
  while (str[i] == ' ')
    i++;
  if (str[i] != '\0')
    argc = 1;
  while (str[i] != '\0') {
    if (str[i] == ' ') {
      while (str[++i] == ' ')
        ;
      if (str[i] == '\0')
        break;
      argc++;
    }
    i++;
  }
  argv.resize(offset + argc);
  i = 0;
  while (str[i] == ' ')
    i++;
  for (n = offset; n < argv.size() && i < end; n++) {
    argv[n] = &(str[i]);
    while (str[++i] != ' ' && i < end)
      ;
    if (i >= end)
      break;
    str[i] = '\0';
    while (str[++i] == ' ' && i < end)
      ;
  }
  return argv;
}

struct modulespace {
  std::unique_ptr<llvm::orc::JIT> jit;
  llvm::ExitOnError ExitOnErr;
};

struct dataspace {
  OPDS h;
  MYFLT *res;
  MYFLT *handle;
  STRINGDAT *code;
  STRINGDAT *entry;
  STRINGDAT *cflags;
  STRINGDAT *dylibs;
  modulespace *m;
};

union fltptr {
  MYFLT fl;
  modulespace *m;
};

int module_deinit(CSOUND *csound, dataspace *p) {
  delete p->m;
  return OK;
}

int module_compile(CSOUND *csound, dataspace *p) {
  char *code = p->code->data;
  const char *temp_directory = std::getenv("TMPDIR");
  if (temp_directory == nullptr) temp_directory = "/tmp";
  char srcpath[0x500];

  if(std::strcmp(p->h.optext->t.opcod, "c_module_compile"))
    std::snprintf(srcpath, 0x500, "%s/opcode_XXXXXX.cpp", temp_directory);
  else
    std::snprintf(srcpath, 0x500, "%s/opcode_XXXXXX.c", temp_directory);
  int fd = mkstemps(srcpath, 4);
  FILE* fp = fdopen(fd, "w");
  std::fprintf(fp,"%s",code);
  std::fclose(fp);
  std::vector<const char*> args;
  auto m = new modulespace;
  fltptr conv;
  conv.m = p->m = m;
  *p->res = NOTOK;
  *p->handle = FL(0.0);
  llvm::ExitOnError &ExitOnErr = m->ExitOnErr;
  args.push_back("arg0");
  args.push_back(srcpath);
  
  
  // MACOS defs
#ifdef __APPLE__
  char csfmk[128];
  char *home = getenv("HOME");
  snprintf(csfmk,128,"-I%s/Library/Frameworks/CsoundLib64.framework/Headers", home);
  
  args.push_back("-DTARGET_OS_OSX");
  args.push_back("-DTARGET_OS_IPHONE");
  args.push_back(csfmk);
  args.push_back("-I/Library/Frameworks/CsoundLib64.framework/Headers");  
  args.push_back("-I/Applications/Xcode.app/Contents/Developer"
                 "/Platforms/MacOSX.platform/Developer/SDKs"
                 "/MacOSX.sdk/usr/include/c++/v1");
  args.push_back("-I/Applications/Xcode.app/Contents/Developer/"
                 "Toolchains/XcodeDefault.xctoolchain"
                 "/usr/lib/clang/13.0.0/include/");
  args.push_back("-I/Applications/Xcode.app/Contents/Developer"
                 "/Platforms/MacOSX.platform/Developer/SDKs"
                 "/MacOSX.sdk/usr/include/");
  // Linux defs
#elif defined(__linux__)
  args.push_back("-I/usr/local/include/");
  args.push_back("-I/usr/local/include/csound");
#endif
  args.push_back("-I.");
  args.push_back("-fsyntax-only");
  args.push_back("-w");
  args.push_back("-O3");

  if(p->INOCOUNT > 2) 
    parse_str(p->cflags->data, args);
   
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticPrinter *DiagClient = new TextDiagnosticPrinter(llvm::errs(), &*DiagOpts);
  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagClient);
  const std::string TripleStr = llvm::sys::getProcessTriple();
  llvm::Triple T(TripleStr);

  // Use ELF on Windows-32 and MingW for now.
#ifndef CLANG_INTERPRETER_COFF_FORMAT
  if (T.isOSBinFormatCOFF())
    T.setObjectFormat(llvm::Triple::ELF);
#endif

  ExitOnErr.setBanner("Csound opcode compiler: ");
  Driver TheDriver(llvm::sys::fs::getMainExecutable("",
                                                    (void*)(intptr_t)
                                                    csoundModuleInfo),
                   T.str(), Diags);
  TheDriver.setTitle("Csound opcode compiler: ");
  TheDriver.setCheckInputsExist(false);

  std::unique_ptr<Compilation> C(TheDriver.BuildCompilation(args));
  if (!C) {
    csound->ErrorMsg(csound, "could not start compiler driver \n");
    return NOTOK;
  }

  const driver::JobList &Jobs = C->getJobs();
  const driver::Command &Cmd = cast<driver::Command>(*Jobs.begin());
  if (llvm::StringRef(Cmd.getCreator().getName()) != "clang") {
    Diags.Report(diag::err_fe_expected_clang_command);
    csound->ErrorMsg(csound, "could not get driver command\n");
    return NOTOK;
  }

  // Initialize a compiler invocation object from the clang (-cc1) arguments.
  const llvm::opt::ArgStringList &CCArgs = Cmd.getArguments();
  std::unique_ptr<CompilerInvocation> CI(new CompilerInvocation);
  CompilerInvocation::CreateFromArgs(*CI, CCArgs, Diags);

  // Create a compiler instance to handle the actual work.
  CompilerInstance Clang;
  Clang.setInvocation(std::move(CI));

  // Create the compilers actual diagnostics engine.
  Clang.createDiagnostics();
  if (!Clang.hasDiagnostics()) {
    csound->ErrorMsg(csound, "could not create diagnostics\n");
    return NOTOK;
  }

  // Infer the builtin include path if unspecified.
  if (Clang.getHeaderSearchOpts().UseBuiltinIncludes &&
      Clang.getHeaderSearchOpts().ResourceDir.empty())
    Clang.getHeaderSearchOpts().ResourceDir =
      CompilerInvocation::GetResourcesPath("csound",
                                           (void*) (intptr_t)
                                           csoundModuleInfo);

  // Create and execute the frontend to generate an LLVM bitcode module.
  std::unique_ptr<CodeGenAction> Act(new EmitLLVMOnlyAction());
  if (!Clang.ExecuteAction(*Act)) {
    csound->ErrorMsg(csound, "llvm bitcode not available\n");
    return NOTOK;
  }

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  std::unique_ptr<llvm::LLVMContext> Ctx(Act->takeLLVMContext());
  std::unique_ptr<llvm::Module> Module = Act->takeModule();

  if (Module){
    if(p->INOCOUNT > 3){
      std::vector<const char*> libs;  
      parse_str(p->dylibs->data, libs);
      for (auto lib : libs) 
          llvm::sys::DynamicLibrary::
            LoadLibraryPermanently(lib);
    }

    m->jit = ExitOnErr(llvm::orc::JIT::Create());
    ExitOnErr(m->jit->addModule(llvm::orc::
                                ThreadSafeModule(std::move(Module),
                                                 std::move(Ctx))));
    *p->handle = conv.fl;
    if(p->INOCOUNT > 1) 
      if(std::strcmp(p->entry->data,"")) {
        auto Main = (int (*)(CSOUND *))
          ExitOnErr(m->jit->getSymbolAddress(p->entry->data));
        *p->res = (int) Main(csound);
        return OK;
      }
  } else {
    csound->ErrorMsg(csound, "JIT module not ready\n");
    return NOTOK;
  }
  
  *p->res = 0;
  return OK;
}

#define MAXA 32
struct fcall {
  OPDS h;
  MYFLT *out[MAXA];
  MYFLT *handle;
  STRINGDAT *entry;
  MYFLT *in[VARGMAX];
};

int fcall_opcode(CSOUND *csound, fcall *p) {
  if(*p->handle == 0){
    csound->Message(csound, "invalid handle\n");
    return NOTOK;
}
  fltptr conv;
  conv.fl = *p->handle;
  auto m = conv.m;
  auto funcxx = (int (*)(CSOUND *, const OPDS &, MYFLT*[], MYFLT*[]))
    m->ExitOnErr(m->jit->getSymbolAddress(p->entry->data));
  auto func = (int (*)(CSOUND *, OPDS, MYFLT*[], MYFLT*[]))
    m->ExitOnErr(m->jit->getSymbolAddress(p->entry->data));    
  if(((strcmp(p->h.optext->t.opcod, "c_module_fcall") == 0 || 
       strcmp(p->h.optext->t.opcod, "c_module_fcallk") == 0) ?
      func(csound,p->h,p->out,p->in) :
      funcxx(csound,p->h,p->out,p->in)) == OK)
    return OK;
  else return NOTOK;
}

#include <jitplugin.h>
#include <cstdlib>

struct oobj {
  OPDS h;
  MYFLT *out[MAXA];
  MYFLT *handle;
  STRINGDAT *entry;
  MYFLT *in[VARGMAX];
  BasePlugin *obj;
};

int deinit_plugin_opcode(CSOUND *csound, oobj *p) {
   delete p->obj;
   return OK;
}

int instantiate_opcode(CSOUND *csound, oobj *p) {
  if(*p->handle != 0) {
  fltptr conv;
  conv.fl = *p->handle;
  auto m = conv.m;
  auto funcxx = (BasePlugin *(*)(OPDS))
    m->ExitOnErr(m->jit->getSymbolAddress(p->entry->data));
  p->obj = funcxx(p->h);
  if (p->obj != nullptr) {
    p->obj->csound = (csnd::Csound *) csound;
    p->obj->outargs.set(p->out);
    p->obj->outargs.size(p->OUTOCOUNT);
    p->obj->inargs.set(p->in);
    p->obj->inargs.size(p->INOCOUNT-1);
    p->obj->nsmps = CS_KSMPS;
    p->obj->offset = p->h.insdshead->ksmps_offset;
    return OK;
   }
    return NOTOK;
  } else return csound->InitError(csound, "invalid handle\n");
}

int init_plugin_opcode(CSOUND *csound, oobj *p) {
  if(instantiate_opcode(csound,p) == OK) { 
    return p->obj->init();
  } return NOTOK;
}

int perfk_plugin_opcode(CSOUND *csound, oobj *p) {
  if(p->obj != nullptr) {
    return p->obj->perf();
  } return NOTOK;
}

int perfa_plugin_opcode(CSOUND *csound, oobj *p) {
  if(p->obj != nullptr) {
    csnd::Csound *cs = (csnd::Csound *) csound;
    auto &outargs = p->obj->outargs;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t nsmps = p->obj->nsmps = p->h.insdshead->ksmps - early;
    uint32_t offset = p->obj->offset = p->h.insdshead->ksmps_offset;
    if (UNLIKELY(offset || early))
      for (auto &arg : outargs) {
        if (cs->is_asig(arg)) {
          std::fill(arg, arg + offset, 0);
          std::fill(arg + nsmps, arg + nsmps + early, 0);
        }
      }
    return p->obj->perf();
  } return NOTOK;
}


int csoundModuleCreate(CSOUND *csound) {
  csound->Message(csound, "creating clang/llvm plugin lib\n");
  return OK;
}

int csoundModuleDestroy(CSOUND *csound) {
  //llvm::llvm_shutdown();
  csound->Message(csound, "closing clang/llvm plugin lib\n");
  return OK;
}

int csoundModuleInit(CSOUND *csound){
  csound->AppendOpcode(csound, (char *) "cxx_module_compile",
                       sizeof(dataspace), 0, (char *)"ii",
                       (char *) "SW", (SUBR) module_compile, NULL, (SUBR) module_deinit);
  csound->AppendOpcode(csound, (char *) "c_module_compile",
                       sizeof(dataspace), 0,  (char *)"ii",
                       (char *) "SW", (SUBR) module_compile, NULL,(SUBR) module_deinit);
  csound->AppendOpcode(csound, (char *) "cxx_module_fcall",
                       sizeof(fcall), 0,  (char *)"********************************",
                       (char *) "iSm", (SUBR) fcall_opcode, NULL, NULL);
  csound->AppendOpcode(csound, (char *) "cxx_module_fcallk",
                       sizeof(fcall), 0, (char *)"********************************",
                       (char *) "iSM", NULL, (SUBR) fcall_opcode, NULL);
  csound->AppendOpcode(csound, (char *) "c_module_fcall",
                       sizeof(fcall), 0, (char *)"********************************",
                       (char *) "iSm", (SUBR) fcall_opcode, NULL, NULL);
  csound->AppendOpcode(csound, (char *) "c_module_fcallk",
                       sizeof(fcall), 0, (char *)"********************************",
                       (char *) "iSM", NULL, (SUBR) fcall_opcode, NULL);
  csound->AppendOpcode(csound, (char *) "cxx_opcode_ik",
                       sizeof(oobj), 0,  (char *)"********************************",
                       (char *) "iSM", (SUBR) init_plugin_opcode,
                       (SUBR) perfk_plugin_opcode, (SUBR) deinit_plugin_opcode);
  csound->AppendOpcode(csound, (char *) "cxx_opcode_ia",
                       sizeof(oobj), 0, (char *)"********************************",
                       (char *) "iSM", (SUBR) init_plugin_opcode,
                       (SUBR) perfa_plugin_opcode,  (SUBR) deinit_plugin_opcode);
  csound->AppendOpcode(csound, (char *) "cxx_opcode_i",
                       sizeof(oobj), 0,  (char *)"********************************",
                       (char *) "iSm", (SUBR) init_plugin_opcode,
                       NULL, (SUBR) deinit_plugin_opcode);
  csound->AppendOpcode(csound, (char *) "cxx_opcode_k",
                       sizeof(oobj), 0, (char *)"********************************",
                       (char *) "iSM", (SUBR) instantiate_opcode,
                       (SUBR) perfk_plugin_opcode, (SUBR) deinit_plugin_opcode);
  csound->AppendOpcode(csound, (char *) "cxx_opcode_a",
                       sizeof(oobj), 0,  (char *)"********************************",
                       (char *) "iSM", (SUBR) instantiate_opcode,
                       (SUBR) perfa_plugin_opcode, (SUBR) deinit_plugin_opcode);
  return OK;
}

int csoundModuleInfo(void){
  return ((CS_VERSION << 16) + (CS_SUBVER << 8) + (int) sizeof(MYFLT));
}
  
