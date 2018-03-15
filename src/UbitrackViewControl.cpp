//
// Created by Ulrich Eck on 15.03.18.
//


#include "basic_facade_demo/UbitrackViewControl.h"

#include <IO/ClassIO/IJsonConvertibleIO.h>

namespace three{

void UbitrackViewControl::Reset()
{
    if (animation_mode_ == AnimationMode::FreeMode) {
        ViewControl::Reset();
    }
}

void UbitrackViewControl::ChangeFieldOfView(double step)
{
    if (animation_mode_ == AnimationMode::FreeMode) {
        if (!view_trajectory_.view_status_.empty()) {
            // Once editing starts, lock ProjectionType.
            // This is because ProjectionType cannot be easily switched in a
            // smooth trajectory.
            if (GetProjectionType() == ProjectionType::Perspective) {
                double old_fov = field_of_view_;
                ViewControl::ChangeFieldOfView(step);
                if (GetProjectionType() == ProjectionType::Orthogonal) {
                    field_of_view_ = old_fov;
                }
            } else {
                // do nothing, lock as ProjectionType::Orthogonal
            }
            SetProjectionParameters();
        } else {
            ViewControl::ChangeFieldOfView(step);
        }
    }
}

void UbitrackViewControl::Scale(double scale)
{
    if (animation_mode_ == AnimationMode::FreeMode) {
        ViewControl::Scale(scale);
    }
}

void UbitrackViewControl::Rotate(double x, double y, double xo,
        double yo)
{
    if (animation_mode_ == AnimationMode::FreeMode) {
        ViewControl::Rotate(x, y, xo, yo);
    }
}

void UbitrackViewControl::Translate(double x, double y, double xo,
        double yo)
{
    if (animation_mode_ == AnimationMode::FreeMode) {
        ViewControl::Translate(x, y, xo, yo);
    }
}

void UbitrackViewControl::SetAnimationMode(AnimationMode mode)
{
    if (mode != AnimationMode::FreeMode && view_trajectory_.view_status_.empty()) {
        return;
    }
    animation_mode_ = mode;
    switch (mode) {
    case AnimationMode::PreviewMode:
    case AnimationMode::PlayMode:
        view_trajectory_.ComputeInterpolationCoefficients();
        GoToFirst();
        break;
    case AnimationMode::FreeMode:
    default:
        break;
    }
}

void UbitrackViewControl::AddKeyFrame()
{
    if (animation_mode_ == AnimationMode::FreeMode) {
        ViewParameters current_status;
        ConvertToViewParameters(current_status);
        if (view_trajectory_.view_status_.empty()) {
            view_trajectory_.view_status_.push_back(current_status);
            current_keyframe_ = 0.0;
        } else {
            size_t current_index = CurrentKeyframe();
            view_trajectory_.view_status_.insert(
                    view_trajectory_.view_status_.begin() + current_index + 1,
                    current_status);
            current_keyframe_ = current_index + 1.0;
        }
    }
}

void UbitrackViewControl::UpdateKeyFrame()
{
    if (animation_mode_ == AnimationMode::FreeMode &&
            !view_trajectory_.view_status_.empty()) {
        ConvertToViewParameters(
                view_trajectory_.view_status_[CurrentKeyframe()]);
    }
}

void UbitrackViewControl::DeleteKeyFrame()
{
    if (animation_mode_ == AnimationMode::FreeMode &&
            !view_trajectory_.view_status_.empty()) {
        size_t current_index = CurrentKeyframe();
        view_trajectory_.view_status_.erase(
                view_trajectory_.view_status_.begin() + current_index);
        current_keyframe_ = RegularizeFrameIndex(current_index - 1.0,
                view_trajectory_.view_status_.size(),
                view_trajectory_.is_loop_);
    }
    SetViewControlFromTrajectory();
}

void UbitrackViewControl::AddSpinKeyFrames(int num_of_key_frames
        /* = 20*/)
{
    if (animation_mode_ == AnimationMode::FreeMode) {
        double radian_per_step = M_PI * 2.0 / double(num_of_key_frames);
        for (int i = 0; i < num_of_key_frames; i++) {
            ViewControl::Rotate(radian_per_step / ROTATION_RADIAN_PER_PIXEL, 0);
            AddKeyFrame();
        }
    }
}

std::string UbitrackViewControl::GetStatusString() const
{
    std::string prefix;
    switch (animation_mode_) {
    case AnimationMode::FreeMode:
        prefix = "Editing ";
        break;
    case AnimationMode::PreviewMode:
        prefix = "Previewing ";
        break;
    case AnimationMode::PlayMode:
        prefix = "Playing ";
        break;
    }
    char buffer[DEFAULT_IO_BUFFER_SIZE];
    if (animation_mode_ == AnimationMode::FreeMode) {
        if (view_trajectory_.view_status_.empty()) {
            sprintf(buffer, "empty trajectory");
        } else {
            sprintf(buffer, "#%u keyframe (%u in total%s)",
                    (unsigned int)CurrentKeyframe() + 1,
                    (unsigned int)view_trajectory_.view_status_.size(),
                    view_trajectory_.is_loop_ ? ", looped" : "");
        }
    } else {
        if (view_trajectory_.view_status_.empty()) {
            sprintf(buffer, "empty trajectory");
        } else {
            sprintf(buffer, "#%u frame (%u in total%s)",
                    (unsigned int)CurrentFrame() + 1,
                    (unsigned int)view_trajectory_.NumOfFrames(),
                    view_trajectory_.is_loop_ ? ", looped" : "");
        }
    }
    return prefix + std::string(buffer);
}

void UbitrackViewControl::Step(double change)
{
    if (view_trajectory_.view_status_.empty()) {
        return;
    }
    if (animation_mode_ == AnimationMode::FreeMode) {
        current_keyframe_ += change;
        current_keyframe_ = RegularizeFrameIndex(current_keyframe_,
                view_trajectory_.view_status_.size(),
                view_trajectory_.is_loop_);
    } else {
        current_frame_ += change;
        current_frame_ = RegularizeFrameIndex(current_frame_,
                view_trajectory_.NumOfFrames(), view_trajectory_.is_loop_);
    }
    SetViewControlFromTrajectory();
}

void UbitrackViewControl::GoToFirst()
{
    if (view_trajectory_.view_status_.empty()) {
        return;
    }
    if (animation_mode_ == AnimationMode::FreeMode) {
        current_keyframe_ = 0.0;
    } else {
        current_frame_ = 0.0;
    }
    SetViewControlFromTrajectory();
}

void UbitrackViewControl::GoToLast()
{
    if (view_trajectory_.view_status_.empty()) {
        return;
    }
    if (animation_mode_ == AnimationMode::FreeMode) {
        current_keyframe_ = view_trajectory_.view_status_.size() - 1.0;
    } else {
        current_frame_ = view_trajectory_.NumOfFrames() - 1.0;
    }
    SetViewControlFromTrajectory();
}

bool UbitrackViewControl::CaptureTrajectory(
        const std::string &filename/* = ""*/)
{
    if (view_trajectory_.view_status_.empty()) {
        return false;
    }
    std::string json_filename = filename;
    if (json_filename.empty()) {
        json_filename = "ViewTrajectory_" + GetCurrentTimeStamp() + ".json";
    }
    PrintDebug("[Visualizer] Trejactory capture to %s\n", json_filename.c_str());
    return WriteIJsonConvertible(json_filename, view_trajectory_);
}

bool UbitrackViewControl::LoadTrajectoryFromJsonFile(
        const std::string &filename)
{
    bool success = ReadIJsonConvertible(filename, view_trajectory_);
    if (success == false) {
        view_trajectory_.Reset();
    }
    current_keyframe_ = 0.0;
    current_frame_ = 0.0;
    SetViewControlFromTrajectory();
    return success;
}

bool UbitrackViewControl::LoadTrajectoryFromCameraTrajectory(
        const PinholeCameraTrajectory &camera_trajectory)
{
    current_keyframe_ = 0.0;
    current_frame_ = 0.0;
    view_trajectory_.Reset();
    if (camera_trajectory.extrinsic_.empty()) {
        return false;
    }
    view_trajectory_.interval_ = ViewTrajectory::INTERVAL_MIN;
    view_trajectory_.is_loop_ = false;
    view_trajectory_.view_status_.resize(camera_trajectory.extrinsic_.size());
    for (size_t i = 0; i < camera_trajectory.extrinsic_.size(); i++) {
        UbitrackViewControl view_control = *this;
        if (view_control.ConvertFromPinholeCameraParameters(
                camera_trajectory.intrinsic_,
                camera_trajectory.extrinsic_[i]) == false) {
            view_trajectory_.Reset();
            return false;
        }
        if (view_control.ConvertToViewParameters(
                view_trajectory_.view_status_[i]) == false) {
            view_trajectory_.Reset();
            return false;
        }
    }
    SetViewControlFromTrajectory();
    return true;
}

bool UbitrackViewControl::IsValidPinholeCameraTrajectory() const
{
    if (view_trajectory_.view_status_.empty()) {
        return false;
    }
    if (view_trajectory_.view_status_[0].field_of_view_ == FIELD_OF_VIEW_MIN) {
        return false;
    }
    for (const auto &status : view_trajectory_.view_status_) {
        if (status.field_of_view_ !=
                view_trajectory_.view_status_[0].field_of_view_) {
            return false;
        }
    }
    return true;
}

double UbitrackViewControl::RegularizeFrameIndex(
        double current_frame, size_t num_of_frames, bool is_loop)
{
    if (num_of_frames == 0) {
        return 0.0;
    }
    double frame_index = current_frame;
    if (is_loop) {
        while (int(round(frame_index)) < 0) {
            frame_index += double(num_of_frames);
        }
        while (int(round(frame_index)) >= int(num_of_frames)) {
            frame_index -= double(num_of_frames);
        }
    } else {
        if (frame_index < 0.0) {
            frame_index = 0.0;
        }
        if (frame_index > num_of_frames - 1.0) {
            frame_index = num_of_frames - 1.0;
        }
    }
    return frame_index;
}

void UbitrackViewControl::SetViewControlFromTrajectory()
{
    if (view_trajectory_.view_status_.empty()) {
        return;
    }
    if (animation_mode_ == AnimationMode::FreeMode) {
        ConvertFromViewParameters(
                view_trajectory_.view_status_[CurrentKeyframe()]);
    } else {
        bool success;
        ViewParameters status;
        std::tie(success, status) =
                view_trajectory_.GetInterpolatedFrame(CurrentFrame());
        if (success) {
            ConvertFromViewParameters(status);
        }
    }
}

}	// namespace three