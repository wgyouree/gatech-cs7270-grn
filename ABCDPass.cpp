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
		int distance;
		ABCDNode_ *predecessor;
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

	typedef struct {
		ABCDNode *source;
		ABCDNode *target;
		int value;
	} ABCDCheck;

	ABCDCheck *createABCDCheck(ABCDNode *source, ABCDNode *target, int value) {
		ABCDCheck *check = new ABCDCheck();
		check->source = source;
		check->target = target;
		check->value = value;
		return check;
	}

	typedef struct {
		std::map<ABCDCheck *, int> *valueMap;
	} C;

	typedef struct {
		std::map<ABCDNode *, int> *valueMap;
	} active;

	C* createC() {
		return new C();
	}

	active* createActive() {
		return new active();
	}

	void addABCDCheck(C* C, ABCDCheck *check, int value) {
		C->valueMap->insert(std::pair<ABCDCheck *, int>(check, value));
	}

	ABCDCheck *getOrCreateABCDCheck(C* C, ABCDNode *u, ABCDNode *v, int value, int c) {
		ABCDCheck *check = createABCDCheck(u,v,value);
		if ( C->valueMap->find(check) == C->valueMap->end() ) {
			C->valueMap->insert(std::pair<ABCDCheck *, int>(check,c));
		}
		return check;
	}

	int getValue(C *c, ABCDCheck *check) {
		std::map<ABCDCheck *, int>::iterator result = c->valueMap->find(check);
		if ( result != c->valueMap->end() ) {
			return result->second->value;
		}
		return -1;
	}
	
	typedef struct {
		ABCDNode *source;
		ABCDNode *target;
		int weight;
	} ABCDEdge;

	ABCDEdge *createABCDEdge(ABCDNode *source, ABCDNode *target, int weight) {
		ABCDEdge *edge = new ABCDEdge();
		edge->source = source;
		edge->target = target;
		edge->weight = weight;
		return edge;
	}
}

namespace {

	struct ABCDPass : public FunctionPass{
		static char ID;

		ABCDPass() : FunctionPass(ID) {}

		virtual bool demandProve(Graph::ABCDGraph *graph, Graph::ABCDNode *index, Graph::ABCDNode *arrayLength) {
			Graph::active *active = new Graph::active();
			Graph::C *C = new Graph::C();
			int c = -1;

			if ( prove ( graph, active, C, index, arrayLength, c ) >= 0 ) {
				return true;
			}
			return false;
		}

		// use ABCD algorithm to prove redundancy
		// 1 is True
		// 0 is Reduced
		// -1 is False
		virtual int prove(Graph::ABCDGraph *graph, Graph::active *active, Graph::C *C, Graph::ABCDNode *a, Graph::ABCDNode *v, int c) {
			
			Graph::ABCDCheck *check = Graph::createABCDCheck(a, v);

			// same or stronger difference was already proven
			if ( C->valueMap->find(check) == 1 ) {
				return 1;
			}
			// same or weaker difference was already proven
			else if ( C->valueMap->find(check) == -1 ) {
				return -1;
			}
			// v is on a cycle that was reduced for same or stronger difference
			else if ( C->valueMap->find(check) == 0 ) {
				return 0;
			}

			// traversal reached the source vertex, success if a - a <= c
			if ( v == a && c >= 0 ) {
				return 1;
			}

			// if no constraint exist on the value of v, we fail
			if ( v->inList.size() == 0 ) {
				return -1;
			}

			// a cycle was encountered
			if ( active->valueMap->find(v) != NULL ) {
				if ( c > active->valueMap.find(v)->second ) {
					return -1; // an amplifying cycle
				}
				else {
					return 0; // a "harmless" cycle
				}
			}

			active->valueMap.insert(std::pair<Graph::ABCDNode *, int>(v, c) );
			
			// create set of edges for recursive part of algorithm
			std::vector<Graph::ABCDEdge *> *edges;
			std::map<Value *, Graph::ABCDNode *> *vertices = &(graph->variableList);
			for ( std::map<Value *, Graph::ABCDNode *>::iterator i = vertices->begin(); i != vertices->end(); i++ ) {
				// iterator over all outgoing edges, we are going to compare i and j vertices				
				std::map<Graph::ABCDNode * , int > outList = i->second->outList;
				Graph::ABCDNode *u = i->second;
				for ( std::map<Graph::ABCDNode *, int >::iterator j = outList->begin(); j != outList.end(); j++ ) {
					Graph::ABCDNode *v = j->first;				
					int value = j->second;
					edges->push_back(Graph::createABCDEdge(u,v,value));
				}
			}

			// TODO: finish recursive part of algorithm using set V_phi
			// if v in V_phi
				for ( std::vector<Graph::ABCDEdge *>::iterator i = edges->begin(); i != edges->end(); i++ ) {
					Graph::ABCDEdge *e = i->first;
					Graph::ABCDNode *u = e->source;
					Graph::ABCDNode *v = e->target;
					int value = e->weight;
					int d = distance(graph,u,v);
					Graph::ABCDCheck *check = Graph::getOrCreateABCDCheck(C,u,v,c);
					int prove_result = prove(graph, active, C, a, v, c - d);

					// replace existing entry
					C->valueMap.erase(check);
					// intersection
					if ( check->value == 1 && prove_result == 1 ) {
						C->valueMap.insert(std::pair<Graph::ABCDCheck *, int value>(check, prove_result));
					}
				}
			// else
				for ( std::vector<Graph::ABCDEdge *>::iterator i = edges->begin(); i != edges->end(); i++ ) {
					Graph::ABCDEdge *e = i->first;
					Graph::ABCDNode *u = e->source;
					Graph::ABCDNode *v = e->target;
					int value = e->weight;
					int d = distance(graph,u,v);
					Graph::ABCDCheck *check = C->valueMap.getOrCreateABCDCheck(u,v,c);
					int prove_result = prove(graph, active, C, a, v, c - d);

					// replace existing entry
					C->valueMap.erase(check);
					// union
					if ( check->value == 1 || prove_result == 1 ) {
						C->valueMap.insert(std::pair<Graph::ABCDCheck *, int value>(check, prove_result));
					}
				}
			

			active->valueMap.erase(v); // active[v] = NULL

			return Graph::getValue(C, check); // return C[v - a <= c]
		}

		// calculate shortest distance between source and target
		// uses Bellman-Ford algorithm - Assumes no negative cycles
		virtual int distance(Graph::ABCDGraph *graph, Graph::ABCDNode *source, Graph::ABCDNode *target) {
			
			// create map to store edges for Step 3
			std::vector<Graph::ABCDEdge *> *edges;
			
			// get reference to vertex set
			std::map<Value *, Graph::ABCDNode *> *vertices = &(graph->variableList);
			
			// Step 1: initialize Graph
			for ( std::map<Value *, Graph::ABCDNode *>::iterator i = vertices->begin(); i != vertices->end(); i++ ) {
				Graph::ABCDNode *v = i->second;
				if ( v == source ) {
					v->distance = 0;
				}
				else {
					// infinity
					v->distance = 1000000000;
				}
				v->predecessor = NULL;
			}

			// Step 2: relax edges repeatedly
			for ( std::map<Value *, Graph::ABCDNode *>::iterator i = vertices->begin(); i != vertices->end(); i++ ) {
				// iterator over all outgoing edges, we are going to compare i and j vertices				
				std::map<Graph::ABCDNode * , int > outList = i->second->outList;
				Graph::ABCDNode *u = i->second;
				for ( std::map<Graph::ABCDNode *, int >::iterator j = outList.begin(); j != outList.end(); j++ ) {
					Graph::ABCDNode *v = j->first;				
					int value = j->second;
					if ( u->distance + value < v->distance ) {
						v->distance = u->distance + value;
						v->predecessor = u;
					}
					// create temporary set of edges for Step 3
					edges->push_back(Graph::createABCDEdge(u,v,value));
				}
			}
			
			// Step 3: check for negative-weight cycles
			for ( std::vector<Graph::ABCDEdge*>::iterator i = edges->begin(); i != edges.end(); i++ ) {
				Graph::ABCDNode *u = i->first->source;
				Graph::ABCDNode *v = i->first->target;
				int value = i->first->weight;
				if ( u->distance + value < v->distance ) {
					cerr << "Error, Graph contains a negative-weight cycle";
				}
			}

			// calculate distance between source and target
			int result = 0;
			Graph::ABCDNode *current = target;
			while ( current != source ) {
				result += current->distance;
				current = current->predecessor;
			}
			return result;
		}

		virtual bool runOnFunction(Function &F){
			char *EXITNAME = "exitBlock";
			char *PIFUNCNAME = "pifunction_";
			char *CHECKINST = "abcdcmpinst";
			Graph::ABCDGraph *inequalityGraph = new Graph::ABCDGraph();
			std::vector<Instruction *> arrayCheckInstList;
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
				if(isa<AllocaInst>(*I)){
					AllocaInst *ai = (AllocaInst *)&*I;
					if (ai->getAllocatedType()->isArrayTy()){
						int NumElements = ((const ArrayType *)ai->getAllocatedType())->getNumElements();
						Graph::getOrInsertNode(inequalityGraph, (Value *)ai, NumElements);
					}
				} else if (isa<ICmpInst>(*I)){
					if ((*I).getName().startswith(StringRef(CHECKINST)))
						arrayCheckInstList.push_back(&*I);
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
								//weight of the edge = cons - arraylength
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
//			for (std::map<Value *, Graph::ABCDNode *>::iterator AI = inequalityGraph->variableList.begin(),
//					AE = inequalityGraph->variableList.end(); AI != AE; ++AI){
//				AI->first->dump();
//			}
			for (std::vector<Instruction* >::iterator CI = arrayCheckInstList.begin(),
					CE = arrayCheckInstList.end(); CI != CE; ++CI){
				User *checkInst = (User *)(*CI);
				Graph::ABCDNode *source;
				int length;
				source = Graph::getOrInsertNode(inequalityGraph, checkInst->getOperand(0), 0);
				length = cast<ConstantInt>(*(checkInst->getOperand(1))).getSExtValue();
				for (std::map<Value *, Graph::ABCDNode* >::iterator ANI = inequalityGraph->arrayLengthList.begin(),
						ANE = inequalityGraph->arrayLengthList.end(); ANI != ANE; ++ANI){
					if (ANI->second->length == length){
						if (true){//Graph::isRedundant(source, ANI->second)){
							// delete the instruction and break.
							BasicBlock *parent = (*CI)->getParent(), *nextBlock;
							nextBlock = parent->getTerminator()->getSuccessor(1);
							if (nextBlock->getName().equals(StringRef(EXITNAME)))
								nextBlock = parent->getTerminator()->getSuccessor(0);
							(*CI)->eraseFromParent();
							BranchInst *unCondBrInst = BranchInst::Create(nextBlock);
							llvm::ReplaceInstWithInst(parent->getTerminator(), unCondBrInst);
							break;
						}
					}
				}
			}

			

			

			return true;
		}
	};

	char ABCDPass::ID = 0;
	RegisterPass<ABCDPass> X("abcd", "Redundant Check Elimination");
}
