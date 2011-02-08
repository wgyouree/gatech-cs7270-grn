#define DEBUG_TYPE "SimplePass"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/BasicBlock.h"
#include "llvm/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/ilist.h"
using namespace llvm;

STATISTIC(BasicBlockMax, "Counts number of basic blocks in the entire program.");
STATISTIC(BasicBlockMin, "Counts number of basic blocks in the entire program.");
STATISTIC(BasicBlockAvg, "Counts number of basic blocks in the entire program.");

STATISTIC(FunctionCounter, "Counts number of basic blocks in the entire program.");

STATISTIC(CFGEdgeMax, "Counts number of CFG edges in the entire program.");
STATISTIC(CFGEdgeMin, "Counts number of CFG edges in the entire program.");
STATISTIC(CFGEdgeAvg, "Counts number of CFG edges in the entire program.");

namespace {
// Hello - The first implementation, without getAnalysisUsage.
	struct SimplePass : public FunctionPass {
		static char ID; // Pass identification, replacement for typeid
		SimplePass() : FunctionPass(ID) {}
		
		virtual bool runOnFunction(Function &F) {
			FunctionCounter++;
			unsigned int localBBCounter = 0, localCFGCounter = 0;
			
			for (Function::iterator BBiter = F.begin();  BBiter != F.end(); ++BBiter){
				localBBCounter++;
				localCFGCounter += BBiter->getTerminator()->getNumSuccessors();
			}
			
			BasicBlockMax = (localBBCounter > BasicBlockMax) ? localBBCounter : BasicBlockMax;
			BasicBlockMin = (localBBCounter < BasicBlockMin) ? localBBCounter : BasicBlockMin;
			BasicBlockAvg = ((FunctionCounter - 1)*BasicBlockAvg + localBBCounter) / FunctionCounter;
			
			CFGEdgeMax = (localCFGCounter > CFGEdgeMax) ? localBBCounter : CFGEdgeMax;
			CFGEdgeMin = (localCFGCounter < CFGEdgeMin) ? localBBCounter : CFGEdgeMin;
			CFGEdgeAvg = ((FunctionCounter - 1)*CFGEdgeAvg + localCFGCounter) / FunctionCounter;
			
			return false;
		}
	};
}

char SimplePass::ID = 0;
INITIALIZE_PASS(SimplePass, "SimplePass", "It's a simple pass", false, false);