/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "../modifiable.h"
#include "../utils/random.h"
#include <hal/hal.h>
#include <cstdint>

namespace stackchan {

/**
 * @brief
 *
 */
class BlinkModifier : public Modifier {
public:
    /**
     * @param destroyAfterMs 持续多久后停止眨眼并销毁（0 为永久）
     * @param openIntervalMs 睁眼持续时间
     * @param closeIntervalMs 闭眼持续时间（瞬间）
     */
    BlinkModifier(uint32_t destroyAfterMs = 0, uint32_t openIntervalMs = 5200, uint32_t closeIntervalMs = 100)
        : _open_interval_ms(openIntervalMs), _close_interval_ms(closeIntervalMs)
    {
        uint32_t now = GetHAL().millis();

        // 处理销毁计时
        if (destroyAfterMs > 0) {
            _destroy_at   = now + destroyAfterMs;
            _has_lifetime = true;
        }

        // 初始化：从睁眼状态开始，立即准备闭眼
        _state           = State::Open;
        _next_state_tick = now + next_open_hold();
    }

    void resyncEyeWeights()
    {
        _needs_resync = true;
    }

    void _update(Modifiable& stackchan) override
    {
        if (!stackchan.hasAvatar() || stackchan.avatar().isModifyLocked()) {
            return;
        }

        uint32_t now = GetHAL().millis();

        // 1. 处理销毁逻辑
        if (_has_lifetime && now >= _destroy_at) {
            // 销毁前确保眼睛是睁开的
            if (_state != State::Open) {
                apply_eye_weights(stackchan, _left_eye_weight, _right_eye_weight);
            }
            requestDestroy();
            return;
        }

        // 2. 处理权重同步请求
        // 如果眼睛正闭着，我们只记录权重，等睁眼时再应用
        if (_needs_resync) {
            _needs_resync     = false;
            _left_eye_weight  = stackchan.avatar().leftEye().getWeight();
            _right_eye_weight = stackchan.avatar().rightEye().getWeight();
        }

        // 3. 状态机切换逻辑
        if (now >= _next_state_tick) {
            if (_state == State::Open) {
                _left_eye_weight  = stackchan.avatar().leftEye().getWeight();
                _right_eye_weight = stackchan.avatar().rightEye().getWeight();
                _state           = State::Closing;
                _next_state_tick = now + 50;
                apply_eye_weights(stackchan, 55, 55);
            } else if (_state == State::Closing) {
                _state           = State::Closed;
                _next_state_tick = now + _close_interval_ms;
                apply_eye_weights(stackchan, 0, 0);
            } else if (_state == State::Closed) {
                _state           = State::Opening;
                _next_state_tick = now + 50;
                apply_eye_weights(stackchan, 55, 55);
            } else if (_double_blink_pending) {
                _double_blink_pending = false;
                _state                = State::OpenPause;
                _next_state_tick      = now + 100;
                apply_eye_weights(stackchan, _left_eye_weight, _right_eye_weight);
            } else if (_state == State::OpenPause) {
                _state           = State::Closing;
                _next_state_tick = now + 50;
                apply_eye_weights(stackchan, 55, 55);
            } else {
                _state                = State::Open;
                _double_blink_pending = Random::getInstance().getInt(0, 8) == 0;
                _next_state_tick      = now + next_open_hold();
                apply_eye_weights(stackchan, _left_eye_weight, _right_eye_weight);
            }
        }
    }

private:
    enum class State { Open, Closing, Closed, Opening, OpenPause };

    uint32_t next_open_hold()
    {
        return Random::getInstance().getInt(_open_interval_ms / 2, _open_interval_ms + 1000);
    }

    void apply_eye_weights(Modifiable& stackchan, int left, int right)
    {
        stackchan.avatar().leftEye().setWeight(left);
        stackchan.avatar().rightEye().setWeight(right);
    }

    State _state;
    uint32_t _next_state_tick = 0;
    uint32_t _open_interval_ms;
    uint32_t _close_interval_ms;

    uint32_t _destroy_at  = 0;
    bool _has_lifetime    = false;
    bool _needs_resync    = false;
    bool _double_blink_pending = false;
    int _left_eye_weight  = 100;
    int _right_eye_weight = 100;
};

}  // namespace stackchan
