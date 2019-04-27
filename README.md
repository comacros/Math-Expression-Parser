# Math-Expression-Parser
Library to parse a simple math expression.

I developed a simple math expression parser around 4 years ago appplied to the [Simple Fit app](https://www.originlab.com/FileExchange/details.aspx?fid=239). That was later replaced with [math.js](https://mathjs.org/) when converting to an HTML-based app. It's a great JavaScript library and it works really well in the app.

Recently I'm working on another app in which _math expression parser_ may be involved again. This time the app becomes quite large in size after adding 2 DLL files and I'm considering replacing the ~490KB JavaScript File with a smaller one based on the previous work: a simple math expression parser both for Origin C and others like MSVC, GCC etc.

This library is ready for parsing and evaluating currently. More work is required to implement more features. You are welcome to report issues and recommend features. You may consider buying me a cup of coffee if it helps.

## Basic

This library has been developed to be as simple as possible. You can build it from source code simply adding the following 2 files to your project:

* ```src/MathExpression.h```
* ```src/MathExpression.cpp```

By design, it expects vectors as input symbol bindings. Array operation results in a better performance.

#### Example
```
/* Define the expression */
MathExpression me("1 - sin(2 * x) + cos(pi / y)");

/* Get Symbols */
std::set<std::string> symbols;
me.Symbols(symbols); // {"x", "y", "pi"}

/* Get Functions */
std::set<std::string> functions;
me.Functions(functions); // {"sin", "cos"}

/* Bindings */
std::map<std::string, vector<double> > symbols;
symbols["x"] = std::vector<double>(0.1, .2, .3);
symbols["y"] = std::vector<double>(1, 2, 3);
symbols["pi"] = 3.1415926535897932;

/* Evaluating */
std::vector<double> results;
bool bOK = me.Evaluate(results, symbols);
```
Supported Operators:

1. plus ```+```
2. minus ```-```
3. multiply ```*```
4. divide ```/```
5. power ```^```

Supported Functions:

1. ```sin(x)```
2. ```cos(x)```
3. ```tan(x)```
4. ```acos(x)```
5. ```asin(x)```
6. ```atan(x)```
7. ```abs(x)```
8. ```exp(x)```
9. ```sqrt(x)```
10. ```log(x)```
11. ```log10(x)```
12. ```ln(x)```
13. ```sinh(x)```
14. ```cosh(x)```
15. ```tanh(x)```
16. ```j0(x)```
17. ```j1(x)```
18. ```y0(x)```
19. ```y1(x)```
20. ```atan2(x,y)```


## Support for Origin C

This library currently does not fully support Origin C. Alternatively, include the ```MathExpressionParser.h``` to get the symbols list.

```
void ParseMathExpression(
         const char* lpcszMathExpression,
         vector<string>& symbols
);
```

```lpcszMathExpression```: for example, ```"sqrt(a^2+b^2)"```

```symbols```: symbols in the math expression, in the above case, ```["a", "b"]```

I'll export C functions to support it later.
