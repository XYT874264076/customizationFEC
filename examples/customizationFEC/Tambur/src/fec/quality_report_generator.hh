#ifndef QUALITY_REPORT_GENERATOR_HH
#define QUALITY_REPORT_GENERATOR_HH

#include "examples/customizationFEC/Tambur/src/fec/quality_reporting/loss_metrics.hh"

class QualityReportGenerator
{
public:
  virtual uint8_t get_quality_report(LossMetrics lossMetrics) = 0;
  virtual ~QualityReportGenerator() = default;
};

#endif /* QUALITY_REPORT_GENERATOR_HH */
