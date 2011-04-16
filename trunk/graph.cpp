#include <llvm/Value.h>
#include <vector>

using namespace llvm;

typedef struct ABCDNode_ ABCDNode;
typedef struct ABCDEdge_ ABCDEdge;

struct ABCDEdge_{
	ABCDNode *node;
	int weight;
};

struct ABCDNode_{
	Value *value;
	std::vector<ABCDEdge* > inList;
	std::vector<ABCDEdge* > outList;
};

std::vector<ABCDNode* > graph;

//So the graph is essentially just a list(vector) of nodes.

//Helper functions that needed to be implemented:

/*
As the name suggests, get the node if already present in the list,
(search on the Value*) or create a node with empty edge lists and 
the given value and return it.
*/
ABCDNode *getOrInsertNode(Value *value);

/*
Create two edges - out edge for n1 and in edge for n2.
*/
void insertEdge(ABCDNode *n1, ABCDNode *n2, int weight);
