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
//#include "llvm/Transforms/Utils/ABCDGraph.h"

using namespace llvm;

namespace Graph{

	struct ABCDNode_{
		llvm::Value *value;
		int length; // 0 => not an array length node.
		std::map<struct ABCDNode_* , int> inList;
		std::map<struct ABCDNode_* , int > outList;
	};
	typedef struct ABCDNode_ ABCDNode;

	typedef std::pair<llvm::Value*, ABCDNode* > ValNodePair;
	typedef std::pair<ABCDNode* , int > NodeIntPair;

	typedef struct{
		std::map<llvm::Value*, ABCDNode* > arrayLengthList;
	   	std::map<llvm::Value*, ABCDNode* > variableList;
	}ABCDGraph;
	
	//So the graph is essentially just a list(vector) of nodes.
	//Helper functions that needed to be implemented:

	ABCDNode *createNode(llvm::Value *value, int length){
		ABCDNode *newNode;
		newNode = new ABCDNode();
		newNode->value = value;
		newNode->length = length;
		return newNode;
	}

	/*
	As the name suggests, get the node if already present in the list,
	(search on the Value*) or create a node with empty edge lists and 
	the given value and return it.
	If value isa<AllocaInst> insert into arrayLengthList with the input length,
	else in the variableList.
	*/
	ABCDNode *getOrInsertNode(ABCDGraph *graph, llvm::Value *value, int length){
		std::map<llvm::Value*, ABCDNode* > *list;
		std::map<llvm::Value*, ABCDNode* >::iterator it;
		list = (length == 0) ? &(graph->variableList) : &(graph->arrayLengthList);
		it = list->find(value);
		if (it != list->end())
			return it->second;
		ABCDNode *newNode = createNode(value, length);
		list->insert(ValNodePair(value, newNode));
		return newNode;
	}

	/*
	Create two edges - out edge for n1 and in edge for n2.
	*/
	void insertEdge(ABCDNode *n1, ABCDNode *n2, int weight){
		std::map<ABCDNode* , int >::iterator it;
		it = n1->outList.find(n2);
		if (it == n1->outList.end())
			n1->outList.insert(NodeIntPair(n2, weight));
		it = n2->inList.find(n1);
		if (it == n2->inList.end())
			n2->inList.insert(NodeIntPair(n1, weight));
	}
}

namespace {

	struct ABCDPass : public FunctionPass{
		static char ID;

		ABCDPass() : FunctionPass(ID) {}

		virtual bool runOnFunction(Function &F){
			char *PIFUNCNAME = "pifunction_";
			Graph::ABCDGraph *inequalityGraph = new Graph::ABCDGraph();
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
				if(isa<AllocaInst>(*I)){
					AllocaInst *ai = (AllocaInst *)&*I;
					if (ai->getAllocatedType()->isArrayTy()){
						int NumElements = ((const ArrayType *)ai->getAllocatedType())->getNumElements();
						Graph::getOrInsertNode(inequalityGraph, (Value *)ai, NumElements);
					}
				}
			}
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
				if (isa<PHINode>(*I)){
					PHINode *phi = (PHINode *)(&*I);
					Graph::ABCDNode *res = Graph::getOrInsertNode(inequalityGraph, (Value *)phi, 0);
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
								Graph::insertEdge((*AI).second, res, cons->getSExtValue() - (*AI).second->length);
							}
							continue;
						}
						Graph::ABCDNode *in = Graph::getOrInsertNode(inequalityGraph, inVal, 0);
						Graph::insertEdge(in, res, 0);
					}
				}else if(isa<BinaryOperator>(*I)){
                    int op = I->getOpcode();
                    if(op == Instruction::Add || op == Instruction::Sub || op == Instruction::FAdd || op == Instruction::FSub){
                        Value* otherOperand;
                        ConstantInt* constOperand;
                        
                        int opType = ( (op == Instruction::Add || op == Instruction::FAdd) )?1:-1;
                        if(isa<ConstantInt>(*(I->getOperand(0)))){
                            constOperand = cast<ConstantInt>(I->getOperand(0));
                            otherOperand = I->getOperand(1);
                        }else if(isa<ConstantInt>(*(I->getOperand(1)))){
                            constOperand = cast<ConstantInt>(I->getOperand(1));
                            otherOperand = I->getOperand(0);
                        }else
                            continue;
                        
                        Graph::ABCDNode* nodeFrom = Graph::getOrInsertNode(inequalityGraph,otherOperand,0);
                        Graph::ABCDNode* nodeTo = Graph::getOrInsertNode(inequalityGraph,(Value*)(&*I),0);
                        Graph::insertEdge(nodeFrom,nodeTo,(opType * constOperand->getSExtValue()));
                    }
                }else if(isa<CallInst>(*I)){
                    //Deal with pair of PI functions here
                    inst_iterator temp_I = I;
                    Graph::ABCDNode* nodeTo[2];
                    int i = 0, cntPI = 0;
                    do{
                        if(cast<CallInst>(&*temp_I)->getCalledFunction()->getName().startswith(StringRef(PIFUNCNAME))){ 
                        
                            Graph::ABCDNode* nodeFrom = Graph::getOrInsertNode(inequalityGraph,cast<CallInst>(&*temp_I)->getArgOperand(0),0);
                            nodeTo[cntPI] = Graph::getOrInsertNode(inequalityGraph,((Value*)&*temp_I),0);
                            Graph::insertEdge(nodeFrom,nodeTo[cntPI],0);
                            i--;
                            cntPI++;
                        }
                        temp_I++;
                        i++;
                    }while(isa<CallInst>(*temp_I) && i == 0);
                    if(cntPI>0){
                        int isInTrueBlock = -1;
                        BranchInst* brI = cast<BranchInst>(I->getParent()->getSinglePredecessor()->getTerminator());
                        if(brI->getSuccessor(0) == I->getParent())
                            isInTrueBlock = 1;
                        
                        CmpInst* cmpI = cast<CmpInst>(brI->getCondition());
                        int predicate = cmpI->getPredicate();
                        int predicateClass;
                    
                        if(predicate == CmpInst::FCMP_OLT || predicate == CmpInst::FCMP_ULT || predicate == CmpInst::ICMP_ULT || predicate == CmpInst::ICMP_SLT){
                            predicate = 1;
                            predicateClass = 1;
                        
                        }else if(predicate == CmpInst::FCMP_OEQ ||predicate == CmpInst::FCMP_OLE || predicate == CmpInst::FCMP_ULE || predicate == CmpInst::FCMP_UEQ || predicate == CmpInst::ICMP_ULE || predicate == CmpInst::ICMP_SLE || predicate == CmpInst::ICMP_EQ){
                            predicate = 1;
                            predicateClass = 2;
                            
                        }else if(predicate == CmpInst::FCMP_OGT || predicate == CmpInst::FCMP_UGT || predicate == CmpInst::ICMP_UGT || predicate == CmpInst::ICMP_SGT){
                            predicate = -1;
                            predicateClass = -1;
                                
                        }else if(predicate == CmpInst::FCMP_OGE || predicate == CmpInst::FCMP_UGE || predicate == CmpInst::ICMP_UGE || predicate == CmpInst::ICMP_SGE){
                            predicate = -1;
                            predicateClass = -2;
                        
                        }else{
                            predicate = 0;
                        }
                        
                        
                        Value *operand1, *operand2;
                        int operandLoc;
                        
                        if(cntPI == 1){
                            if(isa<Constant>(*(cmpI->getOperand(0)))){
                                operand1 = cmpI->getOperand(1);
                                operand2 = cmpI->getOperand(0);
                                operandLoc = -1;
                            }else{
                                operand1 = cmpI->getOperand(0);
                                operand2 = cmpI->getOperand(1);
                                operandLoc = 1;
                            }
                            
                            if(isInTrueBlock * predicate * operandLoc > 0)//Ignore cases where variable is greater than constant since we
                                                            //are only considering upper bound checksin this case
                                for(std::map<Value*, Graph::ABCDNode* >::iterator it = (inequalityGraph->arrayLengthList).begin(); it !=(inequalityGraph->arrayLengthList).end(); ++it){
                                    if(predicateClass == 1)
                                        Graph::insertEdge(it->second,nodeTo[0],(cast<ConstantInt>(operand2)->getSExtValue() - 1 - it->second->length));
                                    else
                                        Graph::insertEdge(it->second,nodeTo[0],(cast<ConstantInt>(operand2)->getSExtValue() - it->second->length));
                                
                                }
                    
                        }else{//cntPI == 2
                            I++;
                            if(isInTrueBlock == 1 &&  predicate == 1){
                                if(predicateClass == 1)
                                    Graph::insertEdge(nodeTo[1], nodeTo[0], -1);
                                else
                                    Graph::insertEdge(nodeTo[1], nodeTo[0], 0);
                            }
                            else if(isInTrueBlock == 1 &&  predicate == -1){
                                if(predicateClass == -1)
                                    Graph::insertEdge(nodeTo[0], nodeTo[1], -1);
                                else
                                    Graph::insertEdge(nodeTo[0], nodeTo[1], 0);
                            }    
                            else if(isInTrueBlock == -1 &&  predicate == 1){
                                predicate = cmpI->getPredicate();
                                if(predicate == CmpInst::FCMP_OLT || predicate == CmpInst::FCMP_ULT || predicate == CmpInst::ICMP_ULT || predicate == CmpInst::ICMP_SLT){
                                    Graph::insertEdge(nodeTo[0], nodeTo[1], 0);
                                }else if(predicate == CmpInst::FCMP_OLE || predicate == CmpInst::FCMP_ULE || predicate == CmpInst::ICMP_ULE || predicate == CmpInst::ICMP_SLE){
                                    Graph::insertEdge(nodeTo[0], nodeTo[1], -1);
                                }    
                            }else if(isInTrueBlock == -1 &&  predicate == -1){
                                predicate = cmpI->getPredicate();
                                if(predicate == CmpInst::FCMP_OGT || predicate == CmpInst::FCMP_UGT || predicate == CmpInst::ICMP_UGT || predicate == CmpInst::ICMP_SGT){
                                    Graph::insertEdge(nodeTo[1], nodeTo[0], 0);
                                }else if(predicate == CmpInst::FCMP_OGE || predicate == CmpInst::FCMP_UGE || predicate == CmpInst::ICMP_UGE || predicate == CmpInst::ICMP_SGE){
                                    Graph::insertEdge(nodeTo[1], nodeTo[0], -1);
                                }
                            }
                        }
                    }
                }
			}
			for (std::map<Value *, Graph::ABCDNode *>::iterator AI = inequalityGraph->variableList.begin(),
					AE = inequalityGraph->variableList.end(); AI != AE; ++AI){
				AI->first->dump();
			}
			return true;
		}
	};

	char ABCDPass::ID = 0;
	RegisterPass<ABCDPass> X("abcd", "Redundant Check Elimination");
}
