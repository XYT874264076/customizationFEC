#ifndef EXCEPTION_HH
#define EXCEPTION_HH

#include <cassert>
#include <system_error>

class unix_error : public std::system_error
{
public:
  unix_error()
    : system_error(errno, std::system_category()) {}

  unix_error(const std::string & tag)
    : system_error(errno, std::system_category(), tag) {}
};

inline int check_syscall(const int return_value)
{
  assert(return_value >= 0 && "System call failed");
  return return_value;
}

inline int check_syscall(const int return_value, const std::string & tag)
{
  assert(return_value >= 0 && "System call failed");
  return return_value;
}

#endif /* EXCEPTION_HH */
