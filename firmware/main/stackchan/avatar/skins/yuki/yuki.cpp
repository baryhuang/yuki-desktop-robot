/*
 * SPDX-FileCopyrightText: 2026 Bury Huang
 *
 * SPDX-License-Identifier: MIT
 */
#include "yuki.h"

#include <algorithm>
#include <string>

using namespace stackchan::avatar;
using namespace uitk;
using namespace uitk::lvgl_cpp;

namespace {

constexpr lv_color_t kHair = LV_COLOR_MAKE(68, 88, 124);
constexpr lv_color_t kHairShadow = LV_COLOR_MAKE(43, 57, 88);
constexpr lv_color_t kSkin = LV_COLOR_MAKE(255, 224, 211);
constexpr lv_color_t kSkinShadow = LV_COLOR_MAKE(239, 183, 176);
constexpr lv_color_t kEyeOutline = LV_COLOR_MAKE(50, 56, 75);
constexpr lv_color_t kIris = LV_COLOR_MAKE(87, 177, 211);
constexpr lv_color_t kIrisDark = LV_COLOR_MAKE(35, 91, 132);
constexpr lv_color_t kMouth = LV_COLOR_MAKE(126, 51, 69);
constexpr lv_color_t kTongue = LV_COLOR_MAKE(244, 126, 143);
constexpr lv_color_t kBlush = LV_COLOR_MAKE(242, 127, 142);

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
    container_ = make_shape(parent, 68, 48, lv_color_black(), LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, LV_PART_MAIN);

    brow_ = make_shape(parent, 50, 5, kHairShadow, LV_RADIUS_CIRCLE);
    lv_obj_set_style_transform_pivot_x(brow_, 25, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(brow_, 2, LV_PART_MAIN);

    white_ = make_shape(container_, 58, 36, lv_color_white(), LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(white_, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(white_, kEyeOutline, LV_PART_MAIN);

    iris_ = make_shape(container_, 29, 32, kIris, LV_RADIUS_CIRCLE);
    pupil_ = make_shape(container_, 14, 24, kIrisDark, LV_RADIUS_CIRCLE);
    highlight_large_ = make_shape(container_, 8, 10, lv_color_white(), LV_RADIUS_CIRCLE);
    highlight_small_ = make_shape(container_, 4, 4, lv_color_white(), LV_RADIUS_CIRCLE);

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
            setWeight(78);
            setRotation(is_left_eye_ ? 120 : 3480);
            break;
        case Emotion::Angry:
            setWeight(72);
            setRotation(is_left_eye_ ? 3500 : 100);
            break;
        case Emotion::Sad:
            setWeight(68);
            setRotation(is_left_eye_ ? 180 : 3420);
            break;
        case Emotion::Doubt:
            setWeight(is_left_eye_ ? 92 : 62);
            setRotation(is_left_eye_ ? 60 : 3540);
            break;
        case Emotion::Sleepy:
            setWeight(24);
            setRotation(0);
            break;
        case Emotion::Neutral:
        default:
            setWeight(100);
            setRotation(0);
            break;
    }
}

void YukiEyes::setVisible(bool visible)
{
    Element::setVisible(visible);
    if (visible) {
        lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(brow_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(brow_, LV_OBJ_FLAG_HIDDEN);
    }
}

void YukiEyes::setSize(int size)
{
    Feature::setSize(size);
}

void YukiEyes::_update()
{
    current_weight_ = approach(current_weight_, _weight, 24);
    current_size_ = approach(current_size_, _size, 8);
    current_rotation_ = approach(current_rotation_, _rotation, 90);
    current_position_.x = approach(current_position_.x, _position.x, 8);
    current_position_.y = approach(current_position_.y, _position.y, 8);
    apply();
}

void YukiEyes::apply()
{
    const int base_x = is_left_eye_ ? -54 : 54;
    const int eye_y = -5;
    const int gaze_x = map_value(current_position_.x, -100, 100, -8, 8);
    const int gaze_y = map_value(current_position_.y, -100, 100, -5, 6);
    const int open_height = map_value(current_weight_, 0, 100, 4, 36);
    const int iris_size = map_value(current_size_, -100, 100, 22, 34);
    const int iris_height = std::min(iris_size + 3, std::max(4, open_height - 2));

    align_center(container_, base_x, eye_y);
    align_center(brow_, base_x, eye_y - 32);
    lv_obj_set_style_transform_rotation(brow_, current_rotation_, LV_PART_MAIN);

    lv_obj_set_size(white_, 58, open_height);
    align_center(white_, 0, 0);

    lv_obj_set_size(iris_, iris_size, iris_height);
    align_center(iris_, gaze_x, gaze_y);
    lv_obj_set_size(pupil_, 13, std::min(23, iris_height));
    align_center(pupil_, gaze_x, gaze_y + 2);
    align_center(highlight_large_, gaze_x - 5, gaze_y - 6);
    align_center(highlight_small_, gaze_x + 5, gaze_y + 6);

    const bool show_details = open_height > 9;
    if (show_details) {
        lv_obj_remove_flag(iris_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(highlight_large_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(highlight_small_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(iris_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(highlight_large_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(highlight_small_, LV_OBJ_FLAG_HIDDEN);
    }
}

YukiMouth::YukiMouth(lv_obj_t* parent)
{
    mouth_ = make_shape(parent, 44, 5, kMouth, LV_RADIUS_CIRCLE);
    lv_obj_set_style_transform_pivot_x(mouth_, 22, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(mouth_, 4, LV_PART_MAIN);
    tongue_ = make_shape(mouth_, 28, 9, kTongue, LV_RADIUS_CIRCLE);
    left_corner_ = make_shape(parent, 9, 4, kMouth, LV_RADIUS_CIRCLE);
    right_corner_ = make_shape(parent, 9, 4, kMouth, LV_RADIUS_CIRCLE);

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
    lv_obj_t* parts[] = {mouth_, left_corner_, right_corner_};
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
    current_weight_ = approach(current_weight_, _weight, 14);
    current_rotation_ = approach(current_rotation_, _rotation, 80);
    current_position_.x = approach(current_position_.x, _position.x, 8);
    current_position_.y = approach(current_position_.y, _position.y, 8);
    apply();
}

void YukiMouth::apply()
{
    const int offset_x = map_value(current_position_.x, -100, 100, -10, 10);
    const int offset_y = map_value(current_position_.y, -100, 100, -7, 7);
    const int width = map_value(current_weight_, 0, 100, 42, 32);
    const int height = map_value(current_weight_, 0, 100, 5, 30);
    int base_y = 53 + offset_y;

    if (emotion_ == Emotion::Sad) {
        base_y += 3;
    }

    lv_obj_set_size(mouth_, width, height);
    align_center(mouth_, offset_x, base_y);
    lv_obj_set_style_transform_rotation(mouth_, current_rotation_, LV_PART_MAIN);

    lv_obj_set_size(tongue_, std::max(12, width - 12), std::max(3, height / 3));
    lv_obj_align(tongue_, LV_ALIGN_BOTTOM_MID, 0, 1);
    if (height > 12) {
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
    panel_->setBgColor(LV_COLOR_MAKE(207, 233, 243));
    panel_->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* root = panel_->get();

    lv_obj_t* hair_back = make_shape(root, 236, 236, kHairShadow, 108);
    align_center(hair_back, 0, 8);
    lv_obj_t* hair_left = make_shape(root, 62, 184, kHairShadow, 30);
    align_center(hair_left, -105, 35);
    lv_obj_t* hair_right = make_shape(root, 62, 184, kHairShadow, 30);
    align_center(hair_right, 105, 35);

    lv_obj_t* ear_left = make_shape(root, 28, 54, kSkinShadow, LV_RADIUS_CIRCLE);
    align_center(ear_left, -101, 5);
    lv_obj_t* ear_right = make_shape(root, 28, 54, kSkinShadow, LV_RADIUS_CIRCLE);
    align_center(ear_right, 101, 5);

    lv_obj_t* face = make_shape(root, 190, 206, kSkin, 88);
    align_center(face, 0, 9);

    lv_obj_t* fringe_center = make_shape(root, 76, 76, kHair, 30);
    align_center(fringe_center, 0, -86);
    lv_obj_t* fringe_left = make_shape(root, 88, 58, kHair, 26);
    align_center(fringe_left, -53, -80);
    lv_obj_t* fringe_right = make_shape(root, 88, 58, kHair, 26);
    align_center(fringe_right, 53, -80);
    lv_obj_t* side_lock_left = make_shape(root, 24, 92, kHair, 12);
    align_center(side_lock_left, -88, -34);
    lv_obj_t* side_lock_right = make_shape(root, 24, 92, kHair, 12);
    align_center(side_lock_right, 88, -34);

    blush_left_ = make_shape(root, 34, 10, kBlush, LV_RADIUS_CIRCLE);
    align_center(blush_left_, -66, 38);
    blush_right_ = make_shape(root, 34, 10, kBlush, LV_RADIUS_CIRCLE);
    align_center(blush_right_, 66, 38);
    lv_obj_set_style_bg_opa(blush_left_, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(blush_right_, LV_OPA_40, LV_PART_MAIN);

    lv_obj_t* nose = make_shape(root, 6, 4, kSkinShadow, LV_RADIUS_CIRCLE);
    align_center(nose, 0, 30);

    _key_elements.leftEye = std::make_unique<YukiEyes>(root, true);
    _key_elements.rightEye = std::make_unique<YukiEyes>(root, false);
    _key_elements.mouth = std::make_unique<YukiMouth>(root);
    _key_elements.speechBubble = std::make_unique<YukiSpeechBubble>(root, font);
}

void YukiAvatar::setEmotion(const Emotion& emotion)
{
    Avatar::setEmotion(emotion);

    lv_opa_t blush_opacity = LV_OPA_40;
    if (emotion == Emotion::Happy) {
        blush_opacity = LV_OPA_70;
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
