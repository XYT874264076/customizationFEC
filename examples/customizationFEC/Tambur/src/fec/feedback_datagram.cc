#include <stdexcept>

#include "examples/customizationFEC/Tambur/src/fec/feedback_datagram.hh"
#include "examples/customizationFEC/Tambur/src/util/serialization.hh"

using namespace std;

bool FeedbackDatagram::parse_from_string(const string & binary)
{
  if (binary.size() < feedback_size()) {
    return false; // datagram is too small to contain a header
  }
  WireParser parser(binary);
  qr = parser.read_uint8();
  return true;
}

string FeedbackDatagram::serialize_to_string() const
{
  string binary;
  binary.reserve(feedback_size());
  binary += put_number(qr);
  return binary;
}
