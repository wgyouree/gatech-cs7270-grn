#define DEBUG_TYPE "InstruPass2"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/BasicBlock.h"
#include "llvm/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/ilist.h"
#include "llvm/Analysis/Dominators.h"
#include <set>
using namespace llvm;

#define INF 1000

STATISTIC(BasicBlockMax, "Max number of basic blocks in a function.");
STATISTIC(BasicBlockMin, "Min number of basic blocks in a function.");
STATISTIC(BasicBlockAvg, "Avg number of basic blocks in a function.");

STATISTIC(FunctionCounter, "Number of functions in the entire program excluding library functions.");

STATISTIC(CFGEdgeMax, "Max number of CFG edges in a function.");
STATISTIC(CFGEdgeMin, "Min number of CFG edges in a function.");
STATISTIC(CFGEdgeAvg, "Avg number of CFG edges in a function.");

STATISTIC(DomOutAvg, "Avg number of dominated nodes in a function.");

STATISTIC(DomInAvg, "Avg number of nodes dominating in a function.");

STATISTIC(LoopMax, "Max number of single entry loops in a function.");
STATISTIC(LoopMin, "Min number of single entry loops in a function.");
STATISTIC(LoopAvg, "Avg number of single entry loops in a function.");

STATISTIC(LoopBlockMax, "Max number of single entry loop blocks in a function.");
STATISTIC(LoopBlockMin, "Min number of single entry loop blocks in a function.");
STATISTIC(LoopBlockAvg, "Avg number of single entry loop blocks in a function.");

namespace {
	struct InstruPass2 : public FunctionPass {
		static char ID; // Pass identification, replacement for typeid
		static unsigned int DomOutCum,DomInCum, BasicBlockCum, CFGEdgeCum, LoopCum, LoopBlockCum;
		
		InstruPass2() : FunctionPass(ID) {		
		}
		
		virtual bool runOnFunction(Function &F) {
			FunctionCounter++;
			if (FunctionCounter == 1){
				BasicBlockMin = INF;
				CFGEdgeMin = INF;
				LoopMin = INF;
				LoopBlockMin = INF;
			}
			
			unsigned int localBBCounter = 0, localCFGCounter = 0;
			
			for (Function::iterator BBiter = F.begin();  BBiter != F.end(); ++BBiter){
				localBBCounter++;
				localCFGCounter += BBiter->getTerminator()->getNumSuccessors();
			}
			
			BasicBlockMax = (localBBCounter > BasicBlockMax) ? localBBCounter : BasicBlockMax;
			BasicBlockMin = (localBBCounter < BasicBlockMin) ? localBBCounter : BasicBlockMin;
			BasicBlockCum += localBBCounter;
			BasicBlockAvg = BasicBlockCum / FunctionCounter;
			
			CFGEdgeMax = (localCFGCounter > CFGEdgeMax) ? localBBCounter : CFGEdgeMax;
			CFGEdgeMin = (localCFGCounter < CFGEdgeMin) ? localBBCounter : CFGEdgeMin;
			CFGEdgeCum += localCFGCounter;
			CFGEdgeAvg = CFGEdgeCum / FunctionCounter;
			
			DominatorTree &domTree = getAnalysis<DominatorTree>();
			
			// Count DomIn, DomOut
			DomTreeNode *currNode = domTree.getNode(&(F.getEntryBlock()));
			
			// DomInMin has to be 0; no need to compute.
			// And DomOutMin also has to be 0 (Leaf nodes of the dominator tree) 
			unsigned int totalDomOut = 0, totalDomIn = 0;
			recursiveDomCount(currNode, 1, &totalDomOut, &totalDomIn);
			
			DomOutCum += totalDomOut;
			DomInCum += totalDomIn;
			DomOutAvg = DomOutCum/BasicBlockCum;
			DomInAvg = DomInCum/BasicBlockCum;

			//Count Loops
			std::set<BasicBlock*> globalSeenLoopBlocks;
			unsigned int localLoopCounter = 0, localLoopBlockCounter = 0;
			
			for (Function::iterator BBiter = F.begin();  BBiter != F.end(); ++BBiter){
				int numSuccessors = BBiter->getTerminator()->getNumSuccessors();
				for(int successorID = 0; successorID < numSuccessors; successorID++)
				{
					BasicBlock* successor = BBiter->getTerminator()->getSuccessor(successorID);
					if(domTree.dominates(successor,BBiter))
					{
						std::set<BasicBlock*> seenLoopBlocks;
						localLoopCounter++;
						localLoopBlockCounter += loopBlocksCount(BBiter,successor,seenLoopBlocks, globalSeenLoopBlocks);
					}
				}
			}

			LoopMax = (localLoopCounter > LoopMax) ? localLoopCounter : LoopMax;
			LoopMin = (localLoopCounter < LoopMin) ? localLoopCounter : LoopMin;
			LoopCum += localLoopCounter;
			LoopAvg = LoopCum / FunctionCounter;
			//LoopAvg = ((FunctionCounter - 1)*LoopAvg + localLoopCounter) / FunctionCounter;
			
			LoopBlockMax = (localLoopBlockCounter > LoopBlockMax) ? localLoopBlockCounter : LoopBlockMax;
			LoopBlockMin = (localLoopBlockCounter < LoopBlockMin) ? localLoopBlockCounter : LoopBlockMin;
			LoopBlockCum += localLoopBlockCounter;
			LoopBlockAvg = LoopBlockCum / FunctionCounter;
			//LoopBlockAvg = ((FunctionCounter - 1)*LoopBlockAvg + localLoopBlockCounter) / FunctionCounter;
			
			return false;
		}

	        virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<DominatorTree>();      
			//AU.setPreservesAll();
	        }

			unsigned int recursiveDomCount(DomTreeNode *currNode, unsigned int DomIn, unsigned int *totalDomOut, unsigned int *totalDomIn){
				unsigned int DomOut = 0;
				const std::vector<DomTreeNodeBase<BasicBlock> *> &children = currNode->getChildren();
				if (children.size() == 0)
					DomOut = 1;
				unsigned int i;
				for (i = 0; i < children.size(); i++){
					DomOut += recursiveDomCount(children[i], DomIn + 1, totalDomOut, totalDomIn);
				}
				*totalDomIn += DomIn;
				*totalDomOut += DomOut;
				return DomOut;
			}
			
			unsigned int loopBlocksCount(BasicBlock* lastBlock, BasicBlock* startBlock, std::set<BasicBlock*> seenLoopBlocks, std::set<BasicBlock*>& globalSeenLoopBlocks){
				
				unsigned int numBlocks = 1;
				
				if((globalSeenLoopBlocks.find(lastBlock)) != globalSeenLoopBlocks.end())
				{
					numBlocks = 0;
				}
					
				else
					globalSeenLoopBlocks.insert(lastBlock);
					
				if((seenLoopBlocks.find(lastBlock)) != seenLoopBlocks.end())
					return 0;
				
				
				if(lastBlock == startBlock)
					return numBlocks;
					
				seenLoopBlocks.insert(lastBlock);
				
					
				for (pred_iterator PI = pred_begin(lastBlock);  PI != pred_end(lastBlock); ++PI){
					numBlocks += loopBlocksCount(*PI, startBlock,seenLoopBlocks, globalSeenLoopBlocks);			
				}
				return numBlocks;
			}
	};
}

char InstruPass2::ID = 0;
unsigned int InstruPass2::DomOutCum = 0;
unsigned int InstruPass2::DomInCum = 0;
unsigned int InstruPass2::BasicBlockCum = 0;
unsigned int InstruPass2::CFGEdgeCum = 0;
unsigned int InstruPass2::LoopCum = 0;
unsigned int InstruPass2::LoopBlockCum = 0;
INITIALIZE_PASS(InstruPass2, "phase0-rmangal3-2", "Instrumentation Pass 2 using CFG", false, false);
