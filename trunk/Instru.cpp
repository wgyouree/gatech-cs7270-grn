#define DEBUG_TYPE "CallGraph"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include <stdio.h>

using namespace llvm;

STATISTIC(FunctionCounter, "Counts number of functions");

STATISTIC(EdgeCounter, "Counts number of Call Graph edges");

namespace {
  struct Instru : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    Instru() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
	
	CallGraph &CG = getAnalysis<CallGraph>();
        //CG.print(errs(),&M);
	//printf("CG has %i nodes",CG.size());
//	printf("test");
      for(df_iterator<CallGraph*> CG_iterB = df_begin <CallGraph*> (&CG), CG_iterE = df_end <CallGraph*> (&CG); CG_iterB != CG_iterE; ++CG_iterB){
      	FunctionCounter++;
      	EdgeCounter = EdgeCounter + (*CG_iterB)->size();
      	(*CG_iterB)->print(errs());
      }
	fprintf(stderr, "test");
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
