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

namespace {

	struct ABCPass : public FunctionPass {

		static char ID;
		Function *f;
		Module *m;
		ABCPass() : FunctionPass(ID) {}

		virtual bool doInitialization(Module &M){
			std::vector<const Type *> params = std::vector<const Type *>();
			params.push_back(Type::getInt32Ty(M.getContext()));

			FunctionType *fType = FunctionType::get(Type::getVoidTy(M.getContext()), params, false);

			Constant *temp = M.getOrInsertFunction("exit", fType);

			if(!temp){
				errs() << "exit function not in symbol table\n";
				exit(1);
			}

			f = cast<Function>(temp);
			m = &M;
			return true;
		}

		virtual bool runOnFunction(Function &F) {
			std::list<Instruction *> arrayAccessInstList;
			bool exitBlockCreated = false;
			char *EXITNAME = "exitBlock";
			char *CHECKINST = "abcdcmpinst";

			BasicBlock *otherBlock;
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
				if(isa<GetElementPtrInst>(*I))
					arrayAccessInstList.push_back(&*I);
			}
			std::list<Instruction *>::iterator instIter;
			for (instIter = arrayAccessInstList.begin(); instIter != arrayAccessInstList.end(); ++instIter){
				Instruction *inst = *instIter;
				GetElementPtrInst *GEPI = cast<GetElementPtrInst>(inst);
				gep_type_iterator GI = gep_type_begin(GEPI), GE = gep_type_end(GEPI);

				uint64_t NumElements = 0;
				Value *value = NULL;

				while(GI!=GE) {
					if (const ArrayType *AT = dyn_cast<ArrayType>(*GI)) {
						NumElements = AT->getNumElements();

						value = GEPI->getOperand(2);

						//Works only for array accesses that can be determined in compile time
						//uint64_t AccessElements = cast<ConstantInt>(GEPI->getOperand(2))->getZExtValue();
						//errs() << "Accessing element: " << AccessElements << "\n";

					}
					GI++;
				}

				if(value==NULL || NumElements==0)
					continue;

				const IntegerType *intType = IntegerType::get(F.getContext(), 32);
				ConstantInt *consInt = ConstantInt::get(intType, NumElements);
				Constant *cons = cast<Constant>(consInt);
				Value *upperbound = cast<Value>(cons);

				BasicBlock *currBlock = inst->getParent();

				ICmpInst *cmpinst = new ICmpInst(inst, ICmpInst::ICMP_SLT, value, upperbound, Twine(CHECKINST));

				BasicBlock *newBlock = currBlock->splitBasicBlock(inst, "bounds");

				TerminatorInst *termInst = newBlock->getTerminator();
				//BasicBlock *otherBlock = termInst->getSuccessor(0);
				if (!exitBlockCreated){
					exitBlockCreated = true;
					otherBlock = BasicBlock::Create(F.getContext(), Twine(EXITNAME), &F);
					Value *one = ConstantInt::get(Type::getInt32Ty(F.getContext()),1);
					CallInst *exitCall = CallInst::Create(f, one, "", otherBlock);
					new UnwindInst(F.getContext(), otherBlock);
				}

				BranchInst *branchInst = BranchInst::Create(newBlock, otherBlock, cmpinst);

				llvm::ReplaceInstWithInst(currBlock->getTerminator(), branchInst);
			}
			errs() << "Number of array accesses= " << arrayAccessInstList.size() << "\n";

			return true;
		}
	};

	char ABCPass::ID = 0;
	RegisterPass<ABCPass> X("abc", "Array Bounds Checking");
}
