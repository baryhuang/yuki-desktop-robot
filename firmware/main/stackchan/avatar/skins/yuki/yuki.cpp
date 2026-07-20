/*
 * SPDX-FileCopyrightText: 2026 Bury Huang
 *
 * SPDX-License-Identifier: MIT
 */
#include "yuki.h"

#include <algorithm>
#include <assets/assets.h>
#include <esp_log.h>
#include <string>

using namespace stackchan::avatar;
using namespace uitk;
using namespace uitk::lvgl_cpp;

namespace {

constexpr lv_color_t kBackground = LV_COLOR_MAKE(224, 234, 229);
constexpr lv_color_t kHairShadow = LV_COLOR_MAKE(91, 43, 35);
constexpr lv_color_t kSkin = LV_COLOR_MAKE(252, 222, 199);
constexpr lv_color_t kMouth = LV_COLOR_MAKE(123, 60, 57);
constexpr lv_color_t kTongue = LV_COLOR_MAKE(235, 133, 127);
constexpr lv_color_t kBlush = LV_COLOR_MAKE(236, 139, 132);

int approach(int current, int target, int step)
{
    if (current < target) {
        return std::min(current + step, target);
    }
    if (current > target) {
        return std::max(current - step, target);
    }
    return current;
}

int map_value(int value, int in_min, int in_max, int out_min, int out_max)
{
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

lv_obj_t* make_shape(lv_obj_t* parent, int width, int height, lv_color_t color, int radius)
{
    lv_obj_t* object = lv_obj_create(parent);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    return object;
}

void align_center(lv_obj_t* object, int x, int y)
{
    lv_obj_align(object, LV_ALIGN_CENTER, x, y);
}

}  // namespace

YukiEyes::YukiEyes(lv_obj_t* parent, bool is_left_eye) : is_left_eye_(is_left_eye)
{
    container_ = make_shape(parent, 38, 26, lv_color_black(), LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, LV_PART_MAIN);

    eyelid_ = make_shape(container_, 36, 1, kSkin, 8);
    closed_line_left_ = make_shape(container_, 17, 2, kHairShadow, LV_RADIUS_CIRCLE);
    closed_line_right_ = make_shape(container_, 17, 2, kHairShadow, LV_RADIUS_CIRCLE);
    lv_obj_set_style_transform_pivot_x(closed_line_left_, 17, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(closed_line_right_, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(closed_line_left_, 3550, LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(closed_line_right_, 50, LV_PART_MAIN);
    lv_obj_add_flag(closed_line_left_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(closed_line_right_, LV_OBJ_FLAG_HIDDEN);

    setWeight(100);
    current_position_ = _position;
    apply();
}

void YukiEyes::setPosition(const Vector2i& position)
{
    Element::setPosition(position);
}

void YukiEyes::setWeight(int weight)
{
    Feature::setWeight(weight);
}

void YukiEyes::setRotation(int rotation)
{
    Element::setRotation(rotation);
}

void YukiEyes::setEmotion(const Emotion& emotion)
{
    if (getIgnoreEmotion()) {
        return;
    }

    switch (emotion) {
        case Emotion::Happy:
            setWeight(100);
            break;
        case Emotion::Angry:
            setWeight(100);
            break;
        case Emotion::Sad:
            setWeight(100);
            break;
        case Emotion::Doubt:
            setWeight(100);
            break;
        case Emotion::Sleepy:
            setWeight(24);
            break;
        case Emotion::Neutral:
        default:
            setWeight(100);
            break;
    }
}

void YukiEyes::setVisible(bool visible)
{
    Element::setVisible(visible);
    if (visible) {
        lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }
}

void YukiEyes::setSize(int size)
{
    Feature::setSize(size);
}

void YukiEyes::_update()
{
    current_weight_ = _weight;
    current_size_ = _size;
    current_rotation_ = _rotation;
    current_position_.x = approach(current_position_.x, _position.x, 8);
    current_position_.y = approach(current_position_.y, _position.y, 8);
    apply();
}

void YukiEyes::apply()
{
    const int base_x = is_left_eye_ ? -31 : 29;
    const int eye_y = -11;
    const int covered_height = current_weight_ >= 75 ? 0 : (current_weight_ >= 25 ? 10 : 21);

    align_center(container_, base_x, eye_y);

    if (covered_height > 0) {
        lv_obj_set_size(eyelid_, 36, covered_height);
        lv_obj_align(eyelid_, LV_ALIGN_TOP_MID, 0, 2);
        lv_obj_remove_flag(eyelid_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(eyelid_, LV_OBJ_FLAG_HIDDEN);
    }

    if (current_weight_ < 18) {
        align_center(closed_line_left_, -8, 1);
        align_center(closed_line_right_, 8, 1);
        lv_obj_remove_flag(closed_line_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(closed_line_right_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(closed_line_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(closed_line_right_, LV_OBJ_FLAG_HIDDEN);
    }
}

YukiMouth::YukiMouth(lv_obj_t* parent)
{
    mouth_mask_ = make_shape(parent, 38, 18, kSkin, LV_RADIUS_CIRCLE);
    mouth_ = make_shape(parent, 28, 3, kMouth, LV_RADIUS_CIRCLE);
    lv_obj_set_style_transform_pivot_x(mouth_, 14, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(mouth_, 4, LV_PART_MAIN);
    tongue_ = make_shape(mouth_, 18, 7, kTongue, LV_RADIUS_CIRCLE);
    left_corner_ = make_shape(parent, 6, 3, kMouth, LV_RADIUS_CIRCLE);
    right_corner_ = make_shape(parent, 6, 3, kMouth, LV_RADIUS_CIRCLE);

    current_position_ = _position;
    apply();
}

void YukiMouth::setPosition(const Vector2i& position)
{
    Element::setPosition(position);
}

void YukiMouth::setWeight(int weight)
{
    Feature::setWeight(weight);
}

void YukiMouth::setRotation(int rotation)
{
    Element::setRotation(rotation);
}

void YukiMouth::setEmotion(const Emotion& emotion)
{
    emotion_ = emotion;
    switch (emotion) {
        case Emotion::Happy:
            setWeight(16);
            break;
        case Emotion::Angry:
            setWeight(8);
            break;
        case Emotion::Sad:
            setWeight(5);
            break;
        case Emotion::Doubt:
            setWeight(10);
            break;
        case Emotion::Sleepy:
            setWeight(4);
            break;
        case Emotion::Neutral:
        default:
            setWeight(0);
            break;
    }
}

void YukiMouth::setVisible(bool visible)
{
    Element::setVisible(visible);
    lv_obj_t* parts[] = {mouth_mask_, mouth_, left_corner_, right_corner_};
    for (auto* part : parts) {
        if (visible) {
            lv_obj_remove_flag(part, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void YukiMouth::_update()
{
    current_weight_ = _weight;
    current_rotation_ = approach(current_rotation_, _rotation, 80);
    current_position_.x = approach(current_position_.x, _position.x, 8);
    current_position_.y = approach(current_position_.y, _position.y, 8);
    apply();
}

void YukiMouth::apply()
{
    const int offset_x = map_value(current_position_.x, -100, 100, -10, 10);
    const int offset_y = map_value(current_position_.y, -100, 100, -7, 7);
    const int mouth_frame = current_weight_ < 25 ? 0 : (current_weight_ < 80 ? 1 : 2);
    int width = mouth_frame == 0 ? 26 : (mouth_frame == 1 ? 24 : 28);
    int height = mouth_frame == 0 ? 2 : (mouth_frame == 1 ? 10 : 21);
    int base_y = 29 + offset_y;

    if (emotion_ == Emotion::Happy) {
        width += mouth_frame == 0 ? 3 : 6;
    } else if (emotion_ == Emotion::Angry) {
        width -= 2;
    } else if (emotion_ == Emotion::Doubt && mouth_frame == 2) {
        width = 18;
        height = 21;
    }

    if (emotion_ == Emotion::Sad) {
        base_y += 3;
    }

    align_center(mouth_mask_, offset_x, base_y);
    lv_obj_set_size(mouth_, width, height);
    align_center(mouth_, offset_x, base_y);
    lv_obj_set_style_transform_rotation(mouth_, current_rotation_, LV_PART_MAIN);

    lv_obj_set_size(tongue_, std::max(12, width - 12), std::max(3, height / 3));
    lv_obj_align(tongue_, LV_ALIGN_BOTTOM_MID, 0, 1);
    if (height > 10) {
        lv_obj_remove_flag(tongue_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(tongue_, LV_OBJ_FLAG_HIDDEN);
    }

    const int corner_y = emotion_ == Emotion::Happy ? base_y - 2 : base_y + 1;
    align_center(left_corner_, offset_x - width / 2, corner_y);
    align_center(right_corner_, offset_x + width / 2, corner_y);
    lv_obj_set_style_transform_rotation(left_corner_, emotion_ == Emotion::Happy ? 250 : 0, LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(right_corner_, emotion_ == Emotion::Happy ? 3350 : 0, LV_PART_MAIN);
}

YukiSpeechBubble::YukiSpeechBubble(lv_obj_t* parent, const lv_font_t* font)
{
    bubble_ = make_shape(parent, 294, 42, LV_COLOR_MAKE(247, 252, 255), 10);
    lv_obj_set_style_bg_opa(bubble_, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(bubble_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(bubble_, LV_COLOR_MAKE(116, 151, 176), LV_PART_MAIN);
    lv_obj_align(bubble_, LV_ALIGN_BOTTOM_MID, 0, -7);

    label_ = lv_label_create(bubble_);
    lv_obj_set_width(label_, 274);
    lv_label_set_long_mode(label_, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(label_, LV_COLOR_MAKE(37, 54, 74), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_, font, LV_PART_MAIN);
    lv_obj_center(label_);
    lv_obj_add_flag(bubble_, LV_OBJ_FLAG_HIDDEN);
}

void YukiSpeechBubble::setSpeech(std::string_view text)
{
    std::string owned(text);
    lv_label_set_text(label_, owned.c_str());
    setVisible(!owned.empty());
}

void YukiSpeechBubble::clearSpeech()
{
    lv_label_set_text(label_, "");
    setVisible(false);
}

void YukiSpeechBubble::setVisible(bool visible)
{
    Element::setVisible(visible);
    if (visible) {
        lv_obj_remove_flag(bubble_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(bubble_);
    } else {
        lv_obj_add_flag(bubble_, LV_OBJ_FLAG_HIDDEN);
    }
}

void YukiSpeechBubble::setTextFont(void* font)
{
    if (font) {
        lv_obj_set_style_text_font(label_, static_cast<const lv_font_t*>(font), LV_PART_MAIN);
    }
}

void YukiAvatar::init(lv_obj_t* parent, const lv_font_t* font)
{
    panel_ = std::make_unique<Container>(parent);
    panel_->align(LV_ALIGN_CENTER, 0, 0);
    panel_->setSize(320, 240);
    panel_->setRadius(0);
    panel_->setBorderWidth(0);
    panel_->setBgColor(kBackground);
    panel_->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* root = panel_->get();

    portrait_ = assets::get_image("yuki_reference.bin");
    if (portrait_.data_size != 0) {
        portrait_object_ = lv_image_create(root);
        lv_image_set_src(portrait_object_, &portrait_);
        lv_obj_center(portrait_object_);
    } else {
        ESP_LOGE("YukiAvatar", "Yuki portrait is missing from the assets partition");
    }

    blush_left_ = make_shape(root, 34, 10, kBlush, LV_RADIUS_CIRCLE);
    align_center(blush_left_, -57, 31);
    blush_right_ = make_shape(root, 34, 10, kBlush, LV_RADIUS_CIRCLE);
    align_center(blush_right_, 57, 31);
    lv_obj_set_style_bg_opa(blush_left_, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(blush_right_, LV_OPA_20, LV_PART_MAIN);

    _key_elements.leftEye = std::make_unique<YukiEyes>(root, true);
    _key_elements.rightEye = std::make_unique<YukiEyes>(root, false);
    _key_elements.mouth = std::make_unique<YukiMouth>(root);
    _key_elements.speechBubble = std::make_unique<YukiSpeechBubble>(root, font);
}

void YukiAvatar::setEmotion(const Emotion& emotion)
{
    Avatar::setEmotion(emotion);

    lv_opa_t blush_opacity = LV_OPA_20;
    if (emotion == Emotion::Happy) {
        blush_opacity = LV_OPA_40;
    } else if (emotion == Emotion::Angry) {
        blush_opacity = LV_OPA_20;
    } else if (emotion == Emotion::Sleepy) {
        blush_opacity = LV_OPA_10;
    }
    lv_obj_set_style_bg_opa(blush_left_, blush_opacity, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(blush_right_, blush_opacity, LV_PART_MAIN);
}

Container* YukiAvatar::getPanel() const
{
    return panel_.get();
}
