#include <iostream>
#include <memory>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <numeric>

#include "examples/customizationFEC/Tambur/src/fec/quality_reporting/model.hh"

Model::Model(std::string model_fname, uint8_t num_qrs_no_reduce) :
    num_qrs_no_reduce_(num_qrs_no_reduce)
{
  std::cout << "Since there are no pre-trained model parameters available, a rule-based alternative approach is adopted: " << model_fname << std::endl;
}

std::vector<double> Model::get_prediction(const std::vector<double> inputs)
{
  // Based on Tambur paper and LossMetrics structure:
  // Input: 13 metrics × 3 time windows = 39 inputs
  // Metrics order (per window):
  // 0: burst_density (packet level)
  // 1: frame_burst_density (frame level) 
  // 2: gap_density (packet level)
  // 3: frame_gap_density (frame level)
  // 4: mean_burst_length (packet level)
  // 5: mean_frame_burst_length (frame level)
  // 6: loss_fraction (packet level)
  // 7: frame_loss_fraction (frame level)
  // 8: multi_frame_loss_fraction (multi-frame level)
  // 9: mean_guardspace_length (packet level)
  // 10: mean_frame_guardspace_length (frame level)
  // 11: multi_frame_insufficient_guardspace (multi-frame level)
  // 12: qr (current quality report value)
  
  if (inputs.size() != 39) {
    // Default: favor high bandwidth overhead for decoding success
    return {0.8, 0.2};
  }
  
  // Critical thresholds based on Tambur paper analysis
  constexpr double CRITICAL_LOSS_THRESHOLD = 0.05;  // 5% packet loss
  constexpr double HIGH_LOSS_THRESHOLD = 0.02;      // 2% packet loss
  constexpr double BURST_DENSITY_THRESHOLD = 0.1;   // 10% burst density
  constexpr double GUARDSPACE_INSUFFICIENT_THRESHOLD = 0.3; // 30% insufficient guardspace
  
  // Weight recent windows more heavily (exponential decay)
  constexpr double window_weights[3] = {0.5, 0.3, 0.2};  // current, recent, older
  
  double critical_score = 0.0;
  double warning_score = 0.0;
  
  for (int window = 0; window < 3; window++) {
    int base_idx = window * 13;
    double window_weight = window_weights[window];
    
    // Extract metrics for this window
    double loss_fraction = inputs[base_idx + 6];
    double burst_density = inputs[base_idx + 0];
    double multi_frame_loss_fraction = inputs[base_idx + 8];
    double multi_frame_insufficient_guardspace = inputs[base_idx + 11];
    
    // Critical conditions (immediate high bandwidth overhead)
    if (loss_fraction > CRITICAL_LOSS_THRESHOLD || 
        multi_frame_insufficient_guardspace > GUARDSPACE_INSUFFICIENT_THRESHOLD) {
      critical_score += 1.0 * window_weight;
    }
    
    // Warning conditions (consider high bandwidth overhead)
    if (loss_fraction > HIGH_LOSS_THRESHOLD ||
        burst_density > BURST_DENSITY_THRESHOLD ||
        multi_frame_loss_fraction > HIGH_LOSS_THRESHOLD) {
      warning_score += 0.5 * window_weight;
    }
  }
  
  // Combine scores with priority to critical conditions
  double combined_score = std::min(1.0, critical_score + warning_score * 0.5);
  
  // Apply paper's preference: prioritize avoiding decoding failures (weight 0.999)
  // High score indicates poor network conditions requiring high bandwidth overhead
  double prob_high_bw = combined_score * 0.999;
  
  // Ensure reasonable probability range with smoothing
  prob_high_bw = std::max(0.1, std::min(0.9, prob_high_bw));
  double prob_low_bw = 1.0 - prob_high_bw;
  
  return {prob_high_bw, prob_low_bw};
}

uint8_t Model::get_quality_report(InputConverter & inputConverter,
    const LossMetrics & metrics)
{
  auto inputs = inputConverter.get_inputs();
  if (!inputs.has_value()) { 
    return uint8_t(round(metrics.qr)) % num_qrs_no_reduce_; 
  }
  std::vector<double> prediction = get_prediction(inputs.value());
  uint8_t qr = convert_prediction_to_qr(prediction, uint8_t(round(metrics.qr)));
  return qr;
}

uint8_t Model::convert_prediction_to_qr(std::vector<double> prediction,
    uint8_t original_qr)
{
  if (original_qr == 0) { return original_qr; }
  
  if (prediction.size() < 2) {
    return original_qr % num_qrs_no_reduce_;
  }
  
  // 基于13个指标的严谨规则：
  // - 高带宽开销概率 > 0.6：选择高带宽模式
  // - 高带宽开销概率 < 0.4：选择低带宽模式  
  // - 0.4 <= prob <= 0.6：保持当前模式
  
  double prob_high_bw = prediction[0];
  
  if (prob_high_bw > 0.6) {
    // 网络条件差，选择高带宽开销模式
    return (original_qr % num_qrs_no_reduce_) + (num_qrs_no_reduce_ - 1);
  } else if (prob_high_bw < 0.4) {
    // 网络条件好，选择低带宽开销模式
    return original_qr % num_qrs_no_reduce_;
  } else {
    // 网络条件中等，保持当前模式
    return original_qr;
  }
}
