//===--- Haiku.cpp - Haiku ToolChain Implementations --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Haiku.h"
#include "Arch/ARM.h"
#include "Arch/Mips.h"
#include "Arch/Sparc.h"
#include "CommonArgs.h"
#include "clang/Config/config.h" // C_INCLUDE_DIRS
#include "clang/Driver/Compilation.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "clang/Driver/SanitizerArgs.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang::driver::toolchains;
using namespace clang;
using namespace llvm::opt;

void haiku::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                   const InputInfo &Output,
                                   const InputInfoList &Inputs,
                                   const ArgList &Args,
                                   const char *LinkingOutput) const {
  const toolchains::Haiku &ToolChain =
      static_cast<const toolchains::Haiku &>(getToolChain());
  const Driver &D = ToolChain.getDriver();
  //bool IsNoPIC = Args.hasArg(options::OPT_fno_pic, options::OPT_fno_PIC);
  ArgStringList CmdArgs;

  // Silence warning for "clang -g foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_g_Group);
  // and "clang -emit-llvm foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_emit_llvm);
  // and for "clang -w foo.o -o foo". Other warning options are already
  // handled somewhere else.
  Args.ClaimAllArgs(options::OPT_w);

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));
/*
  if (!IsNoPIC)
    CmdArgs.push_back("-fpic");
*/
  CmdArgs.push_back("--eh-frame-hdr");
  if (Args.hasArg(options::OPT_static)) {
    CmdArgs.push_back("-Bstatic");
  } else {
    if (Args.hasArg(options::OPT_rdynamic))
      CmdArgs.push_back("-export-dynamic");
    if (Args.hasArg(options::OPT_shared)) {
      CmdArgs.push_back("-Bshareable");
    }
    // we don't yet support "new" dtags (e.g DT_RUNPATH...)
    CmdArgs.push_back("--disable-new-dtags");
  }

  if (Arg *A = Args.getLastArg(options::OPT_G)) {
    if (ToolChain.getTriple().isMIPS()) {
      StringRef v = A->getValue();
      CmdArgs.push_back(Args.MakeArgString("-G" + v));
      A->claim();
    }
  }

  CmdArgs.push_back("-shared");
  if (Args.hasArg(options::OPT_shared)) {
    //CmdArgs.push_back("-e 0");
  } else {
    CmdArgs.push_back("-no-undefined");
  }

  if (Output.isFilename()) {
    CmdArgs.push_back("-o");
    CmdArgs.push_back(Output.getFilename());
  } else {
    assert(Output.isNothing() && "Invalid output.");
  }

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles,
                   options::OPT_r)) {
    const char *crt1 = nullptr;
    if (!Args.hasArg(options::OPT_shared)) {
      crt1 = "start_dyn.o";
    }

    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtbeginS.o")));
    if (crt1)
      CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath(crt1)));
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("init_term_dyn.o")));
  }

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  ToolChain.AddFilePathLibArgs(Args, CmdArgs);
  Args.AddAllArgs(CmdArgs, options::OPT_T_Group);
  Args.AddAllArgs(CmdArgs, options::OPT_e);
  Args.AddAllArgs(CmdArgs, options::OPT_s);
  Args.AddAllArgs(CmdArgs, options::OPT_t);
  Args.AddAllArgs(CmdArgs, options::OPT_Z_Flag);
  Args.AddAllArgs(CmdArgs, options::OPT_r);

  if (D.isUsingLTO()) {
    assert(!Inputs.empty() && "Must have at least one input.");
    addLTOOptions(ToolChain, Args, CmdArgs, Output, Inputs[0],
                  D.getLTOMode() == LTOK_Thin);
  }

  addLinkerCompressDebugSectionsOption(ToolChain, Args, CmdArgs);
  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs,
                   options::OPT_r)) {
    // Use the static OpenMP runtime with -static-openmp
    bool StaticOpenMP = Args.hasArg(options::OPT_static_openmp) &&
                        !Args.hasArg(options::OPT_static);
    addOpenMPRuntime(CmdArgs, ToolChain, Args, StaticOpenMP);

    if (D.CCCIsCXX()) {
      if (ToolChain.ShouldLinkCXXStdlib(Args))
        ToolChain.AddCXXStdlibLibArgs(Args, CmdArgs);
    }

    CmdArgs.push_back("-lgcc");

    if (Args.hasArg(options::OPT_static)) {
      CmdArgs.push_back("-lgcc_eh");
    } else {
      CmdArgs.push_back("--push-state");
      CmdArgs.push_back("--as-needed");
      CmdArgs.push_back("-lgcc_s");
      CmdArgs.push_back("--no-as-needed");
      CmdArgs.push_back("--pop-state");
    }

    CmdArgs.push_back("-lroot");
    CmdArgs.push_back("-lgcc");

    if (Args.hasArg(options::OPT_static)) {
      CmdArgs.push_back("-lgcc_eh");
    } else {
      CmdArgs.push_back("--push-state");
      CmdArgs.push_back("--as-needed");
      CmdArgs.push_back("-lgcc_s");
      CmdArgs.push_back("--no-as-needed");
      CmdArgs.push_back("--pop-state");
    }
  }

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles,
                   options::OPT_r)) {
    if (Args.hasArg(options::OPT_shared) /*|| IsPIE*/)
      CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtendS.o")));
    else
      CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtend.o")));
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtn.o")));
  }

  ToolChain.addProfileRTLibs(Args, CmdArgs);

  const char *Exec = Args.MakeArgString(getToolChain().GetLinkerPath());
  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
}

/// Haiku - Haiku tool chain which can call as(1) and ld(1) directly.

Haiku::Haiku(const Driver &D, const llvm::Triple &Triple,
                 const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {

  GCCInstallation.init(Triple, Args);

  path_list &Paths = getFilePaths();
  addPathIfExists(D, GCCInstallation.getInstallPath(),Paths);
  addPathIfExists(D, "/boot/system/non-packaged/develop/lib/",Paths);
  addPathIfExists(D, "/boot/system/develop/lib",Paths);
}

ToolChain::CXXStdlibType Haiku::GetDefaultCXXStdlibType() const {
  return ToolChain::CST_Libstdcxx;
}

unsigned Haiku::GetDefaultDwarfVersion() const {
  // Haiku Debugger supports DWARFVersion up to 3;
  return 3;
}

void Haiku::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                              ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc))
    return;

  if (!DriverArgs.hasArg(options::OPT_nobuiltininc)) {
    SmallString<128> Dir(getDriver().ResourceDir);
    llvm::sys::path::append(Dir, "include");
    addSystemInclude(DriverArgs, CC1Args, Dir.str());
  }

  if (DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/app");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/device");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/drivers");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/game");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/interface");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/kernel");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/locale");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/mail");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/media");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/midi");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/midi2");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/net");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/opengl");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/storage");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/support");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/translation");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/add-ons/graphics");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/add-ons/input_server");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/add-ons/mail_daemon");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/add-ons/registrar");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/add-ons/screen_saver");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/add-ons/tracker");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/be_apps/NetPositive");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/os/be_apps/Tracker");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/bsd");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/glibc");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/gnu");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/posix");
  addSystemInclude(DriverArgs, CC1Args, "/boot/system/develop/headers/");

//  if (auto Val = llvm::sys::Process::GetEnv("BEINCLUDES")) {
//    SmallVector<StringRef, 8> Dirs;
//    StringRef(*Val).split(Dirs, ";", /*MaxSplit=*/-1, /*KeepEmpty=*/false);
//    if (!Dirs.empty())
//      addSystemIncludes(DriverArgs, CC1Args, Dirs);
//  }

  // Check for configure-time C include directories.
  StringRef CIncludeDirs(C_INCLUDE_DIRS);
  if (CIncludeDirs != "") {
    SmallVector<StringRef, 5> dirs;
    CIncludeDirs.split(dirs, ":");
    for (StringRef dir : dirs) {
      StringRef Prefix =
          llvm::sys::path::is_absolute(dir) ? StringRef(getDriver().SysRoot) : "";
      addExternCSystemInclude(DriverArgs, CC1Args, Prefix + dir);
    }
    return;
  }

  return;
}


void Haiku::addLibCxxIncludePaths(const llvm::opt::ArgList &DriverArgs,
                                    llvm::opt::ArgStringList &CC1Args) const {
  addSystemInclude(DriverArgs, CC1Args,
                   getDriver().SysRoot + "/system/develop/headers/c++/v1");
}

// is this override neccessary ?
// GCCInstallation provide valid path (but! into '/boot/system/develop/tools/lib/gcc/x86_64-unknown-haiku/11.2.0/include/c++')
// dup paths ?
void Haiku::addLibStdCxxIncludePaths(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  addLibStdCXXIncludePaths(getDriver().SysRoot + "/system/develop/headers/c++",
                           getTriple().str(), "", DriverArgs, CC1Args);
}

void Haiku::AddCXXStdlibLibArgs(const ArgList &Args,
                                  ArgStringList &CmdArgs) const {
  CXXStdlibType Type = GetCXXStdlibType(Args);

  switch (Type) {
  case ToolChain::CST_Libcxx:
    CmdArgs.push_back("-lc++");
    break;

  case ToolChain::CST_Libstdcxx:
    CmdArgs.push_back("-lstdc++");
    break;
  }
}

void Haiku::AddCudaIncludeArgs(const ArgList &DriverArgs,
                                 ArgStringList &CC1Args) const {
  CudaInstallation.AddCudaIncludeArgs(DriverArgs, CC1Args);
}

void Haiku::AddHIPIncludeArgs(const ArgList &DriverArgs,
                                ArgStringList &CC1Args) const {
  RocmInstallation.AddHIPIncludeArgs(DriverArgs, CC1Args);
}

Tool *Haiku::buildLinker() const { return new tools::haiku::Linker(*this); }

bool Haiku::HasNativeLLVMSupport() const { return true; }

bool Haiku::IsUnwindTablesDefault(const ArgList &Args) const { return true; }

bool Haiku::isPICDefault() const { return true; }

bool Haiku::isPIEDefault(const llvm::opt::ArgList &Args) const { return false;}

bool Haiku::GetDefaultStandaloneDebug() const { return true; }
