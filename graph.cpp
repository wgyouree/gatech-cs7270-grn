#include <vector>

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
