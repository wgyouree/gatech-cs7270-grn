#define DEBUG_TYPE "ESSAPass"
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


using namespace llvm;
STATISTIC(PIFuncs, "Number of pi functions inserted");


namespace {

	struct ESSAPass : public FunctionPass {

		static char ID;
		int functionSuffix;
		Module *m;
		ESSAPass() : FunctionPass(ID) {}

		virtual bool doInitialization(Module &M){
			m = &M;
			functionSuffix = 0;
		}

		virtual bool runOnFunction(Function &F) {

			char *EXITNAME = "exitBlock";
			char *PIBLOCKNAME = "piBlock";
			char *PIFUNCNAME = "pifunction_";

			/* Start inserting PI instructions */
			std::list<BranchInst *> brList;
			BasicBlock *curBB;
			for (Function::iterator FI = F.begin(); FI != F.end(); ++FI){
				curBB = &(*FI);
				if (isa<BranchInst>(*(curBB->getTerminator()))){
					BranchInst *temp = (BranchInst *)(curBB->getTerminator());
					if (temp->isConditional())
						brList.push_back(temp);
				}
			}

			StringRef *exitBBName = new StringRef(EXITNAME);
			for (std::list<BranchInst *>::iterator brIter = brList.begin(); brIter != brList.end(); ++brIter){
				Value *operands[2];
				BranchInst *curBR = *brIter;
				CmpInst *cmp = (CmpInst *)(curBR->getCondition());
				if (cmp->getNumOperands() < 2)
					continue;
				if (!cmp->isIntPredicate())
					continue;

				operands[0] = cmp->getOperand(0);
				operands[1] = cmp->getOperand(1);

				if(operands[0]->getType()->getTypeID() == Type::PointerTyID || operands[1]->getType()->getTypeID() == Type::PointerTyID)
					continue;
				  
/*				if(operands[0]->getType()->getTypeID() != 8 && operands[0]->getType()->getTypeID() != 9 && operands[1]->getType()->getTypeID() != 8 && operands[1]->getType()->getTypeID() != 9)
					continue;	
*/

				BasicBlock *newPIBlock;
				const Type *opType;
				char piFuncName[20];
				//				bool exit = false;
				//
				//				for (int j = 0; j < 2; j++){
				//					if (isa<Constant>(*operands[j]))
				//						continue;
				//					if (!isa<LoadInst>(*operands[j]))
				//						continue;
				//					opType = operands[j]->getType();
				//					std::vector<const Type *> params = std::vector<const Type *>();
				//					params.push_back(opType);
				//					FunctionType *fType = FunctionType::get(opType, params, false);
				//					sprintf(piFuncName, "%s%d", PIFUNCNAME, opType->getTypeID());
				//					Constant *ct = (m->getOrInsertFunction(StringRef(piFuncName), fType));
				//					if (!isa<Function>(*ct)){
				//						exit = true;
				//						break;
				//					}
				//				}
				//
				//				if (exit)
				//					continue;
				//errs() << "Starting ";
				for (int i=0; i < curBR->getNumSuccessors(); i++){
					newPIBlock = NULL;
					curBB = curBR->getSuccessor(i);
					if (curBB->getName().equals(*exitBBName))
						continue;
					for (int j = 0; j < 2; j++){
						//	errs () << "pred " << cmp->getPredicate();
						if (isa<Constant>(*operands[j]))
							continue;
						//errs() << " Mid1 ";
						if (!isa<LoadInst>(*operands[j]))
							continue;
						if(!newPIBlock){
							newPIBlock = BasicBlock::Create(F.getContext(), Twine(PIBLOCKNAME), &F, curBB);
							curBB->removePredecessor(curBR->getParent(), false);
							curBR->setSuccessor(i, newPIBlock);
						}
						//	errs() << " Mid ";
						opType = operands[j]->getType();
						std::vector<const Type *> params;// = std::vector<const Type *>();
						params.push_back(opType);
						FunctionType *fType = FunctionType::get(opType, params, false);
						sprintf(piFuncName, "%s%d", PIFUNCNAME, functionSuffix++); //opType->getTypeID()
				  	
						Function *temp = dyn_cast<Function>(m->getOrInsertFunction(StringRef(piFuncName), fType));  	
				  	PIFuncs++;

						if((temp->getBasicBlockList()).size() == 0){
							BasicBlock *returnBB = BasicBlock::Create(temp->getContext(), "return", temp);
							BasicBlock *entryBB = BasicBlock::Create(temp->getContext(), "entry", temp, returnBB);
							BranchInst *newBr = BranchInst::Create(returnBB,entryBB);

							Argument* arg = ((temp->arg_begin()));
							ReturnInst *newRet = ReturnInst::Create(temp->getContext(),arg,returnBB);
						}

						CallInst *piCall = CallInst::Create(temp, operands[j], "", newPIBlock);
						StoreInst *stInst = new StoreInst((Value *)piCall, ((LoadInst *)operands[j])->getOperand(0), newPIBlock);
					}
					if (newPIBlock)
						BranchInst::Create(curBB, newPIBlock);
				}

				//errs() << "Stop\n";
			}

			return true;
		}
	};

	char ESSAPass::ID = 0;
	RegisterPass<ESSAPass> X("essa", "Inserting PI instructions to generate e-SSA");
}
