#ifndef _MATH_EXPRESSION_H_
#define _MATH_EXPRESSION_H_

#include <string>
#include <vector>
#include <set>
#include <map>

using namespace std;

typedef enum {
    MathExprNodeType_Number       = 0,
    MathExprNodeType_Operator     = 1,
    MathExprNodeType_Symbol       = 2,
    MathExprNodeType_Function     = 3,
    MathExprNodeType_Expression   = 4,
    MathExprNodeType_Sign         = 5,
    MathExprNodeType_Separator    = 6,
    
    MathExprNodeTypeCount
} MathExprNodeType;

typedef struct MathExpressionNode
{
    MathExprNodeType type;
    string repr;
    vector<double> values;
    vector<MathExpressionNode> children;
} MathExpressionNode;


typedef struct MathExprNodeEvalTaskBuffer
{
    double* p;
    size_t n;
} MathExprNodeEvalTaskBuffer;

typedef double (*MathFunction_1)(double);
typedef double (*MathFunction_2)(double, double);
typedef double (*MathFunction_n)(double*, size_t);


// #pragma GCC visibility push(hidden)

class MathExpression
{
public:
    MathExpression(const char* lpcszExpr);
    void Symbols(set<string>& symbols);
    void Functions(set<string>& functions);
    void BindSymbols(const map<string, double>& symbols);
    bool Evaluate(vector<double>& results, const map<string, vector<double> >& symbols);
    
protected:
    bool IsBalanced(const char* lpcszExpr);
    bool IsValidForNumberBeginning(char chr);
    bool IsValidForName(char chr, bool bFirst);
    bool IsValidOperator(char chr);
    bool IsValidWhiteSpace(char chr);
    bool IsOperatorWithGreaterPrecedence(const string& A, const string& B);
    bool GetSubExpressionLength(const char* lpcszExpr, size_t& nExprSubLength);
    bool GetTokens(const char* lpcszExpr, vector<MathExpressionNode>& nodes, string& error);
    bool ValidatePreviousNext(const vector<MathExpressionNode>& nodes, size_t offset, const MathExprNodeType* pValidPrevious, size_t nValidPrevious, const MathExprNodeType* pValidNext, size_t nValidNext);
    bool Validate(const vector<MathExpressionNode>& nodes);
    bool ShuntingYard(vector<MathExpressionNode>& results, const vector<MathExpressionNode>& nodes, string& error);
    size_t GetSegmentSize();
    bool EvaluateEx(vector<double>& results, map<string, MathExprNodeEvalTaskBuffer>& bindings, const vector<MathExpressionNode>& nodes);
private:
    void initialize_f1();
    void initialize_f2();
    void initialize_constants();
private:
    string m_error;
    string m_expr;
    map<string, MathFunction_1> m_f1;
    map<string, MathFunction_2> m_f2;
    vector<MathExpressionNode> m_nodes;
    
};

// #pragma GCC visibility pop


#endif // _MATH_EXPRESSION_H_
