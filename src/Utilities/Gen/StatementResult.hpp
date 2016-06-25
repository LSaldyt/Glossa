#include "Import.hpp"

namespace Gen{
  struct StatementResult
  {
      bool result;
      SymbolicTokens parsed;
      std::shared_ptr<Statement> statement;
      StatementResult(bool set_result, SymbolicTokens set_parsed,
                      std::shared_ptr<Statement> set_statement)
      {
          result    = set_result;
          parsed    = set_parsed;
          statement = set_statement;
      }
  };
}
