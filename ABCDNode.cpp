#include<vector>
#include<string>

using namespace std;


class ABCDNode;
class ABCDVariable;
class ABCDEdge;
class PhiFunction;
class ABCDGraph;
class ABCDCheck;

class ABCDVariable {

	private:
		string name;
		int number;

	public:
		ABCDVariable ( string name, int number );
		string getName();
		int getNumber();
};

class ABCDEdge {
	
	private:
		ABCDNode* source;
		ABCDNode* target;
		int edgeWeight;

	public:
		ABCDEdge ( ABCDNode* source, ABCDNode* target, int edgeWeight );
		ABCDNode* getSource();
		ABCDNode* getTarget();
		int getEdgeWeight();
		void setEdgeWeight ( int edgeWeight );

};

class ABCDNode {
	
	private:
		vector<ABCDEdge*> edges;
		ABCDVariable* variable;
		PhiFunction* phiFunction;
		bool _isPhiFunction;

	public:
		ABCDNode ( ABCDVariable* variable );
		ABCDNode ( vector<ABCDEdge*> edges, ABCDVariable* variable );
		ABCDNode ( vector<ABCDEdge*> edges, ABCDVariable* variable, PhiFunction* phiFunction ); 
		bool isPhiFunction();
		void addEdge ( ABCDEdge* edge );
		vector<ABCDEdge*> getEdges();
		ABCDVariable* getVariable();
		PhiFunction* getPhiFunction();

};

class PhiFunction {
	
	private:
		vector<ABCDVariable*> variables;
	
	public:
		PhiFunction();
		PhiFunction ( vector<ABCDVariable*> variables );
		void addVariable ( ABCDVariable* variable );
		vector<ABCDVariable*> getVariables();

};

class ABCDGraph {
	
	private:
		ABCDNode* rootNode;

	public:
		ABCDGraph ( ABCDNode* root );
		ABCDNode* getRootNode();
};

class ABCDCheck {

	private:
		ABCDNode* source;
		ABCDNode* target;
		int value;

	public:
		ABCDCheck ( ABCDNode* source, ABCDNode* target, int value );
		ABCDNode* getSource();
		ABCDNode* getTarget();
		int getValue();
};



ABCDVariable::ABCDVariable ( string name, int number ) {
	this->name = name;
	this->number = number;
}

string ABCDVariable::getName() {
	return this->name;
}

int ABCDVariable::getNumber() {
	return this->number;
}



ABCDNode::ABCDNode ( ABCDVariable* variable ) {
	this->variable = variable;
	//this->edges = new vector<ABCDEdge>();
	this->_isPhiFunction = false;
	this->phiFunction = NULL;
}

ABCDNode::ABCDNode ( vector<ABCDEdge*> edges, ABCDVariable* variable ) {
	this->edges = edges;
	this->variable = variable;
	this->_isPhiFunction = false;
	this->phiFunction = NULL;
}

ABCDNode::ABCDNode ( vector<ABCDEdge*> edges, ABCDVariable* variable, PhiFunction* phiFunction ) {
	this->edges = edges;
	this->variable = variable;
	this->phiFunction = phiFunction;
	this->_isPhiFunction = true;
}

bool ABCDNode::isPhiFunction() {
	return this->_isPhiFunction;
}

void ABCDNode::addEdge ( ABCDEdge* edge ) {
	this->edges.push_back ( edge );
}

vector<ABCDEdge*> ABCDNode::getEdges() {
	return this->edges;
}

ABCDVariable* ABCDNode::getVariable() {
	return this->variable;
}

PhiFunction* ABCDNode::getPhiFunction() {
	return this->phiFunction;
}



ABCDEdge::ABCDEdge ( ABCDNode* source, ABCDNode* target, int edgeWeight ) {
	this->source = source;
	this->target = target;
	this->edgeWeight = edgeWeight;
}

ABCDNode* ABCDEdge::getSource() {
	return this->source;
}

ABCDNode* ABCDEdge::getTarget() {
	return this->target;
}

int ABCDEdge::getEdgeWeight() {
	return this->edgeWeight;
}

void ABCDEdge::setEdgeWeight(int edgeWeight) {
	this->edgeWeight = edgeWeight;
}



PhiFunction::PhiFunction() {
	//this->variables = new vector<ABCDVariable>();
}

PhiFunction::PhiFunction ( vector<ABCDVariable*> variables ) {
	this->variables = variables;
}

void PhiFunction::addVariable ( ABCDVariable* variable ) {
	this->variables.push_back ( variable );
}

vector<ABCDVariable*> PhiFunction::getVariables() {
	return this->variables;
}


ABCDGraph::ABCDGraph ( ABCDNode *rootNode ) {
	this->rootNode = rootNode;
}

ABCDNode* ABCDGraph::getRootNode() {
	return this->rootNode;
}

ABCDCheck::ABCDCheck ( ABCDNode* source, ABCDNode* target, int value ) {
	this->source = source;
	this->target = target;
	this->value = value;
}

ABCDNode* ABCDCheck::getSource() {
	return this->source;
}

ABCDNode* ABCDCheck::getTarget() {
	return this->target;
}

int ABCDCheck::getValue() {
	return this->value;
}
