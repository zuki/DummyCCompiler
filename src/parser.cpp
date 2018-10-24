#include "parser.hpp"



/**
  * コンストラクタ
  */
Parser::Parser(std::string filename){
	Tokens=LexicalAnalysis(filename);
}


/**
  * 構文解析実効
  * @return 解析成功：true　解析失敗：false
  */
bool Parser::doParse(){
	if(!Tokens){
		fprintf(stderr, "error at lexer\n");
		return false;
	}else
		return visitTranslationUnit();
}


/**
  * AST取得
  * @return TranslationUnitへの参照
  */
TranslationUnitAST &Parser::getAST(){
	if(TU)
		return *TU;
	else
		return *(new TranslationUnitAST());
}

// translation_unit : { external_declaration }
/**
  * TranslationUnit用構文解析メソッド
  * @return 解析成功：true　解析失敗：false
  */
bool Parser::visitTranslationUnit(){
	//最初にprintnumの宣言追加
	TU=new TranslationUnitAST();
	std::vector<std::string> param_list;
	param_list.push_back("i");
	TU->addPrototype(new PrototypeAST("printnum", param_list));
	PrototypeTable["printnum"]=1;

	//ExternalDecl
	while(true){
		if(!visitExternalDeclaration(TU)){
			SAFE_DELETE(TU);
			return false;
		}
		if(Tokens->getCurType()==TOK_EOF)
			break;
	}
	return true;
}

// external_declaration
//	: function_declaration | function_definition
/**
  * ExternalDeclaration用構文解析クラス
  * 解析したPrototyupeとFunctionASTをTranslationUitに追加
  * @param TranslationUnitAST
  * @return true
  */
bool Parser::visitExternalDeclaration(
		TranslationUnitAST *tunit
		){
	//FunctionDeclaration
	PrototypeAST *proto=visitFunctionDeclaration();
	if(proto){
		tunit->addPrototype(proto);
		return true;
	}

	//FunctionDefinition
	FunctionAST *func_def=visitFunctionDefinition();
	if(func_def){
		tunit->addFunction(func_def);
		return true;
	}

	return false;
}


// function_declaration : prototype , ";"
/**
  * FunctionDclaration用構文解析メソッド
  * @return 解析成功：PrototypeAST　解析失敗：NULL
  */
PrototypeAST *Parser::visitFunctionDeclaration(){
	int bkup=Tokens->getCurIndex();
	PrototypeAST *proto=visitPrototype();
	if(!proto){
		return NULL;
	}

	//prototype;
	if(Tokens->getCurString()==";"){
		// PrototypeTableに登録済 または FunctionTableに
		// 登録済みで引数の数が違う場合はエラー
		if( PrototypeTable.find(proto->getName()) != PrototypeTable.end() ||
			(FunctionTable.find(proto->getName()) != FunctionTable.end() &&
			FunctionTable[proto->getName()] != proto->getParamNum() ) ){
			fprintf(stderr, "Function：%s is redefined" ,proto->getName().c_str());
			SAFE_DELETE(proto);
			return NULL;
		}
		PrototypeTable[proto->getName()]=proto->getParamNum();
		Tokens->getNextToken();
		return proto;
	}else{ // NextTokenが ';' でなければエラー
		SAFE_DELETE(proto);
		Tokens->applyTokenIndex(bkup);
		return NULL;
	}

}

// function_definition : prototype, function_statement
/**
  * FunctionDfinition用構文解析メソッド
  * @return 解析成功：FunctionAST　解析失敗：NULL
  */
FunctionAST *Parser::visitFunctionDefinition(){
	int bkup=Tokens->getCurIndex();
	// 先頭は Prototype
	PrototypeAST *proto=visitPrototype();
	if(!proto){
		return NULL;
	// PrototypeTableに登録済で引数の数が違う
	// または FunctionTableに登録済み場合はエラー
	}else if( (PrototypeTable.find(proto->getName()) != PrototypeTable.end() &&
				PrototypeTable[proto->getName()] != proto->getParamNum() ) ||
				FunctionTable.find(proto->getName()) != FunctionTable.end()){
			fprintf(stderr, "Function：%s is redefined" ,proto->getName().c_str());
			SAFE_DELETE(proto);
			return NULL;
	}

	VariableTable.clear();
	FunctionStmtAST *func_stmt = visitFunctionStatement(proto);
	if(func_stmt){
		FunctionTable[proto->getName()]=proto->getParamNum();
		return new FunctionAST(proto,func_stmt);
	}else{
		SAFE_DELETE(proto);
		Tokens->applyTokenIndex(bkup);
		return NULL;
	}
}

// prototype : "int" , identifier , "(" , parameter_list , ")"
/**
  * Prototype用構文解析メソッド
  * @return 解析成功：PrototypeAST　解析失敗：NULL
  */
PrototypeAST *Parser::visitPrototype(){
	std::string func_name;

	//bkup index
	int bkup=Tokens->getCurIndex();

	//type_specifier: 'int'
	if(Tokens->getCurType()==TOK_INT){
		Tokens->getNextToken();
	}else{
		return NULL;
	}


	//IDENTIFIER
	if(Tokens->getCurType()==TOK_IDENTIFIER){
		func_name=Tokens->getCurString();
		Tokens->getNextToken();
	}else{
		Tokens->ungetToken(1);	//unget TOK_INT
		return NULL;
	}

	//'('
	if(Tokens->getCurString()=="("){
		Tokens->getNextToken();
	}else{
		Tokens->ungetToken(2);	//unget TOK_INT IDENTIFIER
		return NULL;
	}


	//parameter_list: [ parameter, { ',' , parameter } ]
	//  parameter : "int" , identifier
	std::vector<std::string> param_list;
	bool is_first_param = true;
	while(true){
		//','
		if(!is_first_param && Tokens->getCurType()==TOK_SYMBOL && Tokens->getCurString()==","){
			Tokens->getNextToken();
		}
		if(Tokens->getCurType()==TOK_INT){
			Tokens->getNextToken();
		}else{
			break; // 最初に 'int' が来なければparamterは終了
		}

		if(Tokens->getCurType()==TOK_IDENTIFIER){
			//引数の変数名に被りがあったらエラー
			if(std::find(param_list.begin(), param_list.end(), Tokens->getCurString()) !=
					param_list.end()){
				Tokens->applyTokenIndex(bkup);
				return NULL;
			}
			param_list.push_back(Tokens->getCurString());
			Tokens->getNextToken();
		}else{ // intの次が識別子ではないのでエラー
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}
		is_first_param = false;
	}


	// ')'
	if(Tokens->getCurString()==")"){
		Tokens->getNextToken();
		return new PrototypeAST(func_name, param_list);
	}else{
		Tokens->applyTokenIndex(bkup);
		return NULL;
	}
}

// function_statement
//   : "{" , [ variable_declaration_list ] , statement_list , "}"
/**
  * FunctionStatement用構文解析メソッド
  * @param 勘数名，引数を格納したPrototypeクラスのインスタンス
  * @return 解析成功：FunctionSmtAST　解析失敗：NULL
  */
FunctionStmtAST *Parser::visitFunctionStatement(PrototypeAST *proto){
	int bkup=Tokens->getCurIndex();

	// '{'
	if(Tokens->getCurString()=="{"){
		Tokens->getNextToken();
	}else{
		return NULL;
	}

	//create FunctionStatement
	FunctionStmtAST *func_stmt = new FunctionStmtAST();

	// 引数を FunctionStatement に追加
	for(int i=0; i<proto->getParamNum(); i++){
		VariableDeclAST *vdecl=new VariableDeclAST(proto->getParamName(i));
		vdecl->setDeclType(VariableDeclAST::param); // 変数種別はparam
		func_stmt->addVariableDeclaration(vdecl);
		VariableTable.push_back(vdecl->getName());
	}

	VariableDeclAST *var_decl;
	BaseAST *stmt;
	BaseAST *last_stmt = nullptr;

	//variable_declaration_list: ローカル変数定義
	if((var_decl=visitVariableDeclaration())){
		while(var_decl){
			var_decl->setDeclType(VariableDeclAST::local);
			// 変数テーブルに登録済みの場合はエラー
			if(std::find(VariableTable.begin(), VariableTable.end(), var_decl->getName()) !=
					VariableTable.end()){
				SAFE_DELETE(var_decl);
				SAFE_DELETE(func_stmt);
				return NULL;
			}
			func_stmt->addVariableDeclaration(var_decl);
			VariableTable.push_back(var_decl->getName());
			//parse Variable Delaration
			var_decl=visitVariableDeclaration();
		}

		if((stmt=visitStatement())){
			while(stmt){
				last_stmt=stmt;
				func_stmt->addStatement(stmt);
				stmt=visitStatement();
			}
		}
	//{statement_list}: 本体ステートメント
	}else if((stmt=visitStatement())){
		while(stmt){
			last_stmt=stmt;
			func_stmt->addStatement(stmt);
			stmt=visitStatement();
		}


	//other
	}else{
		SAFE_DELETE(func_stmt);
		Tokens->applyTokenIndex(bkup);
		return NULL;
	}

	//check if last statement is jump_statement
	// 関数定義は jump_statement ('return' 文)で終わらなければならない)
	if(!last_stmt || !llvm::isa<JumpStmtAST>(last_stmt)){
		SAFE_DELETE(func_stmt);
		Tokens->applyTokenIndex(bkup);
		return NULL;
	}

	// '}'
	if(Tokens->getCurString()=="}"){
		Tokens->getNextToken();
		return func_stmt;
	}else{
		SAFE_DELETE(func_stmt);
		Tokens->applyTokenIndex(bkup);
		return NULL;
	}

}

// variable_declaration : "int" , identifier , ";"
/**
  * VariableDeclaration用構文解析メソッド
  * @return 解析成功：VariableDeclAST　解析失敗：NULL
  */
VariableDeclAST *Parser::visitVariableDeclaration(){
	std::string name;

	// 'int'
	if(Tokens->getCurType()==TOK_INT){
		Tokens->getNextToken();
	}else{
		return NULL;
	}

	//IDENTIFIER
	if(Tokens->getCurType()==TOK_IDENTIFIER){
		name=Tokens->getCurString();
		Tokens->getNextToken();
	}else{
		Tokens->ungetToken(1);
		return NULL;
	}

	// ';'
	if(Tokens->getCurString()==";"){
		Tokens->getNextToken();
		return new VariableDeclAST(name);
	}else{
		Tokens->ungetToken(2);
		return NULL;
	}
}

// statement : expression_statement | jump_statement
/**
  * Statement用構文解析メソッド
  * @return 解析成功：AST　解析失敗：NULL
  */
BaseAST *Parser::visitStatement(){
	BaseAST *stmt=NULL;
	if((stmt=visitExpressionStatement())){
		return stmt;
	}else if((stmt=visitJumpStatement())){
		return stmt;
	}else{
		return NULL;
	}
}

// expression_statement : ";" | assignment_expression , ";"
/**
  * ExpressionStatement用構文解析メソッド
  * @return  解析成功：AST　解析失敗：NULL
  */
BaseAST *Parser::visitExpressionStatement(){
	BaseAST *assign_expr;

	//NULL Expression
	if(Tokens->getCurString()==";"){
		Tokens->getNextToken();
		return new NullExprAST();
	}else if((assign_expr=visitAssignmentExpression())){
		if(Tokens->getCurString()==";"){
			Tokens->getNextToken();
			return assign_expr;
		}
	}
	return NULL;
}

// jump_statement : "return" , assignment_expression , ";"
/**
  * JumpStatement用構文解析メソッド
  * @return 解析成功：AST　解析失敗：NULL
  */
BaseAST *Parser::visitJumpStatement(){
	//bakup index
	int bkup=Tokens->getCurIndex();
	BaseAST *expr;

	if(Tokens->getCurType() == TOK_RETURN){
		Tokens->getNextToken();
		if(!(expr=visitAssignmentExpression()) ){
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}

		if(Tokens->getCurString()==";"){
			Tokens->getNextToken();
			return new JumpStmtAST(expr);
		}else{
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}
	}else{
		return NULL;
	}
}

// assignment_expression : identifier , "=" , additive_expression
//   | additive_expression
/**
  * AssignmentExpression用構文解析メソッド
  * @return 解析成功：AST　解析失敗：NULL
  */
BaseAST *Parser::visitAssignmentExpression(){
	int bkup=Tokens->getCurIndex();

	//	IDENTIFIER '=' additive_expression
	BaseAST *lhs;
	if(Tokens->getCurType()==TOK_IDENTIFIER){
		//変数は宣言されていなければならない
		// 宣言されていない変数への代入は無視する
		if(std::find(VariableTable.begin(), VariableTable.end(), Tokens->getCurString()) !=
				VariableTable.end()){

			lhs=new VariableAST(Tokens->getCurString());
			Tokens->getNextToken();
			BaseAST *rhs;
			if(Tokens->getCurType()==TOK_SYMBOL &&
				Tokens->getCurString()=="="){
				Tokens->getNextToken();
				if((rhs=visitAdditiveExpression(NULL))){
					return new BinaryExprAST("=", lhs, rhs);
				}else{
					SAFE_DELETE(lhs);
					Tokens->applyTokenIndex(bkup);
				}
			}else{
				SAFE_DELETE(lhs);
				Tokens->applyTokenIndex(bkup);
			}
		}else{
			Tokens->applyTokenIndex(bkup);
		}
	}

	//additive_expression
	BaseAST *add_expr=visitAdditiveExpression(NULL);
	if(add_expr){
		return add_expr;
	}

	return NULL;
}

// additive_expression : multiplicative_expression ,
//    [ { "+" , multiplicative_expression
//      | "-" , multiplicative_expression } ]
/**
  * AdditiveExpression用構文解析メソッド
  * @param lhs(左辺),初回呼び出し時はNULL
  * @return 解析成功：AST 解析失敗：NULL
  */
BaseAST *Parser::visitAdditiveExpression(BaseAST *lhs){
	//bkup index
	int bkup=Tokens->getCurIndex();

	if(!lhs)
		lhs=visitMultiplicativeExpression(NULL);
	if(!lhs){
		return NULL;
	}
	BaseAST *rhs;
	// '+'
	if(Tokens->getCurType()==TOK_SYMBOL &&
				Tokens->getCurString()=="+"){
		Tokens->getNextToken();
		rhs=visitMultiplicativeExpression(NULL);
		if(rhs){
			return visitAdditiveExpression(
						new BinaryExprAST("+", lhs, rhs)
					);
		}else{
			SAFE_DELETE(lhs);
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}

	// '-'
	}else if(Tokens->getCurType()==TOK_SYMBOL &&
				Tokens->getCurString()=="-"){
		Tokens->getNextToken();
		rhs=visitMultiplicativeExpression(NULL);
		if(rhs){
			return visitAdditiveExpression(
						new BinaryExprAST("-", lhs, rhs)
					);
		}else{
			SAFE_DELETE(lhs);
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}
	}
	return lhs;
}

// multiplicative_expression : postfix_expression ,
//   [ { "*" , postfix_expression
//     | "/" , postfix_expression } ]
/**
  * MultiplicativeExpression用解析メソッド
  * @param  lhs(左辺),初回呼び出し時はNULL
  * @return 解析成功：AST　解析失敗：NULL
  */
BaseAST *Parser::visitMultiplicativeExpression(BaseAST *lhs){
	//bkup index
	int bkup=Tokens->getCurIndex();

	//BaseAST *lhs=visitPostfixExpression();
	if(!lhs)
		lhs=visitPostfixExpression();
	if(!lhs){
			return NULL;
	}
	BaseAST *rhs;
	// '*'
	if(Tokens->getCurType()==TOK_SYMBOL &&
				Tokens->getCurString()=="*"){
		Tokens->getNextToken();
		rhs=visitPostfixExpression();
		if(rhs){
			return visitMultiplicativeExpression(
						new BinaryExprAST("*", lhs, rhs)
					);
		}else{
			SAFE_DELETE(lhs);
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}

	// '/'
	}else if(Tokens->getCurType()==TOK_SYMBOL &&
				Tokens->getCurString()=="/"){
		Tokens->getNextToken();
		rhs=visitPostfixExpression();
		if(rhs){
			return visitMultiplicativeExpression(
						new BinaryExprAST("/", lhs, rhs)
					);
		}else{
			SAFE_DELETE(lhs);
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}
	}
	return lhs;
}

// postfix_expression : primary_expression
//   |  identifier , "(" , [ assignment_expression , { "," , assignment_expression } ] , ")"
/**
  * PostfixExpression用構文解析メソッド
  * @return 解析成功：AST　解析失敗：NULL
  */
BaseAST *Parser::visitPostfixExpression(){
	//get index
	int bkup=Tokens->getCurIndex();

	// primary_expression
	BaseAST *prim_expr=visitPrimaryExpression();
	if(prim_expr){
		return prim_expr;
	}

	// 関数呼び出し
	if(Tokens->getCurType()==TOK_IDENTIFIER){
		// 引数の個数を得る。関数が未登録の場合はNULL return
		int param_num;
		if(PrototypeTable.find(Tokens->getCurString()) !=
			PrototypeTable.end() ){
			param_num=PrototypeTable[Tokens->getCurString()];
		}else if(FunctionTable.find(Tokens->getCurString()) !=
			FunctionTable.end()){
			param_num=FunctionTable[Tokens->getCurString()];
		}else{
			return NULL;
		}

		//関数名取得
		std::string Callee=Tokens->getCurString();
		Tokens->getNextToken();

		// '('
		if(Tokens->getCurType()!=TOK_SYMBOL ||
				Tokens->getCurString()!="("){
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}

		Tokens->getNextToken();
		// 引数リスト: argument list
		std::vector<BaseAST*> args;
		BaseAST *assign_expr=visitAssignmentExpression();
		if(assign_expr){
			args.push_back(assign_expr);
			while(Tokens->getCurType()==TOK_SYMBOL &&
					Tokens->getCurString()==","){
				Tokens->getNextToken();

				//IDENTIFIER
				assign_expr=visitAssignmentExpression();
				if(assign_expr){
					args.push_back(assign_expr);
				}else{
					break;
				}
			}//end while
		}

		// 実引数の数が定義と違う場合はエラー
		if(args.size() != static_cast<unsigned int>(param_num)){
			for(unsigned int i=0;i<args.size();i++)
				SAFE_DELETE(args[i]);
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}

		// ')'
		if(Tokens->getCurType()==TOK_SYMBOL &&
					Tokens->getCurString()==")"){
			Tokens->getNextToken();
			return new CallExprAST(Callee, args);
		}else{
			for(unsigned int i=0;i<args.size();i++)
				SAFE_DELETE(args[i]);
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}

	}else{
		return NULL;
	}
}

// primary_expression : identifier | integer
//    | "(" , assignment_expression , ")"
/**
  * PrimaryExpression用構文解析メソッド
  * @return 解析成功時：AST　失敗時：NULL
  */
BaseAST *Parser::visitPrimaryExpression(){
	//recored index
	int bkup=Tokens->getCurIndex();

	// identifier: 定義済み変数
	if(Tokens->getCurType()==TOK_IDENTIFIER &&
		(std::find(VariableTable.begin(), VariableTable.end(), Tokens->getCurString()) !=
		VariableTable.end())){
		std::string var_name=Tokens->getCurString();
		Tokens->getNextToken();
		return new VariableAST(var_name);

	// integer
	}else if(Tokens->getCurType()==TOK_DIGIT){
		int val=Tokens->getCurNumVal();
		Tokens->getNextToken();
		return new NumberAST(val);

	// integer(-)
	}else if(Tokens->getCurType()==TOK_SYMBOL &&
			Tokens->getCurString()=="-"){
		Tokens->getNextToken();
		if(Tokens->getCurType()==TOK_DIGIT){
			int val=Tokens->getCurNumVal();
			Tokens->getNextToken();
			return new NumberAST(-val);
		}else{
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}

	// '(' expression ')'
	}else if(Tokens->getCurType()==TOK_SYMBOL &&
			Tokens->getCurString()=="("){
		Tokens->getNextToken();

		//expression
		BaseAST *assign_expr;
		if(!(assign_expr=visitAssignmentExpression())){
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}

		//RIGHT PALEN
		if(Tokens->getCurString()==")"){
			Tokens->getNextToken();
			return assign_expr;
		}else{
			SAFE_DELETE(assign_expr);
			Tokens->applyTokenIndex(bkup);
			return NULL;
		}

	}

	return NULL;
}
