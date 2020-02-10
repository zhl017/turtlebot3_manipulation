﻿/*******************************************************************************
* Copyright 2020 ROBOTIS CO., LTD.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

/* Authors: Ryan Shim */

/*****************************************************************************
** Includes
*****************************************************************************/

#include <ros/ros.h>
#include <ros/network.h>
#include <string>
#include <std_msgs/String.h>
#include <sstream>
#include "../include/turtlebot3_manipulation_gui/qnode.hpp"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace turtlebot3_manipulation_gui {

/*****************************************************************************
** Implementation
*****************************************************************************/

QNode::QNode(int argc, char** argv) 
    :init_argc(argc),
     init_argv(argv)
{}

QNode::~QNode()
{
  if(ros::isStarted()) 
  {
    ros::shutdown(); // explicitly needed since we use ros::start();
    ros::waitForShutdown();
  }
	wait();
}

bool QNode::init()
{
	ros::init(init_argc,init_argv,"turtlebot3_manipulation_gui");
	if ( ! ros::master::check() ) 
  {
		return false;
	}
	ros::start(); // explicitly needed since our nodehandle is going out of scope.
  ros::NodeHandle n("");

  // Moveit 
  ros::AsyncSpinner spinner(1); 
  spinner.start();

  // Move group arm
  std::string planning_group_name = "arm";
  move_group_ = new moveit::planning_interface::MoveGroupInterface(planning_group_name);

  // Move group gripper
  std::string planning_group_name2 = "gripper";
  move_group2_ = new moveit::planning_interface::MoveGroupInterface(planning_group_name2);

  // msg subscriber
  open_manipulator_joint_states_sub_ = n.subscribe("joint_states", 10, &QNode::jointStatesCallback, this);

  start();
	return true;
}

void QNode::run()
{
  ros::Rate loop_rate(10);
	while ( ros::ok() )
  {
		ros::spinOnce();
		loop_rate.sleep();
	}
	std::cout << "Ros shutdown, proceeding to close the gui." << std::endl;
	Q_EMIT rosShutdown();
}

void QNode::jointStatesCallback(const sensor_msgs::JointState::ConstPtr &msg)
{
  std::vector<double> temp_angle;
  temp_angle.resize(NUM_OF_JOINT_AND_TOOL);

  for(int i = 0; i < msg->name.size(); i ++)
  {
    if(!msg->name.at(i).compare("joint1"))  temp_angle.at(0) = (msg->position.at(i));
    else if(!msg->name.at(i).compare("joint2"))  temp_angle.at(1) = (msg->position.at(i));
    else if(!msg->name.at(i).compare("joint3"))  temp_angle.at(2) = (msg->position.at(i));
    else if(!msg->name.at(i).compare("joint4"))  temp_angle.at(3) = (msg->position.at(i));
    else if(!msg->name.at(i).compare("gripper"))  temp_angle.at(4) = (msg->position.at(i));
  }
  present_joint_angle_ = temp_angle;

  ros::AsyncSpinner spinner(1); 
  spinner.start();

  geometry_msgs::Pose current_pose = move_group_->getCurrentPose().pose;  
  std::vector<double> temp_position;
  temp_position.push_back(current_pose.position.x);
  temp_position.push_back(current_pose.position.y);
  temp_position.push_back(current_pose.position.z);
  present_kinematics_position_ = temp_position;
}

std::vector<double> QNode::getPresentJointAngle()
{
  return present_joint_angle_;
}

std::vector<double> QNode::getPresentKinematicsPosition()
{
  return present_kinematics_position_;
}

bool QNode::setJointSpacePath(std::vector<double> joint_angle, double path_time)
{
  ros::AsyncSpinner spinner(1); 
  spinner.start();

  // Next get the current set of joint values for the group.
  const robot_state::JointModelGroup* joint_model_group =
      move_group_->getCurrentState()->getJointModelGroup("arm");
      
  moveit::core::RobotStatePtr current_state = move_group_->getCurrentState();

  std::vector<double> joint_group_positions;
  current_state->copyJointGroupPositions(joint_model_group, joint_group_positions);

  // Now, let's modify one of the joints, plan to the new joint space goal and visualize the plan.
  joint_group_positions[0] = joint_angle.at(0);  // radians
  joint_group_positions[1] = joint_angle.at(1);  // radians
  joint_group_positions[2] = joint_angle.at(2);  // radians
  joint_group_positions[3] = joint_angle.at(3);  // radians
  move_group_->setJointValueTarget(joint_group_positions);

  moveit::planning_interface::MoveGroupInterface::Plan my_plan;
  bool success = (move_group_->plan(my_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);
  if (success == false)
    return false;

  move_group_->move();

  spinner.stop();
  return true;
}

bool QNode::setTaskSpacePath(std::vector<double> kinematics_pose, double path_time)
{
  ros::AsyncSpinner spinner(1); 
  spinner.start();

  move_group_->setGoalTolerance(0.1);
  // move_group_->setMaxVelocityScalingFactor(0.1);
  // move_group_->setMaxAccelerationScalingFactor(0.1);

  geometry_msgs::Pose target_pose;
  target_pose.position.x = kinematics_pose.at(0);
  target_pose.position.y = kinematics_pose.at(1);
  target_pose.position.z = kinematics_pose.at(2);
  move_group_->setPoseTarget(target_pose);

  moveit::planning_interface::MoveGroupInterface::Plan my_plan;
  bool success = (move_group_->plan(my_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);
  if (success == false)
    return false;

  move_group_->move();

  spinner.stop();
  return true;
}

bool QNode::setToolControl(std::vector<double> joint_angle)
{
  ros::AsyncSpinner spinner(1); 
  spinner.start();

  // Next get the current set of joint values for the group.
  const robot_state::JointModelGroup* joint_model_group =
      move_group2_->getCurrentState()->getJointModelGroup("gripper");
      
  moveit::core::RobotStatePtr current_state = move_group2_->getCurrentState();

  std::vector<double> joint_group_positions;
  current_state->copyJointGroupPositions(joint_model_group, joint_group_positions);

  // Now, let's modify one of the joints, plan to the new joint space goal and visualize the plan.
  joint_group_positions[0] = joint_angle.at(0);  // radians
  move_group2_->setJointValueTarget(joint_group_positions);

  moveit::planning_interface::MoveGroupInterface::Plan my_plan;
  bool success = (move_group2_->plan(my_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);
  if (success == false)
    return false;

  move_group2_->move();

  spinner.stop();
  return true;  
}
}  // namespace turtlebot3_manipulation_gui
