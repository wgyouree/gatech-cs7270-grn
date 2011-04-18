#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/BasicBlock.h"
#include "llvm/CallGraphSCCPass.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/CFG.h"
#include "llvm/ADT/StringExtras.h"
//#include "llvm/Support/Streams.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/InstrTypes.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <set>
#include <vector>
#include <list>
#include "llvm/ADT/Twine.h"
#include "graph.cpp"

using namespace llvm;

namespace {

	struct ABCDPass : public FunctionPass{
		static char ID;

		ABCDPass() : FunctionPass(ID) {}

		virtual bool runOnFunction(Function &F){
			Graph::ABCDGraph *inequalityGraph = new Graph::ABCDGraph();
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
				if(isa<AllocaInst>(*I)){
					AllocaInst *ai = (AllocaInst *)&*I;
					if (ai->getAllocatedType()->isArrayTy()){
						int NumElements = ((const ArrayType *)ai->getAllocatedType())->getNumElements();
						getOrInsertNode(inequalityGraph, (Value *)ai, NumElements);
					}
				}
			}
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
				if (isa<PHINode>(*I)){
					PHINode *phi = (PHINode *)(&*I);
					Graph::ABCDNode *res = getOrInsertNode(inequalityGraph, (Value *)phi, 0);
					std::map<Value*, Graph::ABCDNode* > *arrayLengthPtr = &(inequalityGraph->arrayLengthList);
					for (int i = 0, total = phi->getNumIncomingValues(); i < total; i++){
						Value *inVal = phi->getIncomingValue(i);
						if (isa<Constant>(*inVal)){
							if (!isa<ConstantInt>(*inVal))
								continue;
							ConstantInt *cons = (ConstantInt *)inVal;
							for (std::map<Value*, Graph::ABCDNode* >::iterator AI = (*arrayLengthPtr).begin(),
							   AE = (*arrayLengthPtr).end(); AI != AE; ++AI){
//								//weight of the edge = cons - arraylength
								insertEdge((*AI).second, res, cons->getSExtValue() - (*AI).second->length);
							}
							continue;
						}
						Graph::ABCDNode *in = getOrInsertNode(inequalityGraph, inVal, 0);
						insertEdge(in, res, 0);
					}
				}
			}
			return true;
		}
	};

	char ABCDPass::ID = 0;
	RegisterPass<ABCDPass> X("abcd", "Redundant Check Elimination");
}
