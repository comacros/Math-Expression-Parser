# Math-Expression-Parser
Library to parse a simple math expression.

I developed a simple math expression parser around 4 years ago appplied to the [Simple Fit app](https://www.originlab.com/FileExchange/details.aspx?fid=239). That was later replaced with [math.js](https://mathjs.org/) when converting to an HTML-based app. It's a great JavaScript library and it works really well in the app.

Recently I'm working on another app in which _math expression parser_ may be involved again. This time the app becomes quite large in size after adding 2 DLL files and I'm considering replacing the ~490KB JavaScript File with a smaller one based on the previous work: a simple math expression parser both for Origin C and others like MSVC, GCC etc.

I just initialized this library and more work is required. You are welcome to report issues and recommend features. You may consider buying me a cup of coffee if it helps.

## Basic

To use this library in Origin C, just include the ```MathExpressionParser.h```. Currently there is only one function exported:
```
void ParseMathExpression(const char* lpcszMathExpression, vector<string>& symbols);
```
```lpcszMathExpression```: for example, "sqrt(a^2+b^2)"
```symbols```: symbols in the math expression, in the above case, ["a", "b"]

