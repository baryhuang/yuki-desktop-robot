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
    bool tracking_             = false;
};

void StartYukiVision();

}  // namespace stackchan
