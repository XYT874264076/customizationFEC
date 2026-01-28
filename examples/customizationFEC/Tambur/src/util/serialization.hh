#ifndef SERIALIZATION_HH
#define SERIALIZATION_HH

#include <cstring>
#include <string>
#include <string_view>
#include <cassert>
#include <arpa/inet.h>

// convert values from network to host byte order
inline uint8_t ntoh(uint8_t net) { return net; }
inline uint16_t ntoh(uint16_t net) { return ntohs(net); }
inline uint32_t ntoh(uint32_t net) { return ntohl(net); }
inline uint64_t ntoh(uint64_t net) { 
  return ((uint64_t)ntohl(net & 0xFFFFFFFF) << 32) | ntohl(net >> 32); 
}

// convert values from host to network byte order
inline uint8_t hton(uint8_t host) { return host; }
inline uint16_t hton(uint16_t host) { return htons(host); }
inline uint32_t hton(uint32_t host) { return htonl(host); }
inline uint64_t hton(uint64_t host) { 
  return ((uint64_t)htonl(host & 0xFFFFFFFF) << 32) | htonl(host >> 32); 
}

// serialize a number in host byte order to binary data on wire
template<typename T>
std::string put_number(const T host)
{
  const T net = hton(host);
  return {reinterpret_cast<const char *>(&net), sizeof(net)};
}

// deserialize binary data received on wire to a number in host byte order
template<typename T>
T get_number(const std::string_view net)
{
  assert(sizeof(T) <= net.size() && "get_number(): read past end");

  T ret;
  memcpy(&ret, net.data(), sizeof(T));

  return ntoh(ret);
}

// get the value of a bit range in the number
// bit numbering is *MSB 0*, e.g., 0x01 is represented as:
// binary: 0 0 0 0 0 0 0 1
// index:  0 1 2 3 4 5 6 7
template<typename T>
T get_bits(const T number, const size_t bit_offset, const size_t bit_len)
{
  const size_t total_bits = sizeof(T) * 8;

  assert(bit_offset + bit_len <= total_bits && "get_bits(): read past end");

  T ret = number >> (total_bits - bit_offset - bit_len);
  ret &= (1 << bit_len) - 1;

  return ret;
}

class WireParser
{
public:
  WireParser(const std::string_view str) : str_(str) {}

  uint8_t read_uint8() { return read<uint8_t>(); }
  uint16_t read_uint16() { return read<uint16_t>(); }
  uint32_t read_uint32() { return read<uint32_t>(); }
  uint64_t read_uint64() { return read<uint64_t>(); }

  std::string read_string(const size_t len);
  std::string read_string() { return read_string(str_.size()); }

  // skip 'len' bytes ahead
  void skip(const size_t len);

private:
  std::string_view str_;

  template<typename T>
  T read()
  {
    assert(sizeof(T) <= str_.size() && "WireParser::read(): read past end");

    T ret;
    memcpy(&ret, str_.data(), sizeof(T));

    // move the start of string view forward
    str_.remove_prefix(sizeof(T));

    return ntoh(ret);
  }
};

#endif /* SERIALIZATION_HH */
