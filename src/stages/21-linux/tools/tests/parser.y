%{
#include <stdio.h>
#include <ctype.h>
static int result;
int yylex(void);
void yyerror(const char *s);
%}
%token NUMBER
%left '+'
%left '*'
%%
input: expr '\n' { result = $1; };
expr: NUMBER | expr '+' expr { $$ = $1 + $3; }
             | expr '*' expr { $$ = $1 * $3; }
             | '(' expr ')' { $$ = $2; };
%%
int yylex(void)
{
    int c;
    do c = getchar(); while (c == ' ' || c == '\t');
    if (c == EOF) return 0;
    if (isdigit(c)) {
        yylval = 0;
        do {
            yylval = yylval * 10 + c - '0';
            c = getchar();
        } while (c != EOF && isdigit(c));
        if (c != EOF) ungetc(c, stdin);
        return NUMBER;
    }
    return c;
}
void yyerror(const char *s) { fprintf(stderr, "%s\n", s); }
int main(void)
{
    if (yyparse()) return 1;
    printf("%d\n", result);
    return 0;
}
