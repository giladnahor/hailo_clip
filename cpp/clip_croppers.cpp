/**
 * Copyright (c) 2021-2022 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the LGPL license (https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt)
 **/

// This file is a modified version of tappas/core/hailo/libs/croppers/vms/vms_croppers.cpp

#include <vector>
#include <cmath>
#include "clip_croppers.hpp"

#define PERSON_LABEL "person"
#define FACE_LABEL "face"
#define OBJECT_LABEL "object"

/**
* @brief Get the tracking Hailo Unique Id object from a Hailo Detection.
* 
* @param detection HailoDetectionPtr
* @return HailoUniqueIdPtr pointer to the Hailo Unique Id object
*/
HailoUniqueIDPtr get_tracking_id(HailoDetectionPtr detection)
{
    for (auto obj : detection->get_objects_typed(HAILO_UNIQUE_ID))
    {
        HailoUniqueIDPtr id = std::dynamic_pointer_cast<HailoUniqueID>(obj);
        if (id->get_mode() == TRACKING_ID)
        {
            return id;
        }
    }
    return nullptr;
}
std::map<int, int> track_counter;

/**
* @brief Returns a boolean indicating if traker update is required for a given detection.
*       It is determined by the number of frames since the last update.
*       How many frames to wait for an update are defined in TRACK_UPDATE.
* 
* @param detection HailoDetectionPtr
* @param use_track_update boolean can override the default behaviour, false will always require an update
* @return boolean indicating if traker update is required.
*/
bool track_update(HailoDetectionPtr detection, bool use_track_update, int TRACK_UPDATE=15)
{
    auto tracking_obj = get_tracking_id(detection);
    if (tracking_obj && use_track_update)
    {
        int tracking_id = tracking_obj->get_id();
        auto counter = track_counter.find(tracking_id);
        if (counter == track_counter.end())
        {
            // Emplace new element to the track_counter map. track update required.
            track_counter.emplace(tracking_id, 0);
            return true;
        }
        else if (counter->second >= TRACK_UPDATE)
        {
            // Counter passed the TRACK_UPDATE limit - set existing track to 0. track update required.
            track_counter[tracking_id] = 0;
            return true;
        }
        else if (counter->second < TRACK_UPDATE)
        {
            // Counter is still below TRACK_UPDATE_LIMIT - increasing the exising value. track update should be skipped. 
            track_counter[tracking_id] += 1;
        }

        return false;
    }

    return true;
}

/**
 * @brief Returns a vector of detections to crop and resize.
 *
 * @param image The original picture (cv::Mat).
 * @param roi The main ROI of this picture.
 * @param label The label to crop.
 * @param crop_every_x_frames Run crop every X frames per tracked object.
 * @param max_crops_per_frame Max number of objects to crop per frame.
 * @return std::vector<HailoROIPtr> vector of ROI's to crop and resize.
 */

std::vector<HailoROIPtr> object_crop(const std::shared_ptr<HailoMat>& image, const HailoROIPtr& roi, const std::string label=PERSON_LABEL, 
int crop_every_x_frames=30, int max_crops_per_frame=5)
{
    auto object_counter = 0;
    std::vector<HailoROIPtr> crop_rois;

    std::vector<HailoDetectionPtr> detections_ptrs = hailo_common::get_hailo_detections(roi);
    std::vector<HailoDetectionPtr> detections_to_crop;
    
    for (HailoDetectionPtr &detection : detections_ptrs)
    {
        auto detection_label = detection->get_label();
        if (label != detection->get_label())
        {
            // Not the label we are looking for.
            continue;
        }
        auto tracking_obj = get_tracking_id(detection);
        if (!tracking_obj)
        {
            // object is not tracked don't crop it.
            continue;
        }
        if (track_update(detection, true, crop_every_x_frames))
        {
            detections_to_crop.emplace_back(detection);
            object_counter += 1;
            if (object_counter >= max_crops_per_frame)
            {
                break;
            }

        }
    }
    
    for (HailoDetectionPtr &detection : detections_to_crop)
    {
        crop_rois.emplace_back(detection);
    }
    return crop_rois;
}

std::vector<HailoROIPtr> face_cropper(std::shared_ptr<HailoMat> image, HailoROIPtr roi)
{
    return object_crop(image, roi, FACE_LABEL, 15, 5);
}

std::vector<HailoROIPtr> person_cropper(std::shared_ptr<HailoMat> image, HailoROIPtr roi)
{
    return object_crop(image, roi, PERSON_LABEL, 15, 5);
}

std::vector<HailoROIPtr> object_cropper(std::shared_ptr<HailoMat> image, HailoROIPtr roi)
{
    return object_crop(image, roi, OBJECT_LABEL, 30, 10);
}
