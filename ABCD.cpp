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
#include "llvm/ADT/Twine.h"

using namespace llvm;

namespace {

	struct ABCPass : public FunctionPass {

		static char ID;
		Function *f;
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
			return true;
		}

		virtual bool runOnFunction(Function &F) {
			//for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
			//	errs() << *I << "\n";
			//}
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
				//errs() << *I << "\n";

				//if(isa<CallInst>(*I))
				//    errs() << "is a call instruction\n";

				if(!isa<GetElementPtrInst>(*I))
					continue;
				else {
					Instruction *inst = &*I;
					//errs() << *inst << "\n";
					GetElementPtrInst *GEPI = cast<GetElementPtrInst>(inst);
					gep_type_iterator GI = gep_type_begin(GEPI), GE = gep_type_end(GEPI);

					uint64_t NumElements = 0;
					Value *value = NULL;

					while(GI!=GE) {
						//errs()<< *I << "\n";
						if (const ArrayType *AT = dyn_cast<ArrayType>(*GI)) {
							NumElements = AT->getNumElements();
							errs() << "Number of elements in the array: " << NumElements << "\n";

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
					//errs() << currBlock->getName() << "\n";

					ICmpInst *cmpinst = new ICmpInst(inst, ICmpInst::ICMP_SLT, value, upperbound);

					//BasicBlock::iterator instBegin = currBlock->getInstList().begin();
					//BasicBlock::iterator instEnd = currBlock->getInstList().end();

					//while(instBegin != instEnd && *instBegin != inst){
					//	instBegin++;
					//}

					BasicBlock *newBlock = currBlock->splitBasicBlock(inst, "bounds");

					TerminatorInst *termInst = newBlock->getTerminator();
					//BasicBlock *otherBlock = termInst->getSuccessor(0);
					BasicBlock *otherBlock = BasicBlock::Create(F.getContext(), Twine("exitBlock"), &F);
					Value *one = ConstantInt::get(Type::getInt32Ty(F.getContext()),1);
					CallInst *exitCall = CallInst::Create(f, one, "", otherBlock);
					new UnwindInst(F.getContext(), otherBlock);

					//TerminatorInst *termInstc = currBlock->getTerminator();
					//errs() << *termInstc << "\n";

					BranchInst *branchInst = BranchInst::Create(newBlock, otherBlock, cmpinst);

					llvm::ReplaceInstWithInst(currBlock->getTerminator(), branchInst);

					//termInstc = currBlock->getTerminator();
					//errs() << *termInstc << "\n";
					
					break;

				}
			}
			//for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
			//	errs() << *I << "\n";
			//}
			errs() << "Complete\n";

			return true;

		}

//		virtual bool doFinalization(Module &M) {
//			return false;
//		}

//		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
//			AU.setPreservesAll();
//		};
	};

	char ABCPass::ID = 0;
	RegisterPass<ABCPass> X("abc", "Array Bounds Checking");
}
