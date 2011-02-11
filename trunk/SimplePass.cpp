#define DEBUG_TYPE "SimplePass"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/BasicBlock.h"
#include "llvm/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/ilist.h"
#include "llvm/Analysis/Dominators.h"
using namespace llvm;

// TODO: Need to initialize the MIN variables with some large value (INF)

STATISTIC(BasicBlockMax, "Counts number of basic blocks in the entire program.");
STATISTIC(BasicBlockMin, "Counts number of basic blocks in the entire program.");
STATISTIC(BasicBlockAvg, "Counts number of basic blocks in the entire program.");

STATISTIC(FunctionCounter, "Counts number of basic blocks in the entire program.");

STATISTIC(CFGEdgeMax, "Counts number of CFG edges in the entire program.");
STATISTIC(CFGEdgeMin, "Counts number of CFG edges in the entire program.");
STATISTIC(CFGEdgeAvg, "Counts number of CFG edges in the entire program.");

STATISTIC(DomOutMax, "Counts number of dominated nodes.");
STATISTIC(DomOutMin, "Counts number of dominated nodes.");
STATISTIC(DomOutAvg, "Counts number of dominated nodes.");

STATISTIC(DomInMax, "Counts number of nodes dominating.");
STATISTIC(DomInMin, "Counts number of nodes dominating.");
STATISTIC(DomInAvg, "Counts number of nodes dominating.");

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
			
			DominatorTree &domTree = getAnalysis<DominatorTree>();

			for (Function::iterator BBiter = F.begin();  BBiter != F.end(); ++BBiter){
				DomTreeNode *currNode = domTree.getNode(BBiter);
				DomOutMin = (DomOutMin < currNode->getDFSNumOut()) ? DomOutMin : currNode->getDFSNumOut();
				DomOutMax = (DomOutMax > currNode->getDFSNumOut()) ? DomOutMax : currNode->getDFSNumOut();
				DomOutAvg = DomOutAvg + (currNode->getDFSNumOut()/localBBCounter);
			
				DomInMin = (DomInMin < currNode->getDFSNumIn()) ? DomInMin : currNode->getDFSNumIn();
				DomInMax = (DomInMax > currNode->getDFSNumIn()) ? DomInMax : currNode->getDFSNumIn();
				DomInAvg = DomInAvg + (currNode->getDFSNumIn()/localBBCounter);				
			}
			
			return false;
		}

	        // We don't modify the program, so we preserve all analyses
	        virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<DominatorTree>();      
			AU.setPreservesAll();
	        }
	};
}

char SimplePass::ID = 0;
INITIALIZE_PASS(SimplePass, "SimplePass", "It's a simple pass", false, false);
