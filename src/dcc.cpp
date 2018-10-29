#include "llvm/ADT/Optional.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "lexer.hpp"
#include "AST.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include "DummyC-2.hpp"
/* #include "DummyC.hpp" */

extern std::unique_ptr<llvm::Module> TheModule;

/**
 * オプション切り出し用クラス
 */
class OptionParser
{
	private:
		std::string InputFileName;
		std::string OutputFileName;
		std::string LinkFileName;
		bool WithJit;
		bool ObjFile;
		int Argc;
		char **Argv;

	public:
		OptionParser(int argc, char **argv):WithJit(false),Argc(argc), Argv(argv){}
		void printHelp();
		std::string getInputFileName(){return InputFileName;} 	//入力ファイル名取得
		std::string getOutputFileName(){return OutputFileName;} //出力ファイル名取得
		std::string getLinkFileName(){return LinkFileName;} 	//リンク用ファイル名取得
		bool getWithJit(){return WithJit;}		//JIT実行有無
		bool getObjFile(){return ObjFile;}		//オブジェクトファイルを出力
		bool parseOption();
};


/**
 * ヘルプ表示
 */
void OptionParser::printHelp(){
	fprintf(stdout, "Compiler for DummyC...\n" );
	fprintf(stdout, "試作中なのでバグがあったらご報告を\n" );
}


/**
 * オプション切り出し
 * @return 成功時：true　失敗時：false
 */
bool OptionParser::parseOption(){
	if(Argc < 2){
		fprintf(stderr,"引数が足りません\n");
		return false;
	}


	for(int i=1; i<Argc; i++){
		if(Argv[i][0]=='-' && Argv[i][1] == 'o' && Argv[i][2] == '\0'){
			//output filename
			OutputFileName.assign(Argv[++i]);
		}else if(Argv[i][0]=='-' && Argv[i][1] == 'h' && Argv[i][2] == '\0'){
			printHelp();
			return false;
		}else if(Argv[i][0]=='-' && Argv[i][1] == 'l' && Argv[i][2] == '\0'){
			LinkFileName.assign(Argv[++i]);
		}else if(Argv[i][0]=='-' && Argv[i][1] == 'j' && Argv[i][2] == 'i' && Argv[i][3] == 't' && Argv[i][4] == '\0'){
			WithJit = true;
		}else if(Argv[i][0]=='-' && Argv[i][1] == 'o' && Argv[i][2] == 'b' && Argv[i][3] == 'j' && Argv[i][4] == '\0'){
			ObjFile = true;
		}else if(Argv[i][0]=='-'){
			fprintf(stderr,"%s は不明なオプションです\n", Argv[i]);
			return false;
		}else{
			InputFileName.assign(Argv[i]);
		}
	}

	//OutputFileName
	std::string ifn = InputFileName;
	int len = ifn.length();
	if (OutputFileName.empty() && (len > 2) &&
		ifn[len-3] == '.' &&
		((ifn[len-2] == 'd' && ifn[len-1] == 'c'))) {
		OutputFileName = std::string(ifn.begin(), ifn.end()-3);
		OutputFileName += ".s";
	} else if(OutputFileName.empty()){
		OutputFileName = ifn;
		OutputFileName += ".s";
	}

	return true;
}

/**
 * main関数
 */
int main(int argc, char **argv) {
	llvm::InitializeNativeTarget();
	llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
	llvm::PrettyStackTraceProgram X(argc, argv);

	llvm::EnableDebugBuffering = true;

	OptionParser opt(argc, argv);
	if(!opt.parseOption())
	  exit(1);

	//check
	if(opt.getInputFileName().length()==0){
		fprintf(stderr,"入力ファイル名が指定されていません\n");
		exit(1);
	}

	//lex and parse
	Parser *parser=new Parser(opt.getInputFileName());
	if(!parser->doParse()){
		fprintf(stderr, "err at parser or lexer\n");
		SAFE_DELETE(parser);
		exit(1);
	}

	//get AST
	TranslationUnitAST &tunit=parser->getAST();
	if(tunit.empty()){
		fprintf(stderr,"TranslationUnit is empty");
		SAFE_DELETE(parser);
		exit(1);
	}

	CodeGen *codegen=new CodeGen();
	if(!codegen->doCodeGen(tunit, opt.getInputFileName(),
				opt.getLinkFileName()) ){
		fprintf(stderr, "err at codegen\n");
		SAFE_DELETE(parser);
		SAFE_DELETE(codegen);
		exit(1);
	}

	if(!opt.getWithJit() && !opt.getObjFile()) {
		llvm::legacy::PassManager ThePM = llvm::legacy::PassManager();
		ThePM.add(llvm::createInstructionCombiningPass());
		// 式の再結合
		ThePM.add(llvm::createReassociatePass());
		// 共通部分式の消去
		ThePM.add(llvm::createGVNPass());
		// 制御フロー図の簡約化 (到達不能ブロックの削除など).
		ThePM.add(llvm::createCFGSimplificationPass());
		ThePM.run(*TheModule);

		TheModule->dump();

		SAFE_DELETE(parser);
		SAFE_DELETE(codegen);
		return 0;
	}

	if (opt.getWithJit()) {
		// JITコンパイル
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();

/* 旧
		auto TheJIT = llvm::make_unique<llvm::orc::DummyCJIT>();
		TheJIT->addModule(std::move(TheModule));
		// main シンボル用のJITを検索する
-       auto *Main = (int (*)())TheJIT->getSymbolAddress("main");
-       fprintf(stderr, "Evaluated to %d\n", Main());
*/
/*
		llvm::orc::DummyCJIT::Create()とTheJIT->lookup("main")は
		Expected<T>を返す
		  Expected<T> values are also implicitly convertible to boolean,
		  but with the opposite convention to Error:
		    true for success, false for error.
		TheJIT->addModule(std::move(TheModule))はErrorを返す
		  Error values can be implicitly converted to bool:
		    true for error, false for success
*/
		if (auto CreateJIT = llvm::orc::DummyCJIT::Create()) {
			auto TheJIT = std::move(*CreateJIT);
			if (!TheJIT->addModule(std::move(TheModule))) {
				if (auto LookupMain = TheJIT->lookup("main")) {
					auto *Main = (int (*)())LookupMain->getAddress();
					fprintf(stderr, "Evaluated to %d\n", Main());
					goto FIN;
				}
			}
		}
		fprintf(stderr, "Error at JIT\n");
		SAFE_DELETE(parser);
		SAFE_DELETE(codegen);
		exit(1);
	}

	if (opt.getObjFile()) {
		llvm::InitializeAllTargetInfos();
		llvm::InitializeAllTargets();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmParsers();
		llvm::InitializeAllAsmPrinters();

		auto TargetTriple = llvm::sys::getDefaultTargetTriple();
		TheModule->setTargetTriple(TargetTriple);

		std::string err;
		auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, err);
		if (!Target) {
			fprintf(stderr, "%s\n", err.c_str());
			SAFE_DELETE(parser);
			SAFE_DELETE(codegen);
			exit(1);
		}

		auto CPU = "generic";
		auto Features = "";
		llvm::TargetOptions option;
		auto RM = llvm::Optional<llvm::Reloc::Model>();
		auto TheTargetMachine = Target->createTargetMachine(
		  TargetTriple, CPU, Features, option, RM);

		TheModule->setDataLayout(TheTargetMachine->createDataLayout());

		auto Filename = opt.getOutputFileName().c_str();
		std::error_code err_code;
		llvm::raw_fd_ostream dest(Filename, err_code, llvm::sys::fs::F_None);
		if (err_code) {
			fprintf(stderr, "Could not open file: %s\n", err_code.message().c_str());
			SAFE_DELETE(parser);
			SAFE_DELETE(codegen);
			exit(1);
		}

		llvm::legacy::PassManager ThePM = llvm::legacy::PassManager();
		ThePM.add(llvm::createInstructionCombiningPass());
		// 式の再結合
		ThePM.add(llvm::createReassociatePass());
		// 共通部分式の消去
		ThePM.add(llvm::createGVNPass());
		// 制御フロー図の簡約化 (到達不能ブロックの削除など).
		ThePM.add(llvm::createCFGSimplificationPass());

		auto FileType = llvm::TargetMachine::CGFT_ObjectFile;
		if (TheTargetMachine->addPassesToEmitFile(ThePM, dest, nullptr, FileType)) {
			fprintf(stderr, "TheTargetMachine can't emit a file of this type\n");
			SAFE_DELETE(parser);
			SAFE_DELETE(codegen);
			exit(1);
		}
		ThePM.run(*TheModule);
		dest.flush();
	}

FIN:
	//delete
	SAFE_DELETE(parser);
	SAFE_DELETE(codegen);

	return 0;
}
