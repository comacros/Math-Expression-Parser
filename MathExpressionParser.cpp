#ifdef _OC_VER

#include <origin.h>

#pragma dll(msvcrt, system)

double strtod(const char* str, char** endptr);

#pragma dll();

#else

#include <vector>
#include <string>
#include <set>
#include <map>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif

#include "MathExpressionParser.h"

typedef enum
{
	MathExprNodeType_Number       = 0,
	MathExprNodeType_Operator     = 1,
	MathExprNodeType_Symbol       = 2,
	MathExprNodeType_Function     = 3,
	MathExprNodeType_Expression   = 4,
	MathExprNodeType_Separator    = 5,
	
	MathExprNodeTypeCount
} MathExprNodeType;

class MathExprNode
{
public:
	MathExprNode()
	{
		unm = 0;
		repr = NULL;
		parent = NULL;
		child = NULL;
		previous = NULL;
		next = NULL;
	}

	~MathExprNode()
	{
		if(repr)
			free(repr);
		if(child)
			delete child;
		if(next)
			delete next;
		repr = NULL;
		child = NULL;
		next = NULL;
	}
	char* repr;
	bool unm;                        // valid only if type is number, expression, symbol, or function
	MathExprNodeType type;
	MathExprNode* parent;
	MathExprNode* child;          // valid only if type is Expression 
	MathExprNode* previous;
	MathExprNode* next;
	char* error[256];

#ifdef _OC_VER
	void symbols(vector<string>& tokens)
	{
		if(type == MathExprNodeType_Symbol)
		{
			if(!is_in_list(repr, tokens))
				tokens.Add(repr);
		}
		else if(type == MathExprNodeType_Expression)
		{
			MathExprNode* node = child;
			while(node)
			{
				node->symbols(tokens);
				node = node->next;
			}
		}
	}
#else
	void symbols(std::set<std::string>& tokens)
	{
		if(type == MathExprNodeType_Symbol)
		{
			tokens.insert(std::string(repr));
		}
		else if(type == MathExprNodeType_Expression)
		{
			MathExprNode* node = child;
			while(node)
			{
				node->symbols(tokens);
				node = node->next;
			}
		}

		return;
	}
#endif

	void print()
	{
		if(type == MathExprNodeType_Expression)
		{
			if(parent)
				printf("(");
			
			MathExprNode* node = child;
			while(node)
			{
				node->print();
				node = node->next;
			}
			
			if(parent)
				printf(")");
		}
		else
		{
			printf("%s%s", unm ? "-" : "", repr);
		}
		
		if(!parent)
			printf("\n");
	}
	void print(size_t nIndent)
	{
		size_t nBufferSize = 2 * nIndent + 1;
		char * lpcszIndentIndent = (char*)calloc(nBufferSize, sizeof(char));
		for(size_t i = 0; i < nBufferSize - 1; i++)
			lpcszIndentIndent[i] = ' ';
		printf("%s%s%s (%s)\n", lpcszIndentIndent, unm ? "-":"", repr, type == MathExprNodeType_Number ? "Number" : \
			                                     (type == MathExprNodeType_Operator ? "Operator" : \
												 (type == MathExprNodeType_Function ? "Function" : \
												 (type == MathExprNodeType_Expression ? "Expression" : \
												 (type == MathExprNodeType_Symbol ? "Symbol" : \
												 (type == MathExprNodeType_Separator ? "Separator" : "None"))))));
		free(lpcszIndentIndent);
		lpcszIndentIndent = NULL;

		if(child)
		{

			MathExprNode* node = child;
			while(node) {
				node->print(nIndent + 1);
				node = node->next;
			}

		}
	}
	

	MathExprNode* lastchild()
	{
		if(!child)
			return NULL;
		MathExprNode* node = child;
		while(node)
		{
			if(node->next)
				node = node->next;
			else
				return node;
		}

		return NULL;
	}
	size_t count()
	{
		size_t n = 0;
		if(child)
		{
			MathExprNode* node = child;
			while(node)
			{
				n++;
				n += node->count();
				node = node->next;
			}
		}
		return n;
	}
	void children(MathExprNode** nodes, size_t* offset)
	{
		if(child)
		{
			MathExprNode* node = child;
			while(node)
			{
				nodes[*offset] = node;
				*offset += 1;
				node->children(nodes, offset);
				node = node->next;
			}
		}
	}
	void remove(bool bReleaseBuffer) {
		if(next)
			next->previous = previous;
		if(previous)
			previous->next = next;
		if(parent && !previous){
			parent->child = next;
		}

		if(bReleaseBuffer)
		{
			if(repr)
				free(repr);
			repr = NULL;
		}
	}
	void release() {
#ifndef _OC_VER
		size_t N = count();
		
		MathExprNode** nodes = (MathExprNode**)calloc(N, sizeof(MathExprNode*));
		size_t offset = 0;
		children(nodes, &offset);
		for(size_t i = 0; i < N; i++)
		{
			MathExprNode* node = nodes[i];
			if(node)
			{
				if(node->repr)
				{
					free(node->repr);
					node->repr = NULL;
				}

				free(node);
				node = NULL;
			}
				
		}
#endif
		
		return;
	}
};


static bool __z_IsParenthesesPaired(const char* lpcszExpr)
{
	size_t N = 0;
	size_t nExprLength = strlen(lpcszExpr);
	for(size_t i = 0; i < nExprLength; i++)
	{
		if(lpcszExpr[i] == '(')
			N++;
		else if(lpcszExpr[i] == ')')
		{
			if(N == 0)
				return 0;
			else
				N--;
		}
	}
	
	return N == 0;
}
static bool __z_IsValidForNumberBeginning(char chr)
{
	return (chr >= '0' && chr <= '9') || chr == '.';
}
static bool __z_IsValidForName(char chr, bool bFirst)
{
	if(bFirst)
		return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || chr == '_';
	else
		return __z_IsValidForName(chr, 1) || (chr >= '0' && chr <= '9');
}
static bool __z_IsValidOperator(char chr)
{
	// Arithmetic Operators
	return chr == '+' || chr == '-' || chr == '*' || chr == '/' || chr == '^' || chr == '&' || chr == '|';
}

static bool __z_GetSubExpressionLength(const char* lpcszExpr, size_t* pExprSubLength)	// leading '(' and ending ')' included
{
	size_t nExprLength = strlen(lpcszExpr);
	if(nExprLength < 2 || lpcszExpr[0] != '(')
		return 0;
	
	size_t N = 0;
	for(size_t i = 0; i < nExprLength; i++)
	{
		if(lpcszExpr[i] == '(')
			N++;
		else if(lpcszExpr[i] == ')')
		{
			if(N == 0)
				return 0;
			N--;
		}
		
		if(N == 0)
		{
			*pExprSubLength = i + 1;
			return 1;
		}
	}
	
	return 0;
}
static void __z_PushBackMathExprNode(MathExprNode* expr, MathExprNode* exprSub)
{
	if(exprSub->previous)
		exprSub->previous->next = exprSub->next;
	if(exprSub->next)
		exprSub->next->previous = exprSub->previous;
	if(!exprSub->previous && exprSub->parent)
		exprSub->parent->child = exprSub->next;
	
	MathExprNode* exprLastChild = expr->lastchild();
	if(!exprLastChild)
		expr->child = exprSub;
	else
		exprLastChild->next = exprSub;
	exprSub->parent = expr;
	exprSub->previous = exprLastChild;
	
}
static void __z_CheckUnm(MathExprNode* expr)
{
	if(!expr)
		return;
	if(!expr->previous)
		return;
	if(expr->type == MathExprNodeType_Expression || expr->type == MathExprNodeType_Function || \
	   expr->type == MathExprNodeType_Number || expr->type == MathExprNodeType_Symbol)
	{
		MathExprNode* prev = expr->previous;
		while(prev)
		{
			if(prev->type == MathExprNodeType_Operator && (!prev->previous || prev->previous->type == MathExprNodeType_Separator))
			{
				if(strcmp(prev->repr, "-") == 0)
					expr->unm = !expr->unm;
				prev->remove(1);
				prev->parent = NULL;
				prev->child = NULL;
				prev->previous = NULL;
				prev->next = NULL;
				delete prev;

				break;
			}

			if(prev->type == MathExprNodeType_Operator && \
			   (prev->previous && prev->previous->type == MathExprNodeType_Operator) && \
			   (strcmp(prev->repr, "+") == 0 || strcmp(prev->repr, "-") == 0))
			{
				if(strcmp(prev->repr, "-") == 0)
				{
					expr->unm = !expr->unm;
				}

				prev->remove(1);
				prev->parent = NULL;
				prev->child = NULL;
				prev->previous = NULL;
				prev->next = NULL;
				delete prev;

				prev = expr->previous;
			}
			else
				break;
		}
	}
	
	if(expr->unm && expr->previous && expr->previous->type == MathExprNodeType_Operator)
	{
		if(strcmp(expr->previous->repr, "-") == 0)
		{
			expr->unm = 0;
			expr->previous->repr[0] = '+';
		}
		else if(strcmp(expr->previous->repr, "+") == 0)
		{
			expr->unm = 0;
			expr->previous->repr[0] = '-';
		}
	}
	
	return;
}

bool GetTokens(const char* lpcszExpr, MathExprNode* expr)
{
	size_t nExprLength = strlen(lpcszExpr);
	for(size_t i = 0; i < nExprLength;)
	{
		char chr = lpcszExpr[i];
		if(chr == '(')
		{
			size_t nExprSubLength = 0;
			if(!__z_GetSubExpressionLength(lpcszExpr + i, &nExprSubLength))
				return 0;

			MathExprNode* exprSub = new MathExprNode;
			exprSub->type = MathExprNodeType_Expression;
			exprSub->repr = (char*)calloc(nExprSubLength - 1, sizeof(char));
			for(size_t j = 0; j < nExprSubLength - 2; j++)
				exprSub->repr[j] = lpcszExpr[i + j + 1];
			i += nExprSubLength;

			__z_PushBackMathExprNode(expr, exprSub);

			if(exprSub->previous && exprSub->previous->type == MathExprNodeType_Symbol)
				exprSub->previous->type = MathExprNodeType_Function;

			__z_CheckUnm(exprSub);

			if(!GetTokens(exprSub->repr, exprSub))
				return 0;
		}
		else if(__z_IsValidForName(chr, 1))
		{
			size_t j = 0;
			for(j = i + 1; j < nExprLength; j++)
			{
				char chr = lpcszExpr[j];
				if(!__z_IsValidForName(chr, 0))
					break;
			}
			MathExprNode* exprSub = new MathExprNode;
			exprSub->type = MathExprNodeType_Symbol;
			exprSub->repr = (char*)calloc(j - i + 1, sizeof(char));
			for(size_t k = i; k < j; k++)
				exprSub->repr[k - i] = lpcszExpr[k];

			__z_PushBackMathExprNode(expr, exprSub);

			__z_CheckUnm(exprSub);

			i = j;
		}
		else if(__z_IsValidOperator(chr))
		{
			MathExprNode* exprSub = new MathExprNode;
			exprSub->type = MathExprNodeType_Operator;
			exprSub->repr = (char*)calloc(2, sizeof(char));
			exprSub->repr[0] = chr;

			__z_PushBackMathExprNode(expr, exprSub);

			i++;
		}
		else if(__z_IsValidForNumberBeginning(chr))
		{
			char* pEnd;
			double f = strtod(lpcszExpr + i, &pEnd);
			size_t nNumberLength = pEnd - (lpcszExpr + i);

			MathExprNode* exprSub = new MathExprNode;
			exprSub->type = MathExprNodeType_Number;
			exprSub->repr = (char*)calloc(nNumberLength + 1, sizeof(char));
			for(size_t j = 0; j < nNumberLength; j++)
				exprSub->repr[j] = lpcszExpr[i + j];

			
			__z_PushBackMathExprNode(expr, exprSub);
			
			//exprSub->value = atof(exprSub->repr);
			
			__z_CheckUnm(exprSub);

			i = pEnd - lpcszExpr;
		}
		else if(chr == ',')
		{
			MathExprNode* exprSub = new MathExprNode;
			exprSub->type = MathExprNodeType_Separator;
			exprSub->repr = (char*)calloc(2, sizeof(char));
			exprSub->repr[0] = chr;

			__z_PushBackMathExprNode(expr, exprSub);

			i++;
		}
		else if(chr == ' ' || chr == '\t' || chr == '\r' || chr == '\n')
		{
			i++;
		}
		else
		{
			return 0;
		}
	}
	return 1;
}
bool ParseMathExpressionEx(const char* lpcszExpr, MathExprNode* expr)
{
	if(!__z_IsParenthesesPaired(lpcszExpr))
		return 0;

	if(!GetTokens(lpcszExpr, expr))
		return 0;

	//bool bRet = expr->validate();
	
	return 1;
}

#ifdef _OC_VER
void ParseMathExpression(const char* lpcszMathExpression, vector<string>& symbols)
#else
void ParseMathExpression(const char* lpcszMathExpression, std::set<std::string>& symbols)
#endif
{
	// printf("%s\n", lpcszMathExpression);
	size_t nExprLength = strlen(lpcszMathExpression);
	MathExprNode* expr = new MathExprNode;
	expr->type = MathExprNodeType_Expression;
	expr->repr = (char*)calloc(nExprLength + 1, sizeof(char));
	for(size_t i = 0; i < nExprLength; i++)
		expr->repr[i] = lpcszMathExpression[i];
	bool bRet = ParseMathExpressionEx(lpcszMathExpression, expr);
	
#ifdef _OC_VER
	expr->symbols(symbols);
#else
	expr->symbols(symbols);
#endif
	// expr->print(0);
	// expr->print();
	delete expr;
}

//int _tmain(int argc, _TCHAR* argv[])
//int main(int argc, char* argv[])

#ifdef _OC_VER
static void __z_PrintExpressionResults(const char* repr, const vector<string>& symbols)
{
	printf("Expression:\n\t%s\nSymbols(%d):\n", repr, symbols.GetSize());
	for(size_t i = 0; i < symbols.GetSize(); i++)
	{
		printf("\t[ %2d ]\t%s\n", i, symbols[i]);
	}
	printf("\n");
}
#endif

int testMathExpressionParser()
{

#ifdef _OC_VER
	vector<string> symbols1, symbols2;
#else
	std::set<std::string> symbols1, symbols2;
#endif

	char* expr1 = "-3.0e-2 + -a * (-sin(+atan2(b, 8) * - pi) - x)";
	char* expr2 = "sqrt(a^2+b^2)";
	ParseMathExpression(expr1, symbols1);
	ParseMathExpression(expr2, symbols2);

#ifdef _OC_VER
	__z_PrintExpressionResults(expr1, symbols1);
	__z_PrintExpressionResults(expr2, symbols2);
#endif

	return 0;
}

