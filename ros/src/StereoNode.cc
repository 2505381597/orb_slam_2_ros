/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/

#include "StereoNode.hpp"

#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  auto options = rclcpp::NodeOptions();
  auto node = std::make_shared<StereoNode>("Stereo", options);

  node->init();

  if (argc > 1) {
    RCLCPP_WARN(node->get_logger(), "Arguments supplied via command line are neglected.");
  }

  rclcpp::spin(node->get_node_base_interface());

  rclcpp::shutdown();

  return 0;
}

StereoNode::StereoNode(
  const std::string & node_name,
  const rclcpp::NodeOptions & node_options)
: Node(node_name, node_options)
{
  declare_parameter("left_image_topic",
    rclcpp::ParameterValue(std::string("/camera_left/image_raw")));
  declare_parameter("right_image_topic",
    rclcpp::ParameterValue(std::string("/camera_right/image_raw")));
  declare_parameter("camera_info_topic",
    rclcpp::ParameterValue(std::string("/camera/fisheye1/camera_info")));
}

void StereoNode::init()
{
  get_parameter("camera_info_topic", camera_info_topic_);
  Node::init(ORB_SLAM2::System::STEREO);

  get_parameter("left_image_topic", left_image_topic_);
  get_parameter("right_image_topic", right_image_topic_);
  

  left_sub_ = std::make_shared<message_filters::Subscriber<sensor_msgs::msg::Image>>(
    shared_from_this(), left_image_topic_);
  right_sub_ = std::make_shared<message_filters::Subscriber<sensor_msgs::msg::Image>>(
    shared_from_this(), right_image_topic_);

  sync_ = new message_filters::Synchronizer<sync_pol>(sync_pol(10), *left_sub_, *right_sub_);
  sync_->registerCallback(std::bind(&StereoNode::ImageCallback, this,
    std::placeholders::_1, std::placeholders::_2));
}

StereoNode::~StereoNode()
{
  delete sync_;
}

void StereoNode::ImageCallback(
  const sensor_msgs::msg::Image::ConstSharedPtr & msgLeft,
  const sensor_msgs::msg::Image::ConstSharedPtr & msgRight)
{
  cv_bridge::CvImageConstPtr cv_ptrLeft;
  try {
    cv_ptrLeft = cv_bridge::toCvShare(msgLeft);
  } catch (cv_bridge::Exception & e) {
    RCLCPP_ERROR(get_logger(), "cv_bridge exception: %s", e.what());
    return;
  }

  cv_bridge::CvImageConstPtr cv_ptrRight;
  try {
    cv_ptrRight = cv_bridge::toCvShare(msgRight);
  } catch (cv_bridge::Exception & e) {
    RCLCPP_ERROR(get_logger(), "cv_bridge exception: %s", e.what());
    return;
  }

  current_frame_time_ = msgLeft->header.stamp;

  rclcpp::Time msg_time = cv_ptrLeft->header.stamp;
  orb_slam_->TrackStereo(cv_ptrLeft->image, cv_ptrRight->image, msg_time.seconds());

  Update();
}
