#ifndef IV_LV5_RADIO_COLOR_H_
#define IV_LV5_RADIO_COLOR_H_
namespace iv {
namespace lv5 {
namespace radio {

struct Color {
  enum Type {
    CLEAR = 0,
    WHITE = 1,
    BLACK = 2,
    GRAY  = 3
  };
  static const int kMask = 3;
};

} } }  // namespace iv::lv5::radio
#endif  // IV_LV5_RADIO_COLOR_H_
