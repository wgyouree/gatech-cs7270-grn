#include <vector>



namespace Graph{



	typedef struct ABCDNode_ ABCDNode;

	typedef struct ABCDEdge_ ABCDEdge;



	struct ABCDEdge_{

		ABCDNode *node;

		int weight;

	};



	struct ABCDNode_{

		llvm::Value *value;

	   int length; // 0 => not an array length node.

		std::vector<ABCDEdge* > inList;

		std::vector<ABCDEdge* > outList;

	};



	typedef struct{

		std::vector<ABCDNode* > arrayLengthList;

	   	std::vector<ABCDNode* > variableList;

	}ABCDGraph;



	//So the graph is essentially just a list(vector) of nodes.



	//Helper functions that needed to be implemented:



	/*

	As the name suggests, get the node if already present in the list,

	(search on the Value*) or create a node with empty edge lists and 

	the given value and return it.



	If value isa<AllocaInst> insert into arrayLengthList with the input length,

	else in the variableList.

	*/

	ABCDNode *getOrInsertNode(ABCDGraph *graph, llvm::Value *value, int length);



	/*

	Create two edges - out edge for n1 and in edge for n2.

	*/

	void insertEdge(ABCDNode *n1, ABCDNode *n2, int weight);
}
