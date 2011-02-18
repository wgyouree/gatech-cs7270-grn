#define DEBUG_TYPE "SimplePass"
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

// TODO: Need to initialize the MIN variables with some large value (INF)


namespace {
	struct DeadCodeRemoval : public FunctionPass {
		static char ID; // Pass identification, replacement for typeid
		DeadCodeRemoval() : FunctionPass(ID) {}
		
		virtual bool runOnFunction(Function &F) {
			// remove dead code
			std::set<BasicBlock*> visitedNodes;
			std::vector<BasicBlock*> deadNodes;
			BasicBlock &entryBlock = F.getEntryBlock();
			
			recursiveVisit(entryBlock, visitedNodes);
			
			errs() << visitedNodes.size() << "\n" ;
			
			for (Function::iterator BBiter = F.begin(); BBiter != F.end(); ++BBiter) {
				if ( visitedNodes.find((BasicBlock*)BBiter) == visitedNodes.end() ) {
					BasicBlock* temp = (BasicBlock*) BBiter;
					deadNodes.push_back(temp);
				}
			}
			
			for(unsigned int i = 0; i<deadNodes.size(); i++){
				deadNodes[i]->eraseFromParent();
			}
			
			return (deadNodes.size());
		}

	        // We don't modify the program, so we preserve all analyses
	        virtual void getAnalysisUsage(AnalysisUsage &AU) const {
//			AU.addRequired<DominatorTree>();      
			//AU.setPreservesAll();
	        }
	        
	        void recursiveVisit(BasicBlock &bb, std::set<BasicBlock*> &visited){
				int i, numSuccessors;
				BasicBlock* child;
				visited.insert(&bb);
				numSuccessors = bb.getTerminator()->getNumSuccessors();
				for (i = 0; i < numSuccessors; i++){
					child = bb.getTerminator()->getSuccessor(i);
					//if (child->getUniquePredecessor() == bb)
					if(visited.find(child) == visited.end())
						recursiveVisit(*child, visited);
				}
			}
			

	};
}

char DeadCodeRemoval::ID = 0;
INITIALIZE_PASS(DeadCodeRemoval, "deadCodeRemoval", "Dead Code Removal pass", false, false);
