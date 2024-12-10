//===-- clang-format/ClangFormat.cpp - Clang format tool ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements a clang-format tool that automatically formats
/// (fragments of) C++ code.
///
//===----------------------------------------------------------------------===//

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Format/Format.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Process.h"
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <fstream>

using namespace llvm;
using clang::tooling::Replacements;
namespace {
enum class WNoError { Unknown };
}

namespace clang {
namespace format {

static FileID createFile(StringRef FileName, MemoryBufferRef Source,
                                 SourceManager &Sources, FileManager &Files,
                                 llvm::vfs::FileSystem *FS) {
  std::string path = FS->getCurrentWorkingDirectory().get() + "/" + FileName.str();
  std::ofstream file(path);
  file << Source.getBuffer().str();
  file.close();
  auto File = Files.getOptionalFileRef(FileName);
  assert(File && "File not added to FS?");
  return Sources.createFileID(*File, SourceLocation(), SrcMgr::C_User);
}

class ClangFormatDiagConsumer : public DiagnosticConsumer {
  virtual void anchor() {}

  void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                        const Diagnostic &Info) override {

    SmallVector<char, 16> vec;
    Info.FormatDiagnostic(vec);
    errs() << "clang-format error:" << vec << "\n";
  }
};
std::string Style =
    clang::format::DefaultFallbackStyle; // TODO: make this configurable
std::string FallbackStyle = clang::format::DefaultFallbackStyle;
std::string AssumeFileName = "<stdin>";
// Returns true on error.
std::string format(const std::string &source) {
  // On Windows, overwriting a file with an open file mapping doesn't work,
  // so read the whole file into memory when formatting in-place.
  ErrorOr<std::unique_ptr<MemoryBuffer>> CodeOrErr =
      MemoryBuffer::getMemBuffer(source);
  // MemoryBuffer::getFileOrSTDIN(FileName, /*IsText=*/true);
  std::unique_ptr<llvm::MemoryBuffer> Code = std::move(CodeOrErr.get());
  if (Code->getBufferSize() == 0)
    return "";

  StringRef BufStr = Code->getBuffer();

  std::vector<tooling::Range> Ranges;
  StringRef AssumedFileName = AssumeFileName;

  Expected<FormatStyle> FormatStyle = getStyle(
      Style, AssumedFileName, FallbackStyle, Code->getBuffer(), nullptr, true);
  if (!FormatStyle) {
    llvm::errs() << toString(FormatStyle.takeError()) << "\n";
    return "";
  }

  StringRef QualifierAlignmentOrder = ""; // TODO: make this configurable

  FormatStyle->QualifierAlignment =
      StringSwitch<FormatStyle::QualifierAlignmentStyle>(
          QualifierAlignmentOrder.lower())
          .Case("right", FormatStyle::QAS_Right)
          .Case("left", FormatStyle::QAS_Left)
          .Default(FormatStyle->QualifierAlignment);

  if (FormatStyle->QualifierAlignment == FormatStyle::QAS_Left) {
    FormatStyle->QualifierOrder = {"const", "volatile", "type"};
  } else if (FormatStyle->QualifierAlignment == FormatStyle::QAS_Right) {
    FormatStyle->QualifierOrder = {"type", "const", "volatile"};
  } else if (QualifierAlignmentOrder.contains("type")) {
    FormatStyle->QualifierAlignment = FormatStyle::QAS_Custom;
    SmallVector<StringRef> Qualifiers;
    QualifierAlignmentOrder.split(Qualifiers, " ", /*MaxSplit=*/-1,
                                  /*KeepEmpty=*/false);
    FormatStyle->QualifierOrder = {Qualifiers.begin(), Qualifiers.end()};
  }
  FormatStyle->SortIncludes = FormatStyle::SI_CaseSensitive;
  unsigned CursorPosition = 0;
  Replacements Replaces = sortIncludes(*FormatStyle, Code->getBuffer(), Ranges,
                                       AssumedFileName, &CursorPosition);
  // To format JSON insert a variable to trick the code into thinking its
  // JavaScript.
  if (FormatStyle->isJson() && !FormatStyle->DisableFormat) {
    auto Err = Replaces.add(tooling::Replacement(
        tooling::Replacement(AssumedFileName, 0, 0, "x = ")));
    if (Err)
      llvm::errs() << "Bad Json variable insertion\n";
  }
  auto ChangedCode = tooling::applyAllReplacements(Code->getBuffer(), Replaces);
  if (!ChangedCode) {
    llvm::errs() << toString(ChangedCode.takeError()) << "\n";
    return "";
  }
  // Get new affected ranges after sorting `#includes`.
  Ranges = tooling::calculateRangesAfterReplacements(Replaces, Ranges);
  FormattingAttemptStatus Status;
  Replacements FormatChanges =
      reformat(*FormatStyle, *ChangedCode, Ranges, AssumedFileName, &Status);
  Replaces = Replaces.merge(FormatChanges);
  IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS = llvm::vfs::getRealFileSystem();
  auto error_code = FS->setCurrentWorkingDirectory("/tmp");
  // IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> FS(new vfs::InMemoryFileSystem);
  
  FileManager Files(FileSystemOptions(), FS);

  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts(new DiagnosticOptions());
  ClangFormatDiagConsumer IgnoreDiagnostics;
  DiagnosticsEngine Diagnostics(
      IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs), &*DiagOpts,
      &IgnoreDiagnostics, false);
  SourceManager Sources(Diagnostics, Files);
  FileID ID = createFile(AssumedFileName, *Code, Sources, Files, FS.get());
  // FileID ID = createInMemoryFile(AssumedFileName, *Code, Sources, Files, FS.get());
  Rewriter Rewrite(Sources, LangOptions());
  tooling::applyAllReplacements(Replaces, Rewrite);
  std::string Result;
  llvm::raw_string_ostream results(Result);
  Rewrite.getEditBuffer(ID).write(results);
  return Result;
}

} // namespace format
} // namespace clang

std::string format(const std::string &source) {
  return clang::format::format(source);
}