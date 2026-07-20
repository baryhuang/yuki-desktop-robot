/*
 * SPDX-FileCopyrightText: 2026 Bury Huang
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "../modifiable.h"

namespace stackchan {

class YukiFaceTrackingModifier : public Modifier {
public:
    void _update(Modifiable& stackchan) override;

private:
    uint32_t next_motion_tick_ = 0;
    uint32_t voice_pause_until_ = 0;
    int last_target_yaw_       = 0;
    int last_target_pitch_     = 30;
    bool tracking_             = false;
    bool head_tracking_        = false;
    bool voice_pause_latched_  = false;
};

void StartYukiVision();
void EnableYukiVision();
bool YukiFaceSeenRecently(uint32_t within_ms);
void YieldYukiFaceTracking(uint32_t duration_ms);

}  // namespace stackchan
