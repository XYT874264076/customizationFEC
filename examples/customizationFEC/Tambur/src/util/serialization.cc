#include "examples/customizationFEC/Tambur/src/util/serialization.hh"

using namespace std;

string WireParser::read_string(const size_t len)
{
  assert(len <= str_.size() && "WireParser::read_string(): attempted to read past end");

  string ret { str_.data(), len };

  // move the start of string view forward
  str_.remove_prefix(len);

  return ret;
}

void WireParser::skip(const size_t len)
{
  assert(len <= str_.size() && "WireParser::skip(): attempted to skip past end");

  str_.remove_prefix(len);
}
