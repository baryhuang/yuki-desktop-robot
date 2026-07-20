/*
 * SPDX-FileCopyrightText: 2026 Bury Huang
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <string>

namespace stackchan {

struct YukiCuriosityConfig {
    bool enabled = true;
    int interval_minutes = 30;
    std::string interests = "AI, robotics, science, and creative technology";
};

void StartYukiCuriosity();
YukiCuriosityConfig GetYukiCuriosityConfig();
void SetYukiCuriosityConfig(const YukiCuriosityConfig& config);
void RequestYukiCuriosityNow();

}  // namespace stackchan
