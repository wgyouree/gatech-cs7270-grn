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
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/InstrTypes.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
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
		bool isPhi;
		ABCDNode_ *predecessor;
		int name;
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
		newNode->isPhi = false;
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
	

	//Structures to enable graph distance computation
	struct ABCDCheck_ {
		ABCDNode *source;
		ABCDNode *target;
		int value;
		
		bool operator<(const struct ABCDCheck_& a)const;
	};
	typedef struct ABCDCheck_ ABCDCheck;
	
	//Overload to use map class with ABCDCheck as key
	bool ABCDCheck::operator<(const ABCDCheck& a)const{
		if(this->value < a.value || this->source < a.source || this->target < a.target)	
			return true;
		return false;
	}
	


	ABCDCheck *createABCDCheck(ABCDNode *source, ABCDNode *target, int value) {
		ABCDCheck *check = new ABCDCheck();
		check->source = source;
		check->target = target;
		check->value = value;
		return check;
	}

	typedef struct {
		std::map<ABCDCheck, int> valueMap;
	} C;

	typedef struct {
		std::map<ABCDNode *, int> valueMap;
	} active;

	C* createC() {
		return new C();
	}

	active* createActive() {
		return new active();
	}

	void addABCDCheck(C* C, ABCDCheck check, int value) {
		(C->valueMap).insert(std::pair<ABCDCheck, int>(check, value));
	}

	ABCDCheck* getABCDCheck(C* C, ABCDNode *u, ABCDNode *v, int value) {
		ABCDCheck *check = createABCDCheck(u,v,value);
		std::map<ABCDCheck, int>::iterator existingCheck = C->valueMap.find(*check);
		if ( existingCheck == C->valueMap.end() ) {
			return NULL;
		}
		return (ABCDCheck*)(&(existingCheck->first));
	}

	int getValue(C *c, ABCDCheck check) {
		std::map<ABCDCheck, int>::iterator result = c->valueMap.find(check);
		if ( result != c->valueMap.end() ) {
			return result->second;
		}
		return -1;
	}
	
/*	typedef struct {
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
*/	
}

namespace {

	struct ABCDPass : public FunctionPass{
		static char ID;

		ABCDPass() : FunctionPass(ID) {}

		bool demandProve(Graph::ABCDGraph *graph, Graph::ABCDNode *arrayLength, Graph::ABCDNode *index) {
			//errs() << "Demand Prove Called\n";
			Graph::active *active = new Graph::active();
			Graph::C *C = new Graph::C();
			int c = -1;

			if ( prove ( graph, active, C, arrayLength, index, c ) >= 0 ) {
				return true;
			}
			return false;
		}

		void printContentsOfActive(Graph::active *active) {
			errs() << "Active contains: " << active->valueMap.size() << " entries:\n";
			for ( std::map<Graph::ABCDNode *, int>::iterator i = active->valueMap.begin(); i != active->valueMap.end(); i++ ) {
				errs() << i->second << "\n";
			}
			errs() << "\n\n";
		}

		void printContentsOfC(Graph::C *c) {
			errs() << "C contains: " << c->valueMap.size() << " entries:\n";
			for ( std::map<Graph::ABCDCheck, int>::iterator i = c->valueMap.begin(); i != c->valueMap.end(); i++ ) {
				errs() << i->second << "\n";
			}
			errs() << "\n\n";
		}
		
		int meetOperation(int x, int y, int meetOp){
			if (meetOp == 0){
				// min operator
				return (x >= y) ? x : y;
			} else
				return (x <= y) ? x : y;
		}
		
		// use ABCD algorithm to prove redundancy
		// 1 is True
		// 0 is Reduced
		// -1 is False
		int prove(Graph::ABCDGraph *graph, Graph::active *active, Graph::C *C, Graph::ABCDNode *a, Graph::ABCDNode *v, int c) {

			Graph::ABCDCheck *check1 = Graph::createABCDCheck(a, v, -1);
			Graph::ABCDCheck *check2 = Graph::createABCDCheck(a, v, 0);
			Graph::ABCDCheck *check3 = Graph::createABCDCheck(a, v, 1);

			//printContentsOfActive(active);
			//printContentsOfC(C);

			bool hasCheck1 = C->valueMap.find(*check1) == C->valueMap.end();
			bool hasCheck2 = C->valueMap.find(*check2) == C->valueMap.end();
			bool hasCheck3 = C->valueMap.find(*check3) == C->valueMap.end();

			if ( c == -1 ) {
				// same or stronger difference was already proven
				if ( hasCheck1 ) {
					int result = C->valueMap.find(*check1)->second;
					if ( result == 1 ) {
						//errs() << "same or stronger difference was already proven\n";
						return 1;
					}
				}
				// same or weaker difference was already proven
				if ( hasCheck1 || hasCheck2 || hasCheck3 ) {
					bool result = false;
					if ( hasCheck1 && C->valueMap.find(*check1)->second == -1 ) {
						result = true;
					}
					else if ( hasCheck2 && C->valueMap.find(*check2)->second == -1 ) {
						result = true;
					}
					else if ( hasCheck3 && C->valueMap.find(*check3)->second == -1 ) {
						result = true;
					}
					if ( result == true ) {
						//errs() << "same or weaker difference was already proven\n";
						return -1;
					}
				}
				// v is on a cycle that was reduced for same or stronger difference
				if ( hasCheck1 ) {
					int result = C->valueMap.find(*check1)->second;
					if ( result == 0 ) {
						//errs() << "v is on a cycle that was reduced for same or stronger difference\n";
						return 0;
					}
				}
			}
			else if ( c == 0 ) {
				// same or stronger difference was already proven
				if ( hasCheck1 || hasCheck2 ) {
					bool result = false;
					if ( hasCheck1 && C->valueMap.find(*check1)->second == 1 ) {
						result = true;
					}
					else if ( hasCheck2 && C->valueMap.find(*check2)->second == 1 ) {
						result = true;
					}
					if ( result == true ) {
						//errs() << "same or stronger difference was already proven\n";
						return 1;
					}
				}
				// same or weaker difference was already proven
				if ( hasCheck2 || hasCheck3 ) {
					bool result = false;
					if ( hasCheck2 && C->valueMap.find(*check2)->second == -1 ) {
						result = true;
					}
					else if ( hasCheck3 && C->valueMap.find(*check3)->second == -1 ) {
						result = true;
					}
					if ( result == true) {
						//errs() << "same or weaker difference was already proven\n";
						return -1;
					}
				}
				// v is on a cycle that was reduced for same or stronger difference
				if ( hasCheck1 || hasCheck2 ) {
					bool result = false;
					if ( hasCheck1 && C->valueMap.find(*check1)->second == 0 ) {
						result = true;
					}
					else if ( hasCheck2 && C->valueMap.find(*check2)->second == 0 ) {
						result = true;
					}
					if ( result == true ) {
						//errs() << "v is on a cycle that was reduced for same or stronger difference\n";
						return 0;
					}
				}

			}
			else if ( c == 1 ) {
				// same or stronger difference was already proven
				if ( hasCheck1 || hasCheck2 || hasCheck3 ) {
					bool result = false;
					if ( hasCheck1 && C->valueMap.find(*check1)->second == 1 ) {
						result = true;
					}
					else if ( hasCheck2 && C->valueMap.find(*check2)->second == 1 ) {
						result = true;
					}
					else if ( hasCheck3 && C->valueMap.find(*check3)->second == 1 ) {
						result = true;
					}
					if ( result == true ) {
						//errs() << "same or stronger difference was already proven\n";
						return 1;
					}
				}
				// same or weaker difference was already proven
				if ( hasCheck3 ) {
					int result = C->valueMap.find(*check3)->second;
					if ( result == -1 ) {
						//errs() << "same or weaker difference was already proven\n";
						return -1;
					}
				}
				// v is on a cycle that was reduced for same or stronger difference
				if ( hasCheck1 || hasCheck2 || hasCheck3 ) {
					bool result = false;
					if ( hasCheck1 && C->valueMap.find(*check1)->second == 0 ) {
						result = true;
					}
					else if ( hasCheck2 && C->valueMap.find(*check2)->second == 0 ) {
						result = true;
					}
					else if ( hasCheck3 && C->valueMap.find(*check3)->second == 0 ) {
						result = true;
					}
					if ( result == true ) {
						//errs() << "v is on a cycle that was reduced for same or stronger difference\n";
						return 0;
					}
				}
			}

			// traversal reached the source vertex, success if a - a <= c
			if ( v == a && c >= 0 ) {
				//errs() << "traversal reached the source vertex, success if a - a <= c\n";
				return 1;
			}

			// if no constraint exist on the value of v, we fail
			if ( v->inList.size() == 0 ) {
				//errs() << "if no constraint exist on the value of v, we fail\n";
				return -1;
			}
			

			// a cycle was encountered
			if ( active->valueMap.find(v) != active->valueMap.end() ) {
				if ( c > active->valueMap.find(v)->second ) {
					//errs() << "an amplifying cycle\n";
					return -1; // an amplifying cycle
				}
				else {
					//errs() << "a harmless cycle\n";
					return 0; // a "harmless" cycle
				}
			}
			
			//errs() << "added a value to Active\n";
			active->valueMap.insert(std::pair<Graph::ABCDNode *, int>(v, c) );
			
/*			// create set of edges for recursive part of algorithm
			std::vector<Graph::ABCDEdge *> edges;
			std::map<Value *, Graph::ABCDNode *> vertices = graph->variableList;
			for ( std::map<Value *, Graph::ABCDNode *>::iterator i = vertices.begin(); i != vertices.end(); i++ ) {
				// iterator over all outgoing edges, we are going to compare i and j vertices				
				std::map<Graph::ABCDNode * , int > outList = i->second->outList;
				Graph::ABCDNode *u = i->second;
				for ( std::map<Graph::ABCDNode *, int >::iterator j = outList.begin(); j != outList.end(); j++ ) {
					Graph::ABCDNode *v = j->first;				
					int value = j->second;
					edges.push_back(Graph::createABCDEdge(u,v,value));
				}
			}
*/
			
			Graph::ABCDCheck *check = Graph::createABCDCheck(a, v, c);

			if ( v->isPhi == true ) {
				//errs() << "v is Phi node\n";
				for(std::map<Graph::ABCDNode * , int>::iterator i = (v->inList).begin(); i != (v->inList).end(); i++){
					Graph::ABCDNode *u = i->first;
					int d = i->second;
					int prove_result = prove(graph, active, C, a, i->first, c - d);
					Graph::ABCDCheck* existingCheck = Graph::getABCDCheck(C,a,v,c);
					
				
					if(existingCheck == NULL){
						C->valueMap.insert(std::pair<Graph::ABCDCheck, int>(*check,prove_result));
					}else{
						int existingValue = getValue(C, *existingCheck);
						int meetResult = meetOperation(prove_result,existingValue,1);
						C->valueMap.erase(*check);
						C->valueMap.insert(std::pair<Graph::ABCDCheck, int>(*check,meetResult));	
					}
					
				}
			}
			else {
				//errs() << "v is Non-Phi node\n";
				for(std::map<Graph::ABCDNode * , int>::iterator i = (v->inList).begin(); i != (v->inList).end(); i++){
					Graph::ABCDNode *u = i->first;
					int d = i->second;
					int prove_result = prove(graph, active, C, a, i->first, c - d);
					Graph::ABCDCheck* existingCheck = Graph::getABCDCheck(C,a,v,c);
					
				
					if(existingCheck == NULL){
						C->valueMap.insert(std::pair<Graph::ABCDCheck, int>(*check,prove_result));
					}else{
						int existingValue = getValue(C, *existingCheck);
						int meetResult = meetOperation(prove_result,existingValue,0);
						C->valueMap.erase(*check);
						C->valueMap.insert(std::pair<Graph::ABCDCheck, int>(*check,meetResult));	
					}
					
				}
			}
			
			active->valueMap.erase(v); // active[v] = NULL

			return Graph::getValue(C, *check); // return C[v - a <= c]
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
					res->isPhi = true;
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
							//if (!I->getParent()->getTerminator()->getSuccessor(0)->getName().startswith(StringRef("bounds")))						
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
			
			/* Delete redundant check instructions */
			std::vector<Instruction* > toDeleteList;
			for (std::vector<Instruction* >::iterator CI = arrayCheckInstList.begin(),
					CE = arrayCheckInstList.end(); CI != CE; ++CI){
				User *checkInst = (User *)(*CI);
				Graph::ABCDNode *source;
				int length;
				source = Graph::getOrInsertNode(inequalityGraph, checkInst->getOperand(0), 0);
				length = cast<ConstantInt>(*(checkInst->getOperand(1))).getSExtValue();
				for (std::map<Value *, Graph::ABCDNode* >::iterator ANI = inequalityGraph->arrayLengthList.begin(),
						ANE = inequalityGraph->arrayLengthList.end(); ANI != ANE; ++ANI){
					//errs() << "checking to see if demand prove is needed\n";
					if (ANI->second->length == length){
						if (demandProve(inequalityGraph, ANI->second, source)){//Graph::isRedundant(source, ANI->second)){
							// delete the instruction and break.
							BasicBlock *parent = (*CI)->getParent(), *nextBlock;
							nextBlock = parent->getTerminator()->getSuccessor(1);
							if (nextBlock->getName().equals(StringRef(EXITNAME)))
								nextBlock = parent->getTerminator()->getSuccessor(0);
							toDeleteList.push_back(*CI);
							BranchInst *unCondBrInst = BranchInst::Create(nextBlock);
							llvm::ReplaceInstWithInst(parent->getTerminator(), unCondBrInst);
							break;
						}
					}
				}
			}
			errs() << "Number of array checks: " << arrayCheckInstList.size() << "\n";
			errs() << "Redundant array checks: " << toDeleteList.size() << "\n";
			while (!toDeleteList.empty()){
				Instruction *cur = toDeleteList.back();
				toDeleteList.pop_back();
				cur->eraseFromParent();
			}

			return true;
		}
		
		
/*		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<PromotePass>(); 
//			AU.setPreservesAll();
    		    }
*/    		    
	};

	char ABCDPass::ID = 0;
	RegisterPass<ABCDPass> X("abcd", "Redundant Check Elimination");
}
