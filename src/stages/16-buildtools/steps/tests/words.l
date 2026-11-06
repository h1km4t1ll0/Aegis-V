/* rung build1: flex -> tcc -> run (16-buildtools README) */
%option noyywrap
%{
#include <stdio.h>
int words = 0, lines = 0;
%}
%%
[a-zA-Z]+	{ words++; }
\n	{ lines++; }
.	{ }
%%
int main(void)
{
    yylex();
    printf("flex words %d lines %d\n", words, lines);
    return 0;
}
