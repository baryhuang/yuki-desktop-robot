/*
 * SPDX-FileCopyrightText: 2026 Bury Huang
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "../../avatar/avatar.h"
#include "../../avatar/elements/feature.h"
#include <lvgl.h>
#include <smooth_lvgl.hpp>
#include <memory>

namespace stackchan::avatar {

class YukiEyes : public Feature {
public:
    YukiEyes(lv_obj_t* parent, bool is_left_eye);

    void setPosition(const uitk::Vector2i& position) override;
    void setWeight(int weight) override;
    void setRotation(int rotation) override;
    void setEmotion(const Emotion& emotion) override;
    void setVisible(bool visible) override;
    void setSize(int size) override;
    void _update() override;

private:
    void apply();

    bool is_left_eye_ = false;
    int current_weight_ = 100;
    int current_size_ = 0;
    int current_rotation_ = 0;
    uitk::Vector2i current_position_;

    lv_obj_t* container_ = nullptr;
    lv_obj_t* eyelid_ = nullptr;
    lv_obj_t* closed_line_left_ = nullptr;
    lv_obj_t* closed_line_right_ = nullptr;
};

class YukiMouth : public Feature {
public:
    explicit YukiMouth(lv_obj_t* parent);

    void setPosition(const uitk::Vector2i& position) override;
    void setWeight(int weight) override;
    void setRotation(int rotation) override;
    void setEmotion(const Emotion& emotion) override;
    void setVisible(bool visible) override;
    void _update() override;

private:
    void apply();

    int current_weight_ = 0;
    int current_rotation_ = 0;
    uitk::Vector2i current_position_;
    Emotion emotion_ = Emotion::Neutral;

    lv_obj_t* mouth_ = nullptr;
    lv_obj_t* tongue_ = nullptr;
    lv_obj_t* mouth_mask_ = nullptr;
    lv_obj_t* left_corner_ = nullptr;
    lv_obj_t* right_corner_ = nullptr;
};

class YukiSpeechBubble : public SpeechBubble {
public:
    YukiSpeechBubble(lv_obj_t* parent, const lv_font_t* font);

    void setSpeech(std::string_view text) override;
    void clearSpeech() override;
    void setVisible(bool visible) override;
    void setTextFont(void* font) override;

private:
    lv_obj_t* bubble_ = nullptr;
    lv_obj_t* label_ = nullptr;
};

class YukiAvatar : public Avatar {
public:
    void init(lv_obj_t* parent, const lv_font_t* font = &lv_font_montserrat_16);
    void setEmotion(const Emotion& emotion) override;
    uitk::lvgl_cpp::Container* getPanel() const;

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> panel_;
    lv_image_dsc_t portrait_ = {};
    lv_obj_t* portrait_object_ = nullptr;
    lv_obj_t* blush_left_ = nullptr;
    lv_obj_t* blush_right_ = nullptr;
};

}  // namespace stackchan::avatar
