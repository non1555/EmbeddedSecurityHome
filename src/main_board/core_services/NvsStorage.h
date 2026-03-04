#pragma once

#include "configuration_shared_types/Types.h"

namespace CoreServices {

class NvsStorage {
public:
  void begin(); // เปิด namespace ของ NVS (Preferences) สำหรับใช้งานครั้งแรก
  bool loadModeState(ConfigurationSharedTypes::Mode& outLatestMode,
                     bool& outIsNight); // โหลด latest_mode และ is_night ที่เคยบันทึกจาก NVS
  void saveModeState(ConfigurationSharedTypes::Mode latestMode,
                     bool isNight); // บันทึก latest_mode และ is_night ลง NVS

private:
  bool started_ = false; // ระบุว่า begin() ถูกเรียกและพร้อมใช้งานแล้วหรือไม่
};

} // namespace CoreServices
