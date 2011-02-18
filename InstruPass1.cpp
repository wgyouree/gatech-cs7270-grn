#define DEBUG_TYPE "Instru_Pass1"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include <stdio.h>

using namespace llvm;

STATISTIC(FunctionCounter, "Number of functions including library functions");

STATISTIC(EdgeCounter, "Number of Call Graph edges");

namespace {
  struct InstruPass1 : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    InstruPass1() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
	
	CallGraph &CG = getAnalysis<CallGraph>();
	//CG.print(errs(),&M);

     for(df_iterator<CallGraph*> CG_iterB = df_begin<CallGraph*>(&CG), CG_iterE = df_end<CallGraph*>(&CG); CG_iterB != CG_iterE; ++CG_iterB){
      	if ((*CG_iterB)->getFunction() /*&& ((*CG_iterB)->getFunction()->isDeclaration() && (*CG_iterB)->getFunction()->isIntrinsic())*/){
      		FunctionCounter++;
      		EdgeCounter = EdgeCounter + (*CG_iterB)->size();
      	}else{
      		EdgeCounter -= (*CG_iterB)->getNumReferences();
      	}
	      
      //	(*CG_iterB)->print(errs());
     }
      
      return false;
    }

    // We don't modify the program, so we preserve all analyses
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<CallGraph>(); 
	AU.setPreservesAll();
    }
    
  };
}

char InstruPass1::ID = 0;

//static RegisterPass<InstruPass1> X("instruPass1", "InstruPass1", false, false);


INITIALIZE_PASS(InstruPass1, "instruPass1",
                "Instrumentation pass 1 using callgraph",
                false, false);	
