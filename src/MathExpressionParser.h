#ifndef _MATH_EXPRESSION_PARSER_H_
#define _MATH_EXPRESSION_PARSER_H_

#ifdef _OC_VER
void ParseMathExpression(const char* lpcszMathExpression, vector<string>& symbols);
#else
void ParseMathExpression(const char* lpcszMathExpression, std::set<std::string>& symbols);
#endif

#endif // _MATH_EXPRESSION_PARSER_H_
