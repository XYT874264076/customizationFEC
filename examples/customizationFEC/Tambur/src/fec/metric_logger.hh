#ifndef METRIC_LOGGER_HH
#define METRIC_LOGGER_HH

#include <string>

#include "examples/customizationFEC/Tambur/src/fec/frame.hh"

class MetricLogger
{
public:
  virtual ~MetricLogger() = default;
  
  virtual void begin_function(const std::string & fname) = 0;

  virtual void end_function(std::string extra_var) = 0;

  virtual void add_frame(__attribute__((unused)) const Frame& frame) {}
};

#endif /* METRIC_LOGGER_HH */
