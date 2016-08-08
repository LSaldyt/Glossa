#include "Parse.hpp"

namespace Parse
{
    StatementParser statementBuilder(std::function<std::shared_ptr<Statement>(SymbolicTokens&)> builder);

    std::tuple<bool, std::shared_ptr<Statement>>              buildStatement (SymbolicTokens& tokens);
    std::tuple<bool, std::vector<std::shared_ptr<Statement>>> buildStatements(SymbolicTokens& tokens);

    std::tuple<bool, Expression> buildExpression(SymbolicTokens& tokens);

    const auto buildAssignment = statementBuilder([=](SymbolicTokens& tokens){
        Assignment a;
        bindIdent(a.identifier, tokens);
        advance(parseOp("="),   tokens);
        bindExpression(a.value, tokens);
        advance(parseOp("\n"),  tokens);
        return std::make_shared<Assignment>(a);
        });

    const auto buildFunction = statementBuilder([=](SymbolicTokens& tokens){
        Function f;
        bindIdent(f.identifier,             tokens);
        advance(parseOp("("),               tokens);
        bindArgnames(f.argnames,            tokens);
        advance(parseOp(")"),               tokens);
        advance(parseOp(":"),               tokens);
        bindStatements(f.body,              tokens);
        advance(parseOp("return"),          tokens);
        bindExpression(f.return_expression, tokens);
        return std::make_shared<Function>(f);
    });


    const auto buildFunctionCall = statementBuilder([=](SymbolicTokens& tokens){
        FunctionCall fc;
        bindIdent(fc.identifier, tokens);
        advance(parseOp("("),   tokens);
        bindArgnames(fc.args,    tokens);
        advance(parseOp(")"),   tokens);
        return std::make_shared<FunctionCall>(fc);
    });

    const std::vector<StatementParser> statementBuilders = 
    {
        buildAssignment, buildFunction, buildFunctionCall // Larger parsers should come first
    };
}
