/*
    opcode_compile.cpp:

    Copyright (C) 2021, Victor Lazzarini

    This plugin library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

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
  } // end namespace orc
} // end namespace llvm


struct modulespace {
  std::unique_ptr<llvm::orc::JIT> jit;
};

llvm::ExitOnError ExitOnErr;

struct dataspace {
  OPDS h;
  MYFLT *res;
  STRINGDAT *code;
  STRINGDAT *entry;
};

int module_compile(CSOUND *csound, dataspace *p) {
  char *code = p->code->data;
  const char *temp_directory = std::getenv("TMPDIR");
  if (temp_directory == nullptr) temp_directory = "/tmp";
  char srcpath[0x500];
  std::snprintf(srcpath, 0x500, "%s/opcode_XXXXXX.cpp", temp_directory);
  int fd = mkstemps(srcpath, 4);
  FILE* fp = fdopen(fd, "w");
  std::fprintf(fp,"%s",code);
  std::fclose(fp);
  std::vector<const char*> args;
  args.push_back("arg0");
  args.push_back(srcpath);
  
  // MACOS defs
#ifdef __APPLE__   
  args.push_back("-DTARGET_OS_OSX");
  args.push_back("-DTARGET_OS_IPHONE");
  args.push_back("-I/Library/Frameworks/CsoundLib64.framework/Headers");
  args.push_back("-I/usr/local/lib/clang/13.0.0/include/");
  args.push_back("-I/Applications/Xcode.app/Contents/Developer"
                 "/Platforms/MacOSX.platform/Developer/SDKs"
                 "/MacOSX.sdk/usr/include/c++/v1");
  args.push_back("-I/Applications/Xcode.app/Contents/Developer"
                 "/Platforms/MacOSX.platform/Developer/SDKs"
                 "/MacOSX.sdk/usr/include/");

  // Linux defs
#elif defined(__linux__)
  args.push_back("-I/usr/local/include/");
  args.push_back("-I/usr/local/include/csound");
#endif
  args.push_back("-fsyntax-only");
  args.push_back("-w");
   
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
  Driver TheDriver(llvm::sys::fs::getMainExecutable("csound",
                                                    (void*)(intptr_t)
                                                    csoundModuleInfo),
                   T.str(), Diags);
  TheDriver.setTitle("Csound opcode compiler: ");
  TheDriver.setCheckInputsExist(false);

  std::unique_ptr<Compilation> C(TheDriver.BuildCompilation(args));
  if (!C) return NOTOK;

  const driver::JobList &Jobs = C->getJobs();
  const driver::Command &Cmd = cast<driver::Command>(*Jobs.begin());
  if (llvm::StringRef(Cmd.getCreator().getName()) != "clang") {
    Diags.Report(diag::err_fe_expected_clang_command);
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
  if (!Clang.hasDiagnostics()) return NOTOK;

  // Infer the builtin include path if unspecified.
  if (Clang.getHeaderSearchOpts().UseBuiltinIncludes &&
      Clang.getHeaderSearchOpts().ResourceDir.empty())
    Clang.getHeaderSearchOpts().ResourceDir =
      CompilerInvocation::GetResourcesPath("csound",
                                           (void*) (intptr_t)
                                           csoundModuleInfo);

  // Create and execute the frontend to generate an LLVM bitcode module.
  std::unique_ptr<CodeGenAction> Act(new EmitLLVMOnlyAction());
  if (!Clang.ExecuteAction(*Act))
    return 1;

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  std::unique_ptr<llvm::LLVMContext> Ctx(Act->takeLLVMContext());
  std::unique_ptr<llvm::Module> Module = Act->takeModule();

  auto m =
    *((modulespace **)csound->QueryGlobalVariable(csound,
                                                  "::jit_module::"));
  if(!m){
    csound->InitError(csound, "could not get module dataspace\n");
    return NOTOK;
  }
  
  if (Module){
//    std::vector<std::string> libs;
// #ifdef __APPLE__   
//   libs.push_back("/usr/lib/libstdc++.dylib");
//   // Linux defs
// #elif defined(__linux__)
//   libs.push_back("/usr/lib/gcc/x86_64-linux-gnu/9/libstdc++.so");
//   libs.push_back("/usr/lib/gcc/x86_64-linux-gnu/9/libgcc_s.so");
//   libs.push_back("/usr/lib/x86_64-linux-gnu/libm.so");
// #endif          
//  for (auto lib : libs) 
//      llvm::sys::DynamicLibrary::
//        LoadLibraryPermanently(libs.c_str());
            
    if(!m->jit){
      m->jit = ExitOnErr(llvm::orc::JIT::Create());
    }
    ExitOnErr(m->jit->addModule(llvm::orc::
                                ThreadSafeModule(std::move(Module),
                                                 std::move(Ctx))));
    if(p->INCOUNT > 1) 
      if(std::strcmp(p->entry->data,"")) {
      auto Main = (int (*)(CSOUND *))
       ExitOnErr(m->jit->getSymbolAddress(p->entry->data));
      *p->res = (int) Main(csound);
      return OK;
     }
  }
  *p->res = 0;
  return OK;
}

/* this creates the module dataspace object that holds the JIT*/
int csoundModuleCreate(CSOUND *csound) {
  if(csound->CreateGlobalVariable(csound,
                                   "::jit_module::",
                                  sizeof(modulespace*)) != 0){
    csound->Message(csound, "error creating global var\n");
    return NOTOK;
  }
  auto p = new modulespace;
  auto pp = (modulespace **)
    csound->QueryGlobalVariable(csound,"::jit_module::");                                
  *pp = p;
  return OK;
}  

/* this destroys the module dataspace object */
int csoundModuleDestroy(CSOUND *csound) {
  auto p = *((modulespace **)
             csound->QueryGlobalVariable(csound,"::jit_module::"));  
  delete p;
  return OK;
}  


int csoundModuleInit(CSOUND *csound){
  csound->AppendOpcode(csound, (char *) "module_compile",
                       sizeof(dataspace), 0, 1, (char *)"i",
                       (char *) "SW", (SUBR) module_compile, NULL, NULL);
   return OK;
}

int csoundModuleInfo(void){
  return ((CS_APIVERSION << 16) + (CS_APISUBVER << 8) + (int) sizeof(MYFLT));
}
  
