#ifndef MODEL_HH
#define MODEL_HH

#include <string>
#include <deque>
#include <iostream>
#include <memory>
#include <vector>

#include "examples/customizationFEC/Tambur/src/fec/quality_reporting/loss_metrics.hh"
#include "examples/customizationFEC/Tambur/src/fec/quality_reporting/input_converter.hh"

class Model
{
public:
  Model(std::string model_fname, uint8_t num_qrs_no_reduce);

  std::vector<double> get_prediction(const std::vector<double> inputs);

  uint8_t get_quality_report(InputConverter & inputConverter,
      const LossMetrics & metrics);

private:
  uint8_t num_qrs_no_reduce_;

  uint8_t convert_prediction_to_qr(std::vector<double> prediction, uint8_t original_qr);
};

#endif /* MODEL_HH */
