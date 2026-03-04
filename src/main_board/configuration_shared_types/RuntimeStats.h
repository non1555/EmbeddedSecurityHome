#pragma once

#include <Arduino.h>

namespace ConfigurationSharedTypes {
namespace RuntimeStats {

extern volatile uint32_t securityEventDrops; // ตัวนับจำนวนอีเวนต์ที่ตกคิวในลูปความปลอดภัย

} // namespace RuntimeStats
} // namespace ConfigurationSharedTypes
