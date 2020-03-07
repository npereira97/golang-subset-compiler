#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "globalEnum.h"
#include "symbol_table.h"

Context *globalContext;
STEntry * symbolLookup(char *id, SymbolTable *s);
TTEntry * typeLookup(char *id, TypeTable *t);
int main(void) { 
	return 0; 
} // to compile



void symbolCheckExpressionList(ExpList* expressionList,Context* context);
void symbolCheckSwitchCaseClauseList(switchCaseClause* clauseList, Context* context);

void printClauseListSymbol(switchCaseClause* clauseList,int indentLevel);
TTEntry *makeTTEntry(Context* contx, TypeDeclNode *holder);

int hashCode(char * id)

{
	unsigned int hash = 0;
	while ( *id ) hash = (hash << 1) + *(id++);
	return hash % TABLE_SIZE;
}

Context * newContext()
{
	Context *c = (Context *) malloc(sizeof(Context));
	SymbolTable *s = (SymbolTable *) malloc(sizeof(SymbolTable));
	TypeTable *t = (TypeTable *) malloc(sizeof(TypeTable));
	for ( int i = 0; i < TABLE_SIZE; i++ )
	{
		s->entries[i] = NULL;
		t->entries[i] = NULL;
	}
	c->curTypeTable = t;
	c->curSymbolTable = s;
	return c;
}

Context * scopedContext(Context *c)
{
	Context *newC = newContext();
	newC->parent = c;
	return newC;
}

int addSymbolEntry(Context *c, STEntry *s)
{
	int pos = hashCode(s->id);
	
	for ( TTEntry *head = c->curTypeTable->entries[pos]; head; head = head->next ) {
		if ( strcmp(head->id, s->id) == 0 ) return 1;
		
	}
	STEntry *head = c->curSymbolTable->entries[pos];
	if (head == NULL) {
		c->curSymbolTable->entries[pos] = s;
		return 0;
	}
	while (head -> next != NULL)
	{
		if ( strcmp(head->id, s->id) == 0 ) return 1; 
		head = head->next;
	}
	if ( strcmp(head->id, s->id) == 0 ) return 1;
	head -> next = s;
	return 0;
}

int addTypeEntry(Context *c, TTEntry *t)
{
	int pos = hashCode(t->id);
	
	for(STEntry*head=c->curSymbolTable->entries[pos];head;head=head->next) {
		if ( strcmp(head->id, t->id) == 0 ) return 1;
		
	}
	TTEntry *head = c->curTypeTable->entries[pos];
	if (head == NULL) {
		c->curTypeTable->entries[pos] = t;
		return 0;
	}
	while (head -> next != NULL)
	{
		if ( strcmp(head->id, t->id) == 0 ) return 1; 
		head = head->next;
	}
	if ( strcmp(head->id, t->id) == 0 ) return 1;
	head -> next = t;
	return 0;
}

PolymorphicEntry *getEntry(Context *c, char *id)
{
	int pos = hashCode(id);
	for ( STEntry *head = c->curSymbolTable->entries[pos]; head; head = head->next )
	{ //If the adding was done correctly, it doesn't matter in which order we traverse-- the entry will only exist in max one table per scope
		if ( strcmp(head->id, id) == 0 )
		{
			PolymorphicEntry *e = (PolymorphicEntry *) malloc(sizeof(PolymorphicEntry));
			e->isSymbol = 1;
			e->entry.s = head;
			return e;
		}
	}
	for ( TTEntry *head = c->curTypeTable->entries[pos]; head; head = head->next )
	{
		if ( strcmp(head->id, id) == 0 )
		{
			PolymorphicEntry *e = (PolymorphicEntry *) malloc(sizeof(PolymorphicEntry));
			e->isSymbol = 0;
			e->entry.t = head;
			return e;
		}
	}
	if ( c->parent == NULL ) return NULL;
	return getEntry(c->parent, id);
}






void symbolCheckExpression(Exp *e, Context *c)
{
	if ( e->kind == expKindIdentifier )
	{
		if ( getEntry(c, e->val.id) == NULL )
		{
			fprintf(stderr, "Error: (%d) %s not declared as a variable, nor type", e->lineno, e->val.id);
			exit(1);
		}
		e->contextEntry = getEntry(c, e->val.id);
	}
	else if ( e->kind == expKindFieldSelect || e->kind == expKindIndexing )
	{
		symbolCheckExpression(e->val.access.base, c);
		symbolCheckExpression(e->val.access.accessor, c); //problmeatic: how does field selection of a struct work?
	}
	else if ( e->kind == expKindFuncCall )
	{ //All that matters in this stage is that it exists in A table. Which will matter in typecheck and codegen                   
		if ( getEntry(c, e->val.funcCall.base->val.id) == NULL ) //we did yardwork to ensure that base is an identifier
		{
			fprintf(stderr, "Error: (%d) %s not declared as a variable, nor variable", e->lineno, e->val.funcCall.base->val.id); 
			exit(1);
		}
		e->contextEntry = getEntry(c, e->val.funcCall.base->val.id);
		ExpList *curArg = e->val.funcCall.arguments;
		while ( curArg != NULL )
		{
			symbolCheckExpression(curArg->cur, c);
			curArg = curArg->next;
		}
	}
	else if ( isBinary(e) )
	{
		symbolCheckExpression(e->val.binary.left, c);
		symbolCheckExpression(e->val.binary.right, c);
	}
	else if ( isUnary(e) )
	{
		symbolCheckExpression(e->val.unary, c);		
	}
	else if ( e->kind == expKindCapacity || e->kind == expKindLength )
	{
		symbolCheckExpression(e->val.builtInBody, c);
	}
	else if ( e->kind == expKindAppend )
	{
		symbolCheckExpression(e->val.append.list, c);
		symbolCheckExpression(e->val.append.elem, c);
	}
}
void symbolCheckStatement(Stmt* stmt, Context* context){
	if (stmt == NULL){
		return;
	}

	Context* newContext;

	if (context == NULL){
		puts("Context shouldn't be NULL in symbolCheckSatement");
		exit(1);
	}

	switch (stmt->kind){


		
		case StmtKindSwitch :
							newContext = scopedContext(context);
							symbolCheckStatement(stmt->val.switchStmt.statement,newContext);
							symbolCheckExpression(stmt->val.switchStmt.expression,newContext);


							//I'm in sense encasing the clasue list in a block from the perspective of scope
							symbolCheckSwitchCaseClauseList(stmt->val.switchStmt.clauseList,scopedContext(newContext));
							break;



		case StmtKindBlock :
						newContext = scopedContext(context);
						symbolCheckStatement(stmt->val.block.stmt,newContext);
						break;




    	case StmtKindExpression: //TODO (symbolCheckExpression signature, implementation need to change)
						symbolCheckExpression(stmt->val.expression.expr,context);
						break;


		//lvalue assignability checks done at typechecking
		case StmtKindAssignment: 
								symbolCheckExpressionList(stmt->val.assignment.lhs,context);
								symbolCheckExpressionList(stmt->val.assignment.rhs,context);
								break;
	
		

		case StmtKindPrint :
							symbolCheckExpressionList(stmt->val.print.list,context);
							break;
		case StmtKindPrintln : 
							symbolCheckExpressionList(stmt->val.print.list,context);
							break;
		case StmtKindIf :
						newContext = scopedContext(context);

						symbolCheckStatement(stmt->val.ifStmt.statement,newContext);
						if(stmt->val.ifStmt.expression != NULL){
							symbolCheckExpression(stmt->val.ifStmt.expression,newContext);
						}
						symbolCheckStatement(stmt->val.ifStmt.block,newContext);

						symbolCheckStatement(stmt->val.ifStmt.elseBlock,context);

						break;
		case StmtKindReturn :
							if (stmt->val.returnVal.returnVal != NULL){
								symbolCheckExpression(stmt->val.returnVal.returnVal,context);
							}
							break;
		case StmtKindElse :
							symbolCheckStatement(stmt->val.elseStmt.block,context);
							break;
		
		case StmtKindInfLoop : symbolCheckStatement(stmt->val.infLoop.block,context);
								break;
		case StmtKindWhileLoop :
			symbolCheckExpression(stmt->val.whileLoop.conditon,context);
			symbolCheckStatement(stmt->val.whileLoop.block,context);
			break;
		case StmtKindThreePartLoop :
			newContext = scopedContext(context);
			symbolCheckStatement(stmt->val.forLoop.init,newContext);
			if (stmt->val.forLoop.condition != NULL){
				symbolCheckExpression(stmt -> val.forLoop.condition, newContext);
			}
			symbolCheckStatement(stmt->val.forLoop.inc,newContext);
			symbolCheckStatement(stmt->val.forLoop.block,newContext);
			
			break;


		//Trivially symbolcheck
		case StmtKindBreak :break;
		case StmtKindContinue :break;
		




		//For Denali to implement (Probably want to modify declaration nodes to include symbol references)
		case StmtKindTypeDeclaration :
			while(0) {}
			TTEntry *t = makeTTEntry(context, stmt -> val.typeDeclaration);
			if (t -> underlyingTypeType == inferType) {
				fprintf(stderr, "Error: (line %d) identifier (%s) %s\n", stmt -> lineno, stmt -> val.typeDeclaration -> actualType -> identification, t -> id);
				exit(1);
			}
			
			if (addTypeEntry(context, t) != 0) {
				fprintf(stderr, "Error: (on line %d) identifier (%s) has already ben declared in this scope\n", stmt -> lineno, t -> id);
				exit(1);
			}
			
			break;
		case StmtKindVarDeclaration :break;
		case StmtKindShortDeclaration : break; //Short declaration needs to contain a new variable
	}

	symbolCheckStatement(stmt->next,context);
}

void symbolCheckExpressionList(ExpList* expressionList,Context* context){
	if (expressionList == NULL){
		return;
	}

	symbolCheckExpression(expressionList->cur,context);
	symbolCheckExpressionList(expressionList->next,context);

	
}


void symbolCheckSwitchCaseClauseList(switchCaseClause* clauseList, Context* context){
	if (clauseList == NULL){
		return;
	}

	symbolCheckExpressionList(clauseList->expressionList,context);
	symbolCheckStatement(clauseList->statementList,scopedContext(context)); // Each stmt list in a clause has its own scope

	symbolCheckSwitchCaseClauseList(clauseList->next,context);
}


TTEntry *makeTTEntry(Context* contx, TypeDeclNode *holder){
	TTEntry *t = malloc(sizeof(TTEntry));
	t -> id = holder -> identifier;
	t -> underlyingTypeType = holder -> actualType -> kind;
	if (holder -> actualType -> kind == identifierType 
		||
		holder -> actualType -> kind == arrayType
		||
		holder -> actualType -> kind == sliceType
	) {
		PolymorphicEntry *assignee = getEntry(contx, holder -> actualType -> identification);
		if (assignee == NULL) {
			t -> id = "has not been declared";
			t -> underlyingTypeType = inferType;
			return t;
		}
		if (assignee -> isSymbol == 1) {
			t -> id = "was previoiusly declared as a symbol, cannot be used to declare a type";
			t -> underlyingTypeType = inferType;
			return t;
		}
		t -> val.normalType.type = assignee -> entry.t;
		t -> underlyingType = t -> val.normalType.type -> underlyingType;
		return t;
	} else if (holder -> actualType -> kind == structType){
		t -> underlyingType = baseStruct;
		TypeDeclNode *sMembs = holder -> actualType -> structMembers;
		if (sMembs == NULL) {
			t -> val.structType.fields = NULL;
			return t;
		}
		t -> val.structType.fields = malloc(sizeof(EntryTupleList));
		EntryTupleList *iter = t -> val.structType.fields;
//		EntryTupleList auxIter;
		while (sMembs -> nextDecl != NULL) {
			iter -> type = makeTTEntry(contx, sMembs);
			sMembs = sMembs -> nextDecl;
			/*
			iter -> next = NULL;
			for (auxIter = t -> val.structType.fields; auxIter != NULL; auxIter = auxIter -> next) {
				
			}
			*/
			iter -> next = malloc(sizeof(EntryTupleList));
			iter = iter -> next;
		}
		iter -> type = makeTTEntry(contx, sMembs);
		iter -> next = NULL;
		return t;
	} else {
		fprintf(stderr, "I was passed a bad TypeDeclNode. You shouldn't ever see this.\n");
		return NULL;
	}
}

static void indent(int indentLevel){
	for(int i = 0; i < indentLevel; i++){
		printf("	");
	}
}

 
void printStatementSymbol(Stmt* stmt,int indentLevel){

		if (stmt == NULL){
			return;
		}


		switch (stmt->kind){

			
			case StmtKindSwitch : 
								indent(indentLevel);
								printf("{\n");

								printStatementSymbol(stmt->val.switchStmt.statement,indentLevel+1);
								printClauseListSymbol(stmt->val.switchStmt.clauseList,indentLevel+1);

								printf("\n");
								indent(indentLevel);
								printf("}\n");
								break;

			case StmtKindBlock : indent(indentLevel);
								printf("{\n");
								printStatementSymbol(stmt->val.block.stmt,indentLevel+1);
								printf("\n");
								indent(indentLevel);
								printf("}\n");
								break;


			//Cannot introduce symbol, scope
			case StmtKindExpression : break; 
			case StmtKindAssignment : break;
			case StmtKindPrintln : break;
			case StmtKindPrint : break;
			case StmtKindReturn : break;
			case StmtKindBreak : break;
			case StmtKindContinue : break;




			case StmtKindIf : indent(indentLevel);
								printf("{\n");

								printStatementSymbol(stmt->val.ifStmt.statement,indentLevel+1);

								printStatementSymbol(stmt->val.ifStmt.block,indentLevel+1);

								printf("\n");
								indent(indentLevel);
								printf("}\n");
								break;

			case StmtKindElse : printStatementSymbol(stmt->val.elseStmt.block,indentLevel);
								break;
			case StmtKindInfLoop : printStatementSymbol(stmt->val.infLoop.block,indentLevel);
									break;
			case StmtKindWhileLoop : printStatementSymbol(stmt->val.whileLoop.block,indentLevel);
									break;
			case StmtKindThreePartLoop :indent(indentLevel);
										printf("{\n");

										printStatementSymbol(stmt->val.forLoop.init,indentLevel+1);
										printStatementSymbol(stmt->val.forLoop.inc,indentLevel+1);

										printStatementSymbol(stmt->val.forLoop.block,indentLevel+2);

										printf("\n");
										indent(indentLevel);
										printf("}\n");
										break;


				
			//For Denali to implement (I also designed the rest with the assumption that the following are terminated with newline characters)
			case StmtKindTypeDeclaration :break;
			case StmtKindVarDeclaration :break;
			case StmtKindShortDeclaration : break;



		}

		printStatementSymbol(stmt->next,indentLevel);
}


void printClauseListSymbol(switchCaseClause* clauseList,int indentLevel){
	if (clauseList == NULL){
		return;
	}

	indent(indentLevel);
	printf("{\n");
	printStatementSymbol(clauseList->statementList,indentLevel+1);
	printf("\n");
	indent(indentLevel);
	printf("}\n");

	printClauseListSymbol(clauseList->next,indentLevel);
}

