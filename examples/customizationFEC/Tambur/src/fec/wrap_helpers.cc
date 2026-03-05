#include <stdexcept>
#include "examples/customizationFEC/Tambur/src/fec/wrap_helpers.hh"

using namespace std;

bool order_is_switched(uint16_t next_frame_num,
    uint16_t previous_frame_num)
{
  if (next_frame_num >= previous_frame_num) { return false; }
  return (next_frame_num > 100) or previous_frame_num < (65535 - 100 + 1);
}

bool frames_are_out_of_order(const deque<Frame> & frames)
{
  if (frames.size() < 2) { return false; }
  int16_t previous_frame_num = frames.at(0).get_frame_num();
  for (auto it = frames.cbegin(); it != frames.cend(); ++it) {
    uint16_t next_frame_num = (*it).get_frame_num();
    if (order_is_switched(next_frame_num, previous_frame_num)) {

      printf("frames are out of order, next_frame_num: %u, previous_frame_num: %u\n", next_frame_num, previous_frame_num);

      return true;
    }
    previous_frame_num = next_frame_num;
  }
  return false;
}
