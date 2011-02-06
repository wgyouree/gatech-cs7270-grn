#define DEBUG_TYPE "hello"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CallGraph.h"

using namespace llvm;

STATISTIC(HelloCounter, "Counts number of functions greeted");

namespace {
  struct Instru : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    Instru() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
	CallGraph &CG = getAnalysis<CallGraph>();
      CG.print(errs(),&M);
      return false;
    }

    // We don't modify the program, so we preserve all analyses
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<CallGraph>();      
	AU.setPreservesAll();
    }
  };
}

char Instru::ID = 0;

static RegisterPass<Instru> X("instru_test", "Instru Pass", false, false);


//INITIALIZE_PASS(Instru, "intru_test",
//                "Instru Pass (with getAnalysisUsage implemented)",
//                false, false);	
