#ifndef BLOCK_CODE_FACTORY
#define BLOCK_CODE_FACTORY

#include <optional>

#include "examples/customizationFEC/Tambur/src/fec/multi_fec/coding_matrix_info.hh"
#include "examples/customizationFEC/Tambur/src/fec/multi_fec/multi_frame_fec_helpers.hh"
#include "examples/customizationFEC/Tambur/src/fec/multi_fec/streaming_code_helper.hh"

class BlockCodeFactory
{
public:
  BlockCodeFactory(CodingMatrixInfo codingMatrixInfo,
      StreamingCodeHelper streamingCodeHelper, uint16_t delay);

  std::optional<Matrix<int>> & get_generator_matrix() {
    return matrix_;
  }

private:
  std::optional<Matrix<int>> matrix_;
};

#endif /* BLOCK_CODE_FACTORY */
