#pragma once

#include <cstdint>

namespace device_transport {
struct AtParameters {
  uint64_t id{};
  uint16_t sc{};
  uint8_t sd{};
  uint8_t zs{};
  uint8_t nj{};
  uint16_t nw{};
  uint8_t jv{};
  uint8_t jn{};
  uint64_t op{};
  uint16_t oi{};
  uint8_t ch{};
  uint8_t nc{};
  uint64_t sa{};
  uint16_t my{};
  uint64_t da{};
  uint16_t dy{};
  uint8_t ni{};
  uint8_t nh{};
  uint8_t bh{};
  uint8_t ar{};
  uint32_t dd{};
  uint8_t nt{};
  uint8_t no{};
  uint8_t np{};
  uint8_t cr{};
  uint8_t pl{};
  uint8_t pm{};
  uint8_t pp{};
  uint8_t ee{};
  uint8_t eo{};
  uint8_t ky{};
  uint8_t bd{};
  uint8_t nb{};
  uint8_t sb{};
  uint8_t d7{};
  uint8_t d6{};
  uint8_t ap{};
  uint8_t ao{};
  uint8_t sm{};
  uint16_t sn{};
  uint8_t so{};
  uint16_t sp{};
  uint16_t st{};
  uint16_t po{};
  uint16_t vr{};
  uint16_t hv{};
  uint8_t ai{};
  uint8_t db{};
  uint16_t v{};
  uint16_t nr{};
  uint16_t wr{};
  uint32_t sl{};
};
}
