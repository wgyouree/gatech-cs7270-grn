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

namespace Graph2{

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
		//static bool runOnce;

		ABCDPass() : FunctionPass(ID) {
			//runOnce = false;
		}

		bool testDemandProve() {
			Graph2::ABCDGraph *graph = new Graph2::ABCDGraph();
			
			std::map<llvm::Value*, Graph2::ABCDNode* > arrayLengthList;
			std::map<llvm::Value*, Graph2::ABCDNode* > variableList;

			Graph2::ABCDNode *minusOne = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, minusOne));
			
			Graph2::ABCDNode *st0 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, st0));
			
			Graph2::ABCDNode *st1 = Graph2::createNode(NULL, 0);
			st1->isPhi = true;
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, st1));

			Graph2::ABCDNode *st2 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, st2));

			Graph2::ABCDNode *st3 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, st3));

			Graph2::ABCDNode *i0 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, i0));

			Graph2::ABCDNode *i1 = Graph2::createNode(NULL, 0);
			i1->isPhi = true;
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, i1));

			Graph2::ABCDNode *i2 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, i2));

			Graph2::ABCDNode *i3 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, i3));

			Graph2::ABCDNode *i4 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, i4));

			Graph2::ABCDNode *t0 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, t0));

			Graph2::ABCDNode *AdotLength = Graph2::createNode(NULL, 0);
			arrayLengthList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, AdotLength));

			Graph2::ABCDNode *limit0 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, limit0));

			Graph2::ABCDNode *limit1 = Graph2::createNode(NULL, 0);
			limit1->isPhi = true;
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, limit1));

			Graph2::ABCDNode *limit2 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, limit2));

			Graph2::ABCDNode *limit3 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, limit3));

			Graph2::ABCDNode *limit4 = Graph2::createNode(NULL, 0);
			variableList.insert(std::pair<llvm::Value*, Graph2::ABCDNode *>(NULL, limit4));

			Graph2::insertEdge ( minusOne, st0, 0 );
			Graph2::insertEdge ( st0, st1, 0 );
			Graph2::insertEdge ( st1, st2, 0 );
			Graph2::insertEdge ( st3, st1, 0 );
			Graph2::insertEdge ( st2, st3, 1 );
			Graph2::insertEdge ( limit2, st2, -1 );
			Graph2::insertEdge ( limit1, limit2, 0 );
			Graph2::insertEdge ( limit0, limit1, 0 );
			Graph2::insertEdge ( limit3, limit1, 0 );
			Graph2::insertEdge ( limit2, limit3, -1 );
			Graph2::insertEdge ( AdotLength, limit0, 0 );
			Graph2::insertEdge ( limit3, limit4, 0 );
			Graph2::insertEdge ( limit4, i2, -1 );
			Graph2::insertEdge ( i2, i3, 0 );
			Graph2::insertEdge ( i3, t0, 1 );
			Graph2::insertEdge ( AdotLength, i3, -1 );
			Graph2::insertEdge ( i3, i4, 1 );
			Graph2::insertEdge ( i4, i1, 0 );
			Graph2::insertEdge ( i1, i2, 0 );
			Graph2::insertEdge ( st3, i0, 0 );
			Graph2::insertEdge ( i0, i1, 0 );
			
			
			// result should be true
			return demandProve(graph, AdotLength, i2);
		}

		bool demandProve(Graph2::ABCDGraph *graph, Graph2::ABCDNode *arrayLength, Graph2::ABCDNode *index) {
			//errs() << "Demand Prove Called\n";
			Graph2::active *active = new Graph2::active();
			Graph2::C *C = new Graph2::C();
			int c = -1;

			if ( prove ( graph, active, C, arrayLength, index, c ) >= 0 ) {
				return true;
			}
			return false;
		}

		void printContentsOfActive(Graph2::active *active) {
			errs() << "Active contains: " << active->valueMap.size() << " entries:\n";
			for ( std::map<Graph2::ABCDNode *, int>::iterator i = active->valueMap.begin(); i != active->valueMap.end(); i++ ) {
				errs() << i->second << "\n";
			}
			errs() << "\n\n";
		}

		void printContentsOfC(Graph2::C *c) {
			errs() << "C contains: " << c->valueMap.size() << " entries:\n";
			for ( std::map<Graph2::ABCDCheck, int>::iterator i = c->valueMap.begin(); i != c->valueMap.end(); i++ ) {
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
		int prove(Graph2::ABCDGraph *graph, Graph2::active *active, Graph2::C *C, Graph2::ABCDNode *a, Graph2::ABCDNode *v, int c) {

			Graph2::ABCDCheck *check1 = Graph2::createABCDCheck(a, v, -1);
			Graph2::ABCDCheck *check2 = Graph2::createABCDCheck(a, v, 0);
			Graph2::ABCDCheck *check3 = Graph2::createABCDCheck(a, v, 1);

			printContentsOfActive(active);
			printContentsOfC(C);

			bool hasCheck1 = C->valueMap.find(*check1) == C->valueMap.end();
			bool hasCheck2 = C->valueMap.find(*check2) == C->valueMap.end();
			bool hasCheck3 = C->valueMap.find(*check3) == C->valueMap.end();

			if ( c == -1 ) {
				// same or stronger difference was already proven
				if ( hasCheck1 ) {
					int result = C->valueMap.find(*check1)->second;
					if ( result == 1 ) {
						errs() << "same or stronger difference was already proven\n";
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
						errs() << "same or weaker difference was already proven\n";
						return -1;
					}
				}
				// v is on a cycle that was reduced for same or stronger difference
				if ( hasCheck1 ) {
					int result = C->valueMap.find(*check1)->second;
					if ( result == 0 ) {
						errs() << "v is on a cycle that was reduced for same or stronger difference\n";
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
						errs() << "same or stronger difference was already proven\n";
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
						errs() << "same or weaker difference was already proven\n";
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
						errs() << "v is on a cycle that was reduced for same or stronger difference\n";
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
						errs() << "same or stronger difference was already proven\n";
						return 1;
					}
				}
				// same or weaker difference was already proven
				if ( hasCheck3 ) {
					int result = C->valueMap.find(*check3)->second;
					if ( result == -1 ) {
						errs() << "same or weaker difference was already proven\n";
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
						errs() << "v is on a cycle that was reduced for same or stronger difference\n";
						return 0;
					}
				}
			}

			// traversal reached the source vertex, success if a - a <= c
			if ( v == a && c >= 0 ) {
				errs() << "traversal reached the source vertex, success if a - a <= c\n";
				return 1;
			}

			// if no constraint exist on the value of v, we fail
			if ( v->inList.size() == 0 ) {
				errs() << "if no constraint exist on the value of v, we fail\n";
				return -1;
			}
			

			// a cycle was encountered
			if ( active->valueMap.find(v) != active->valueMap.end() ) {
				if ( c > active->valueMap.find(v)->second ) {
					errs() << "an amplifying cycle\n";
					return -1; // an amplifying cycle
				}
				else {
					errs() << "a harmless cycle\n";
					return 0; // a "harmless" cycle
				}
			}
			
			errs() << "added a value to Active\n";
			active->valueMap.insert(std::pair<Graph2::ABCDNode *, int>(v, c) );
			
/*			// create set of edges for recursive part of algorithm
			std::vector<Graph2::ABCDEdge *> edges;
			std::map<Value *, Graph2::ABCDNode *> vertices = graph->variableList;
			for ( std::map<Value *, Graph2::ABCDNode *>::iterator i = vertices.begin(); i != vertices.end(); i++ ) {
				// iterator over all outgoing edges, we are going to compare i and j vertices				
				std::map<Graph2::ABCDNode * , int > outList = i->second->outList;
				Graph2::ABCDNode *u = i->second;
				for ( std::map<Graph2::ABCDNode *, int >::iterator j = outList.begin(); j != outList.end(); j++ ) {
					Graph2::ABCDNode *v = j->first;				
					int value = j->second;
					edges.push_back(Graph2::createABCDEdge(u,v,value));
				}
			}
*/
			
			Graph2::ABCDCheck *check = Graph2::createABCDCheck(a, v, c);

			if ( v->isPhi == true ) {
				errs() << "v is Phi node\n";
				for(std::map<Graph2::ABCDNode * , int>::iterator i = (v->inList).begin(); i != (v->inList).end(); i++){
					Graph2::ABCDNode *u = i->first;
					int d = i->second;
					int prove_result = prove(graph, active, C, a, i->first, c - d);
					Graph2::ABCDCheck* existingCheck = Graph2::getABCDCheck(C,a,v,c);
					
				
					if(existingCheck == NULL){
						C->valueMap.insert(std::pair<Graph2::ABCDCheck, int>(*check,prove_result));
					}else{
						int existingValue = getValue(C, *existingCheck);
						int meetResult = meetOperation(prove_result,existingValue,1);
						C->valueMap.erase(*check);
						C->valueMap.insert(std::pair<Graph2::ABCDCheck, int>(*check,meetResult));	
					}
					
				}
			}
			else {
				errs() << "v is Non-Phi node\n";
				for(std::map<Graph2::ABCDNode * , int>::iterator i = (v->inList).begin(); i != (v->inList).end(); i++){
					Graph2::ABCDNode *u = i->first;
					int d = i->second;
					int prove_result = prove(graph, active, C, a, i->first, c - d);
					Graph2::ABCDCheck* existingCheck = Graph2::getABCDCheck(C,a,v,c);
					
				
					if(existingCheck == NULL){
						C->valueMap.insert(std::pair<Graph2::ABCDCheck, int>(*check,prove_result));
					}else{
						int existingValue = getValue(C, *existingCheck);
						int meetResult = meetOperation(prove_result,existingValue,0);
						C->valueMap.erase(*check);
						C->valueMap.insert(std::pair<Graph2::ABCDCheck, int>(*check,meetResult));	
					}
					
				}
			}
			
			active->valueMap.erase(v); // active[v] = NULL

			return Graph2::getValue(C, *check); // return C[v - a <= c]
		}

		virtual bool runOnFunction(Function &F){

			errs() << "-------------------------- START -----------------------------\n";

			//if ( runOnce == false ) {
				//runOnce = true;
				if ( testDemandProve() ) {
					errs() << "Success\n";
				}
				else {
					errs() << "Failure\n";
				}
			//}

			errs() << "--------------------------- END ------------------------------\n";

			return true;
		}
		
		
/*		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<PromotePass>(); 
//			AU.setPreservesAll();
    		    }
*/    		    
	};

	char ABCDPass::ID = 0;
	RegisterPass<ABCDPass> X("abcd_test", "Redundant Check Elimination");
}
