#ifndef CODEGEN_HPP
#define CODEGEN_HPP


#include<cstdio>
#include<cstdlib>
#include<map>
#include<string>
#include<vector>
#include<llvm/ADT/APInt.h>
#include<llvm/IR/Constants.h>
#include<llvm/ExecutionEngine/ExecutionEngine.h>
#include<llvm/Linker/Linker.h>
#include<llvm/IR/LLVMContext.h>
#include<llvm/IR/Module.h>
#include<llvm/IR/Metadata.h>
#include<llvm/Support/Casting.h>
#include<llvm/Support/SourceMgr.h>
#include<llvm/IR/IRBuilder.h>
#include<llvm/IRReader/IRReader.h>
#include<llvm/IR/MDBuilder.h>
#include<llvm/IR/ValueSymbolTable.h>
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include"APP.hpp"
#include"AST.hpp"
//using namespace llvm;

/**
  * コード生成クラス
  */
class CodeGen{
	private:
		llvm::Function *CurFunc;		//現在コード生成中のFunction
		llvm::IRBuilder<> *Builder;	//LLVM-IRを生成するIRBuilder クラス

	public:
		CodeGen();
		~CodeGen();
		bool doCodeGen(TranslationUnitAST &tunit, std::string name, std::string link_file);

	private:
		bool generateTranslationUnit(TranslationUnitAST &tunit, std::string name);
		llvm::Function *generateFunctionDefinition(FunctionAST *func);
		llvm::Function *generatePrototype(PrototypeAST *proto);
		llvm::Value *generateFunctionStatement(FunctionStmtAST *func_stmt);
		llvm::Value *generateVariableDeclaration(VariableDeclAST *vdecl);
		llvm::Value *generateStatement(BaseAST *stmt);
		llvm::Value *generateBinaryExpression(BinaryExprAST *bin_expr);
		llvm::Value *generateCallExpression(CallExprAST *call_expr);
		llvm::Value *generateJumpStatement(JumpStmtAST *jump_stmt);
		llvm::Value *generateVariable(VariableAST *var);
		llvm::Value *generateNumber(int value);
		bool linkModule(std::string file_name);
};


#endif
