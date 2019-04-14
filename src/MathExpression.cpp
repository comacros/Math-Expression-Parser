#define _USE_MATH_DEFINES
#include <cmath>

#ifdef _MSC_VER
#include <omp.h>
#endif

#include <vector>
#include <string>
#include <set>
#include <map>
#include <limits>
//#include <algorithm>
#include <cstdarg>

//#define NDEBUG
#include <cassert>

#include "MathExpression.h"

using namespace std;

typedef struct MathExpressionOperator
{
    string repr;
    size_t precedence;
    
    friend bool operator>(const MathExpressionOperator& l, const MathExpressionOperator& r)
    {
        return l.precedence > r.precedence;
    }
    
} MathExpressionOperator;

static MathExpressionOperator __MathExpression_operators__[] = {
    {"+", 1},
    {"-", 1},
    {"*", 2},
    {"/", 2},
    {"^", 3}
};


inline bool EvalMathFunction_1(MathFunction_1 f, std::vector<double>& results, const std::vector<double>& values_1)
{
    results.resize(values_1.size());
    for(size_t i = 0; i < values_1.size(); i++)
        results[i] = f(values_1[i]);
    return true;
}
inline bool EvalMathFunction_2(MathFunction_2 f, std::vector<double>& results, const std::vector<double>& values_1, const std::vector<double>& values_2)
{
    if(values_1.size() == values_2.size())
    {
        results.resize(values_1.size());
        for(size_t i = 0; i < results.size(); i++)
            results[i] = f(values_1[i], values_2[i]);
    }
    else if(values_1.size() == 1)
    {
        results.resize(values_2.size());
        for(size_t i = 0; i < values_2.size(); i++)
            results[i] = f(values_1[0], values_2[i]);
    }
    else if(values_2.size() == 1)
    {
        results.resize(values_1.size());
        for(size_t i = 0; i < values_1.size(); i++)
            results[i] = f(values_1[i], values_2[0]);
    }
    else
        return false;
    
    return true;
}

MathExpression::MathExpression(const char* lpcszExpr)
{
    if(!IsBalanced(lpcszExpr))
    {
        m_error = "Parentheses Not Balanced";
        return;
    }
    m_expr.assign(lpcszExpr);
    
    m_nodes.resize(0);
    if(!GetTokens(lpcszExpr, m_nodes, m_error))
        return;
    
    if(!Validate(m_nodes))
    {
        m_error = "Tokens Order Invalid.";
        return;
    }
    
    initialize_f1();
    initialize_f2();
    
    vector<MathExpressionNode> results;
    ShuntingYard(results, m_nodes, m_error);
    m_nodes = results;
    
    return;
}
void MathExpression::Symbols(set<string>& symbols)
{
    symbols.clear();
    for(size_t i = 0; i < m_nodes.size(); i++)
    {
        if(m_nodes[i].type == MathExprNodeType_Symbol)
            symbols.insert(m_nodes[i].repr);
    }
}
void MathExpression::Functions(set<string>& functions)
{
    functions.clear();
    for(size_t i = 0; i < m_nodes.size(); i++)
    {
        if(m_nodes[i].type == MathExprNodeType_Function)
            functions.insert(m_nodes[i].repr);
    }
}
void MathExpression::BindSymbols(const map<string, double>& symbols)
{
    if(!symbols.size())
        return;
    for(size_t i = 0; i < m_nodes.size(); i++)
    {
        if(m_nodes[i].type == MathExprNodeType_Symbol)
        {
            map<string, double>::const_iterator it = symbols.find(m_nodes[i].repr);
            if(it != symbols.end())
            {
                m_nodes[i].values.resize(1);
                m_nodes[i].values[0] = it->second;
            }
        }
    }
}
bool MathExpression::Evaluate(vector<double>& results, const map<string, vector<double> >& symbols)
{
    results.resize(0);
    
    // nMaxLength is 1 in case expression has no symbol.
    size_t nMaxLength = symbols.size() ? 0 : 1;
    for(map<string, vector<double> >::const_iterator it = symbols.begin(); it != symbols.end(); it++)
    {
        if(nMaxLength < it->second.size())
            nMaxLength = it->second.size();
    }
    
    size_t nSegmentSize = GetSegmentSize();
    size_t nTasks = nMaxLength / nSegmentSize;
    if((numeric_limits<unsigned long long>::max)() < nTasks)
    {
        m_error = "Not Enough Memory.";
        return false;
    }
    if(nMaxLength > nTasks * nSegmentSize)
        nTasks++;
    if(!nTasks)
        return false;
    
    typedef struct {
        vector<double> _results;
        map<string, vector<double> > _symbols;
        map<string, MathExprNodeEvalTaskBuffer> _symbols2;
    } MathExprNodeEvalTask;
    
    vector<MathExprNodeEvalTask> tasks(nTasks);
    for(size_t i = 0; i < nTasks; i++)
    {
        for(map<string, vector<double> >::const_iterator it = symbols.begin(); it != symbols.end(); it++)
        {
            size_t N = (it->second.size() >= i * nSegmentSize + nSegmentSize ? nSegmentSize : it->second.size() - i * nSegmentSize);
            if(it->second.size() == 0)
                return false;
            else
            {
                MathExprNodeEvalTaskBuffer buffer;
                buffer.n = N;
                buffer.p = const_cast<double*>(it->second.data() + i * nSegmentSize);
                tasks[i]._symbols2[it->first] = buffer;
            }
        }
    }
    
    signed long long N = static_cast<signed long long>(nTasks);
    size_t nEvalError = 0;
#pragma omp parallel for reduction(+: nEvalError)
    for(signed long long i = 0; i < N; i++)
    {
        if(!EvaluateEx(tasks[i]._results, tasks[i]._symbols2, m_nodes))
            nEvalError = 1;
        else
            nEvalError = 0;
    }
    if(nEvalError)
        return false;
    
    size_t nResultSize = 0;
    for(size_t i = 0; i < tasks.size(); i++)
        nResultSize += tasks[i]._results.size();
    
    results.reserve(nResultSize);
    for(size_t i = 0; i < tasks.size(); i++)
        results.insert(results.end(), tasks[i]._results.begin(), tasks[i]._results.end());
    
    return true;
}

bool MathExpression::IsBalanced(const char* lpcszExpr)
{
    size_t N = 0;
    size_t nExprLength = strlen(lpcszExpr);
    for(size_t i = 0; i < nExprLength; i++)
    {
        if(lpcszExpr[i] == '(')
        {
            N++;
        }
        else if(lpcszExpr[i] == ')')
        {
            if(N == 0)
                return false;
            N--;
        }
    }
    
    return N == 0;
}
bool MathExpression::IsValidForNumberBeginning(char chr)
{
    return (chr >= '0' && chr <= '9') || chr == '.';
}
bool MathExpression::IsValidForName(char chr, bool bFirst)
{
    if(bFirst)
        return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || chr == '_';
    else
        return IsValidForName(chr, 1) || (chr >= '0' && chr <= '9');
}
bool MathExpression::IsValidOperator(char chr)
{
    for(size_t i = 0; i < sizeof(__MathExpression_operators__)/sizeof(MathExpressionOperator); i++)
    {
        if(__MathExpression_operators__[i].repr[0] == chr)
            return true;
    }
    return false;
}
bool MathExpression::IsValidWhiteSpace(char chr)
{
    return chr == ' ' || chr == '\n' || chr == '\r' || chr == '\t' || chr == '\f' || chr == '\v';
}
bool MathExpression::IsOperatorWithGreaterPrecedence(const string& A, const string& B)
{
    // assumes that A and b are valid operators.
    size_t offsetOpA = 0, offsetOpB = 0;
    for(size_t i = 0; i < sizeof(__MathExpression_operators__)/sizeof(MathExpressionOperator); i++)
    {
        if(__MathExpression_operators__[i].repr == A)
            offsetOpA = i;
        else if(__MathExpression_operators__[i].repr == B)
            offsetOpB = i;
    }
    
    return __MathExpression_operators__[offsetOpA] > __MathExpression_operators__[offsetOpB];
}
bool MathExpression::GetSubExpressionLength(const char* lpcszExpr, size_t& nExprSubLength)    // leading '(' and ending ')' included
{
    size_t nExprLength = strlen(lpcszExpr);
    if(!nExprLength || lpcszExpr[0] != '(')
        return false;
    
    size_t N = 0;
    for(size_t i = 0; i < nExprLength; i++)
    {
        if(lpcszExpr[i] == '(')
        {
            N++;
        }
        else if(lpcszExpr[i] == ')')
        {
            if(N == 0)
            {
                return false;
            }
            
            N--;
            
            if(N == 0)
            {
                nExprSubLength = i + 1;
                return true;
            }
        }
    }
    
    return false;
}
bool MathExpression::GetTokens(const char* lpcszExpr, vector<MathExpressionNode>& nodes, string& error)
{
    size_t nExprLength = strlen(lpcszExpr);
    for(size_t i = 0; i < nExprLength;)
    {
        char chr = lpcszExpr[i];
        MathExpressionNode node;
        node.values.resize(0);
        node.children.resize(0);
        
        if(chr == '(')
        {
            node.type = MathExprNodeType_Expression;
            
            size_t nSubSize = 0;
            if(!GetSubExpressionLength(lpcszExpr + i, nSubSize))
            {
                error = "Parentheses Unbalanced.";
                return false;
            }
            node.repr.assign(lpcszExpr + i + 1, nSubSize - 2);
            
            vector<MathExpressionNode> children;
            if(!GetTokens(node.repr.c_str(), children, error))
                return false;
            
            node.children.assign(children.begin(), children.end());
            
            if(nodes.size() && nodes.back().type == MathExprNodeType_Symbol)
                nodes.back().type = MathExprNodeType_Function;
            
            nodes.push_back(node);
            
            i += nSubSize;
            continue;
        }
        else if(IsValidForName(chr, true))
        {
            size_t j = 0;
            for(j = i + 1; j < nExprLength; j++)
            {
                char chr = lpcszExpr[j];
                if(!IsValidForName(chr, false))
                    break;
            }
            node.type = MathExprNodeType_Symbol;
            node.repr.assign(lpcszExpr + i, j - i);
            nodes.push_back(node);
            
            i = j;
            continue;
        }
        else if(IsValidOperator(chr))
        {
            node.type = MathExprNodeType_Operator;
            node.repr = chr;
            
            bool SignMerged = false;
            
            if(nodes.size() == 0 || \
               nodes.back().type == MathExprNodeType_Separator || \
               nodes.back().type == MathExprNodeType_Operator  || nodes.back().type == MathExprNodeType_Sign)
            {
                node.type = MathExprNodeType_Sign;
                if(node.repr != "+" && node.repr != "-")
                {
                    error = "Invalid Operator.";
                    return false;
                }
                
                if(nodes.size())
                {
                    if(nodes.back().type == MathExprNodeType_Sign)
                    {
                        SignMerged = true;
                        nodes.back().repr = (nodes.back().repr == node.repr ? "+" : "-");
                    }
                    else if(nodes.back().type == MathExprNodeType_Operator)
                    {
                        if(nodes.back().repr == "+")
                        {
                            SignMerged = true;
                            nodes.back().repr = node.repr;
                        }
                        else if(nodes.back().repr == "-")
                        {
                            nodes.back().repr = (node.repr == "+" ? "-" : "+");
                        }
                        
                    }
                }
            }
            
            if(!SignMerged)
                nodes.push_back(node);
            if(nodes.size() && nodes.back().type == MathExprNodeType_Sign && nodes.back().repr == "+")
                nodes.pop_back();
            
            i++;
            continue;
        }
        else if(IsValidForNumberBeginning(chr))
        {
            char* pEnd;
            double f = strtod(lpcszExpr + i, &pEnd);
            size_t nNumberLength = pEnd - (lpcszExpr + i);
            
            node.type = MathExprNodeType_Number;
            node.repr.assign(lpcszExpr + i, nNumberLength);
            node.values.push_back(f);
            
            if(nodes.size() && nodes.back().type == MathExprNodeType_Sign && nodes.back().repr == "+")
                nodes.pop_back();
            nodes.push_back(node);
            
            i = pEnd - lpcszExpr;
            continue;
        }
        else if(chr == ',')
        {
            node.type = MathExprNodeType_Separator;
            node.repr = chr;
            nodes.push_back(node);
            
            i++;
            continue;
        }
        else if(IsValidWhiteSpace(chr))
        {
            i++;
            continue;
        }
        else
        {
            error = "Invalid Character Found.";
            return false;
        }
    }
    
    
    return true;
}

bool MathExpression::ValidatePreviousNext(const vector<MathExpressionNode>& nodes, size_t offset, const set<MathExprNodeType>& ValidPrevious, const set<MathExprNodeType>& ValidNext)
{
    if(offset >= nodes.size())
        return false;
    if(offset > 0)
    {
        MathExprNodeType prevtype = nodes[offset - 1].type;
        if(ValidPrevious.find(prevtype) == ValidPrevious.end())
            return false;
    }
    if(offset + 1 < nodes.size())
    {
        MathExprNodeType nexttype = nodes[offset + 1].type;
        if(ValidNext.find(nexttype) == ValidNext.end())
            return false;
    }
    
    return true;
}
bool MathExpression::Validate(const vector<MathExpressionNode>& nodes) {
    /***********************************************************************************************************************
     *  MathExprNodeType_Number,     MathExprNodeType_Operator, MathExprNodeType_Symbol, MathExprNodeType_Function,        *
     *  MathExprNodeType_Expression, MathExprNodeType_Sign,     MathExprNodeType_Separator                                 *
     ***********************************************************************************************************************/
    
    // there must be at least one number / symbol / expression
    bool hasOperand = false;
    for(size_t i = 0; i < nodes.size(); i++)
    {
        if(nodes[i].type == MathExprNodeType_Number || nodes[i].type == MathExprNodeType_Symbol || nodes[i].type == MathExprNodeType_Expression)
        {
            hasOperand = true;
            break;
        }
    }
    if(!hasOperand)
        return false;
    
    for(size_t i = 0; i < nodes.size(); i++)
    {
        MathExpressionNode node = nodes[i];
        
        // MathExprNodeType_Sign should not show here
        // in that it should be merged in GetToken()
        switch(node.type)
        {
            case MathExprNodeType_Number:
            {
                // number can be the first or the last token
                // only operator and separator are allowed before and after a number
                set<MathExprNodeType> prev;
                prev.insert(MathExprNodeType_Operator);
                prev.insert(MathExprNodeType_Sign);
                prev.insert(MathExprNodeType_Separator);
                
                set<MathExprNodeType> next;
                next.insert(MathExprNodeType_Operator);
                next.insert(MathExprNodeType_Separator);
                if(!ValidatePreviousNext(nodes, i, prev, next))
                    return false;
                
                break;
            }
            case MathExprNodeType_Operator:
            {
                // operator can not be the first or the last token
                // leading PLUS/MINUS should have been handled by CheckUnm()
                // only number, symbol, expression and function are allowed before and after operator
                set<MathExprNodeType> prev;
                prev.insert(MathExprNodeType_Number);
                prev.insert(MathExprNodeType_Symbol);
                prev.insert(MathExprNodeType_Expression);
                
                set<MathExprNodeType> next;
                next.insert(MathExprNodeType_Number);
                next.insert(MathExprNodeType_Symbol);
                next.insert(MathExprNodeType_Function);
                next.insert(MathExprNodeType_Expression);
                next.insert(MathExprNodeType_Sign);
                if(!ValidatePreviousNext(nodes, i, prev, next))
                    return false;
                if(i == 0 || i == nodes.size() - 1)
                    return false;
                
                break;
            }
            case MathExprNodeType_Symbol:
            {
                // symbol can not be before or after number, symbol, expression and function
                set<MathExprNodeType> prev;
                prev.insert(MathExprNodeType_Operator);
                prev.insert(MathExprNodeType_Separator);
                prev.insert(MathExprNodeType_Sign);
                
                set<MathExprNodeType> next;
                next.insert(MathExprNodeType_Operator);
                next.insert(MathExprNodeType_Separator);
                if(!ValidatePreviousNext(nodes, i, prev, next))
                    return false;
                
                break;
            }
            case MathExprNodeType_Function:
            {
                // function can not be after number, symbol, expression and function
                // must be before expression
                
                set<MathExprNodeType> prev;
                prev.insert(MathExprNodeType_Operator);
                prev.insert(MathExprNodeType_Separator);
                prev.insert(MathExprNodeType_Sign);
                
                set<MathExprNodeType> next;
                next.insert(MathExprNodeType_Expression);
                if(!ValidatePreviousNext(nodes, i, prev, next))
                    return false;
                if(i == nodes.size() - 1)
                    return false;
                
                break;
            }
            case MathExprNodeType_Expression:
            {
                // expression cannot be before/after number, symbol, expression
                // expression cannot be before function
                // expression can be after function
                // expression must have at least one child
                
                set<MathExprNodeType> prev;
                prev.insert(MathExprNodeType_Operator);
                prev.insert(MathExprNodeType_Function);
                prev.insert(MathExprNodeType_Separator);
                prev.insert(MathExprNodeType_Sign);
                
                set<MathExprNodeType> next;
                next.insert(MathExprNodeType_Operator);
                next.insert(MathExprNodeType_Separator);
                
                if(!ValidatePreviousNext(nodes, i, prev, next))
                    return false;
                if(!node.children.size())
                    return false;
                if(!Validate(node.children))
                    return false;
                
                break;
            }
            case MathExprNodeType_Separator:
            {
                // separator must be between number, symbol, expression and function
                set<MathExprNodeType> prev;
                prev.insert(MathExprNodeType_Number);
                prev.insert(MathExprNodeType_Symbol);
                prev.insert(MathExprNodeType_Expression);
                set<MathExprNodeType> next;
                next.insert(MathExprNodeType_Number);
                next.insert(MathExprNodeType_Symbol);
                next.insert(MathExprNodeType_Function);
                next.insert(MathExprNodeType_Expression);
                next.insert(MathExprNodeType_Sign);
                if(!ValidatePreviousNext(nodes, i, prev, next))
                    return false;
                if(i == 0 || i == nodes.size() - 1)
                    return false;
                break;
            }
            case MathExprNodeType_Sign:
            {
                set<MathExprNodeType> prev, next;
                prev.insert(MathExprNodeType_Operator);
                prev.insert(MathExprNodeType_Separator);
                next.insert(MathExprNodeType_Number);
                next.insert(MathExprNodeType_Symbol);
                next.insert(MathExprNodeType_Function);
                next.insert(MathExprNodeType_Expression);
                if(!ValidatePreviousNext(nodes, i, prev, next))
                    return false;
                if(i == nodes.size() - 1)
                    return false;
                break;
            }
            default: return false;
        }
    }
    return true;
}
bool MathExpression::ShuntingYard(vector<MathExpressionNode>& results, const vector<MathExpressionNode>& nodes, string& error)
{
    // Shunting Yard Algorighm: https://en.wikipedia.org/wiki/Shunting-yard_algorithm
    
    if(nodes.size() == 0)
        return false;
    
    vector<MathExpressionNode> OutputQueue;
    vector<MathExpressionNode> OperatorStack;
    
    for(size_t i = 0; i < nodes.size(); i++)
    {
        if(nodes[i].type == MathExprNodeType_Number || nodes[i].type == MathExprNodeType_Symbol)
            OutputQueue.push_back(nodes[i]);
        else if(nodes[i].type == MathExprNodeType_Expression)
        {
            vector<MathExpressionNode> _results;
            if(!ShuntingYard(_results, nodes[i].children, error))
                return false;
            
            OutputQueue.insert(OutputQueue.end(), _results.begin(), _results.end());
        }
        else if(nodes[i].type == MathExprNodeType_Function)
            OperatorStack.push_back(nodes[i]);
        else if(nodes[i].type == MathExprNodeType_Sign)
        {
            vector<MathExpressionNode> _results, _nodes;
            for(size_t j = i + 1; j < nodes.size(); j++)
            {
                if(nodes[j].type == MathExprNodeType_Number      || \
                   nodes[j].type == MathExprNodeType_Symbol      || \
                   nodes[j].type == MathExprNodeType_Function    || \
                   nodes[j].type == MathExprNodeType_Expression  || \
                   nodes[j].type == MathExprNodeType_Sign        || \
                   (nodes[j].type == MathExprNodeType_Operator && nodes[j].repr != "+" && nodes[j].repr != "-"))
                {
                    _nodes.push_back(nodes[j]);
                }
                else
                    break;
            }
            if(!ShuntingYard(_results, _nodes, error))
                return false;
            OutputQueue.insert(OutputQueue.end(), _results.begin(), _results.end());
            OutputQueue.push_back(nodes[i]);
            i += _nodes.size();
        }
        else if(nodes[i].type == MathExprNodeType_Separator)
        {
            OutputQueue.insert(OutputQueue.end(), OperatorStack.begin(), OperatorStack.end());
            OperatorStack.resize(0);
            
            // unnecessary to push back separator
            //OutputQueue.push_back(nodes[i]);
        }
        else if(nodes[i].type == MathExprNodeType_Operator && nodes[i].repr == "^")
        {
            vector<MathExpressionNode> _results, _nodes;
            for(size_t j = i + 1; j < nodes.size(); j++)
            {
                if(nodes[j].type == MathExprNodeType_Number     || \
                   nodes[j].type == MathExprNodeType_Symbol     || \
                   nodes[j].type == MathExprNodeType_Function   || \
                   nodes[j].type == MathExprNodeType_Expression || \
                   nodes[j].type == MathExprNodeType_Sign       || \
                   (nodes[j].type == MathExprNodeType_Operator && nodes[j].repr == "^"))
                {
                    _nodes.push_back(nodes[j]);
                }
                else
                    break;
            }
            if(!ShuntingYard(_results, _nodes, error))
                return false;
            OutputQueue.insert(OutputQueue.end(), _results.begin(), _results.end());
            OutputQueue.push_back(nodes[i]);
            i += _nodes.size();
        }
        else if(nodes[i].type == MathExprNodeType_Operator && nodes[i].repr != "^")
        {
            if(OperatorStack.size())
            {
                for(size_t j = OperatorStack.size(); j >= 1; j--)
                {
                    MathExprNodeType toptype = OperatorStack.back().type;
                    string toprepr = OperatorStack.back().repr;
                    
                    if(toptype == MathExprNodeType_Function || !IsOperatorWithGreaterPrecedence(nodes[i].repr, toprepr))
                    {
                        OutputQueue.push_back(OperatorStack.back());
                        OperatorStack.pop_back();
                    }
                    else
                        break;
                }
            }
            
            OperatorStack.push_back(nodes[i]);
            
        }
    }
    
    for(size_t i = OperatorStack.size(); i >= 1; i--)
        OutputQueue.push_back(OperatorStack[i - 1]);
    
    results.resize(0);
    results.insert(results.end(), OutputQueue.begin(), OutputQueue.end());
    
    return true;
}
size_t MathExpression::GetSegmentSize()
{
    // to-do: calculate segment size according to available memory.
    return 128 * 1024 * 1;  // 1MB for 131,072 doubles
}
bool MathExpression::EvaluateEx(vector<double>& results, map<string, MathExprNodeEvalTaskBuffer>& bindings, const vector<MathExpressionNode>& nodes)
{
    results.resize(0);
    
    vector<vector<double> > OutputQueue;
    OutputQueue.reserve(nodes.size());
    
    for(size_t i = 0; i < nodes.size(); i++)
    {
        MathExprNodeType nodetype = nodes[i].type;
        if(nodetype == MathExprNodeType_Number)
            OutputQueue.push_back(nodes[i].values);
        else if(nodetype == MathExprNodeType_Symbol)
        {
            map<string, MathExprNodeEvalTaskBuffer>::iterator it = bindings.find(string(nodes[i].repr));
            if(it == bindings.end())
                return false;
            OutputQueue.push_back(vector<double>(it->second.p, it->second.p + it->second.n));
        }
        else if(nodetype == MathExprNodeType_Separator)
        {
            // should not be pushed to nodes previously, just ignore it here without error checking
            continue;
        }
        else if(nodetype == MathExprNodeType_Function)
        {
            size_t nOutputQueues = OutputQueue.size();
            if(!nOutputQueues)
                return false;
            
            std::map<std::string, MathFunction_1>::iterator it1 = m_f1.find(std::string(nodes[i].repr));
            std::map<std::string, MathFunction_2>::iterator it2 = m_f2.find(std::string(nodes[i].repr));
            if(it1 != m_f1.end())
            {
                if(nOutputQueues < 1)
                    return false;
                EvalMathFunction_1(it1->second, OutputQueue[nOutputQueues - 1], OutputQueue[nOutputQueues - 1]);
            }
            else if(it2 != m_f2.end())
            {
                if(nOutputQueues < 2)
                    return false;
                EvalMathFunction_2(it2->second, OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 1]);
                OutputQueue.pop_back();
            }
            else
                return false;
        }
        else if(nodetype == MathExprNodeType_Operator)
        {
            size_t nOutputQueues = OutputQueue.size();
            if(nodes[i].repr == "+" || nodes[i].repr == "-" || nodes[i].repr == "*" || nodes[i].repr == "/" || nodes[i].repr == "^")
            {
                if(nOutputQueues < 2)
                    return false;
                size_t nOperand1Size = OutputQueue[nOutputQueues - 2].size();
                size_t nOperand2Size = OutputQueue[nOutputQueues - 1].size();
                if(!nOperand1Size || !nOperand2Size)
                    return false;
                
                if(nodes[i].repr == "+")
                    EvalMathFunction_2([](double A, double B){return A + B;}, OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 1]);
                else if(nodes[i].repr == "-")
                    EvalMathFunction_2([](double A, double B){return A - B;}, OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 1]);
                else if(nodes[i].repr == "*")
                    EvalMathFunction_2([](double A, double B){return A * B;}, OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 1]);
                else if(nodes[i].repr == "/")
                    EvalMathFunction_2([](double A, double B){return A / B;}, OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 1]);
                else if(nodes[i].repr == "^")
                    EvalMathFunction_2([](double A, double B){return pow(A, B);}, OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 2], OutputQueue[nOutputQueues - 1]);
                
                OutputQueue.pop_back();
            }
            else
                return false;
        }
        else if(nodetype == MathExprNodeType_Sign)
        {
            if(!OutputQueue.size())
                return false;
            if(nodes[i].repr == "-")
            {
                vector<double>& values = OutputQueue.back();
                for(size_t j = 0; j < values.size(); j++)
                    values[j] = -values[j];
            }
            else if(nodes[i].repr != "+")
                return false;
        }
    }
    
    
    if(OutputQueue.size() != 1)
        return false;
    
    results = OutputQueue[0];
    
    return true;
}
void MathExpression::initialize_f1()
{
    m_f1["acos"] = acos;
    m_f1["asin"] = asin;
    m_f1["atan"] = atan;
    m_f1["cos"] = cos;
    m_f1["cosh"] = cosh;
    m_f1["exp"] = exp;
    m_f1["abs"] = abs;
    m_f1["log"] = log;
    m_f1["log10"] = log10;
    m_f1["ln"] = [](double _){ return log(_)/log(exp(1)); };
    m_f1["sin"] = sin;
    m_f1["sinh"] = sinh;
    m_f1["tan"] = tan;
    m_f1["tanh"] = tanh;
    m_f1["sqrt"] = sqrt;
#ifdef _MSC_VER
    m_f1["j0"] = _j0;
    m_f1["j1"] = _j1;
    m_f1["y0"] = _y0;
    m_f1["y1"] = _y1;
#elif defined __GNUC__
    m_f1["j0"] = j0;
    m_f1["j1"] = j1;
    m_f1["y0"] = y0;
    m_f1["y1"] = y1;
#endif
}
void MathExpression::initialize_f2()
{
    m_f2["atan2"] = atan2;
}
void MathExpression::initialize_constants()
{
    map<string, double> constants;
    constants["pi"] = M_PI;
    constants["PI"] = M_PI;
    BindSymbols(constants);
    return;
}
