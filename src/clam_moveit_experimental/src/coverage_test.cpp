/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, CU Boulder
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the CU Boulder nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Dave Coleman
   Desc:   This node goes through a series of x and y locations on the workspace table
   and plans to see if robot can reach each location
*/

// MoveIt
//#include <moveit/move_group_interface/move_group.h>
#include <moveit_msgs/MoveGroupAction.h>
#include <moveit/kinematic_constraints/utils.h>
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>
#include <moveit/robot_state/robot_state.h>
#include <moveit/planning_models_loader/kinematic_model_loader.h>
#include <moveit/robot_state/conversions.h>
// ROS
#include <ros/ros.h>
#include <actionlib/client/simple_action_client.h>
// ClamArm
#include <clam_msgs/ClamArmAction.h> // for controlling the gripper
// Rviz
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
// For recording data
#include <iostream>
#include <fstream>


namespace coverage_test
{

// Constants
static const std::string GROUP_NAME = "arm";
static const std::string DATA_FILE_OUTPUT = "/home/dave/ros/clam/src/clam_moveit_experimental/data/coverage_test.dat";
static const std::string ROBOT_DESCRIPTION="robot_description";

// *********************************************************************************************************
// *********************************************************************************************************
// Coverage testing class
// *********************************************************************************************************
// *********************************************************************************************************

class CoverageTest
{
private:

  // A ROS publisher
  ros::Publisher marker_pub_;

  // A shared node handle
  ros::NodeHandle nh_;

public:
  // *********************************************************************************************************
  // Constructor
  // *********************************************************************************************************
  CoverageTest()
  {
    // -----------------------------------------------------------------------------------------------
    // Planning Scene stuff
    planning_scene_monitor::PlanningSceneMonitor psm(ROBOT_DESCRIPTION);
    planning_scene::PlanningScene &scene = *psm.getPlanningScene();

    // -----------------------------------------------------------------------------------------------
    // Rviz Visualizations
    marker_pub_ = nh_.advertise<visualization_msgs::Marker>("visualization_marker", 1);
    ros::Duration(0.5).sleep();

    ROS_INFO_STREAM("[coverage test] Preparing to send command to group = " << GROUP_NAME);

    // -----------------------------------------------------------------------------------------------
    // Connect to ClamArm action server
    actionlib::SimpleActionClient<clam_msgs::ClamArmAction> clam_arm_client_("clam_arm", true);
    clam_msgs::ClamArmGoal clam_arm_goal_; // sent to the clam_arm_client_server

    while(!clam_arm_client_.waitForServer(ros::Duration(5.0))){ // wait for server to start
      ROS_INFO("[coverage test] Waiting for the clam_arm action server");
    }

    // -----------------------------------------------------------------------------------------------
    // Connect to move_group movegroup_actionion server
    actionlib::SimpleActionClient<moveit_msgs::MoveGroupAction> movegroup_action("move_group", false);
    while(!movegroup_action.waitForServer(ros::Duration(5.0))){ // wait for server to start
      ROS_INFO("[coverage test] Waiting for the move_group action server");
    }
    //    ROS_INFO("[coverage test] Connecting to move_group action server");
    //    movegroup_action.waitForServer();

    // -----------------------------------------------------------------------------------------------
    // Go to home position
    ROS_INFO("[coverage test] Resetting arm to home position");
    clam_arm_goal_.command = clam_msgs::ClamArmGoal::RESET;
    clam_arm_client_.sendGoal(clam_arm_goal_);
    clam_arm_client_.waitForResult(ros::Duration(20.0));
    if(!clam_arm_client_.getState().isDone())
    {
      ROS_ERROR("[coverage test] Timeout: Unable to move to home position");
    }

    // -----------------------------------------------------------------------------------------------
    // Open gripper
    ROS_INFO("[coverage test] Opening gripper");
    clam_arm_goal_.command = clam_msgs::ClamArmGoal::END_EFFECTOR_OPEN;
    clam_arm_client_.sendGoal(clam_arm_goal_);
    clam_arm_client_.waitForResult(ros::Duration(10.0)); // has a timeout

    // Error check
    if( !clam_arm_client_.getState().isDone() ||
        !clam_arm_client_.getResult()->success )
    {
      ROS_ERROR("[coverage test] Timeout: Unable to open end effector");
    }

    // -----------------------------------------------------------------------------------------------
    // Move arm
    //    move_group_interface::MoveGroup group(GROUP_NAME);
    moveit_msgs::MoveGroupGoal goal;
    goal.request.group_name = GROUP_NAME;
    goal.request.num_planning_attempts = 1;
    goal.request.allowed_planning_time = 5.0; // fix for moveit? ros::Duration(5.0);


    // Position
    double x = 0.0;
    double y = 0.0;
    double z = 0.03;

    // Orientation
    double qx = 0.00;
    double qy = 0.710502;
    double qz = -0.01755;
    double qw = 0.70346;

    // Save results to file
    std::ofstream data_file;
    data_file.open(DATA_FILE_OUTPUT.c_str());

    //group.setStartStateToCurrentState();
    //group.setEndEffectorLink("gripper_fake_tip_link");
    //  group.setEndEffectorLink("camera_calibration_link");
    //group.setEndEffectorLink("l_gripper_aft_link");

    bool gripperOpen = true;
    double x_offset = 0.15;

    //ROS_INFO_STREAM( group.getEndEffectorLink() << " name " << group.getEndEffector() << " pose " << group.getPoseReferenceFrame() );

    // -----------------------------------------------------------------------------------------------
    // Loop through x and y range
    for( x = 0.1; x < 0.5; x += 0.05 )
    {
      for( y = 0.2; y > -0.2; y -= 0.05 )
      {
        // -------------------------------------------------------------------------------------------
        // Create goal state
        geometry_msgs::PoseStamped pose;
        pose.header.frame_id = "base_link";
        pose.pose.position.x = x;
        pose.pose.position.y = y;
        pose.pose.position.z = z;
        pose.pose.orientation.x = qx;
        pose.pose.orientation.y = qy;
        pose.pose.orientation.z = qz;
        pose.pose.orientation.w = qw;
        double tolerance_pose = 1e-3; // default: 1e-3... meters
        double tolerance_angle = 0.1; // default 1e-2... radians
        moveit_msgs::Constraints g0 =
          kinematic_constraints::constructGoalConstraints("gripper_roll_link", pose,
                                                          tolerance_pose, tolerance_angle);


        g0.position_constraints[0].target_point_offset.x = x_offset;
        g0.position_constraints[0].target_point_offset.y = 0.0;
        g0.position_constraints[0].target_point_offset.z = 0.0;

        goal.request.goal_constraints.resize(1);
        goal.request.goal_constraints[0] = g0;

        // -------------------------------------------------------------------------------------------
        // Create start state
        robot_state::RobotState start = scene.getCurrentState();


        //group.setStartState( start_state );
        //  group.setPositionTarget(0.22222, 0, 0.2);
        ROS_INFO_STREAM("[coverage test] Planning for x:" << x << " y:" << y << " z:" << z);

        /*
          group.setPositionTarget(x, y, z);
          group.setOrientationTarget( qx, qy, qz, qw );

          ROS_INFO_STREAM("[coverage test] Planning for x:" << x << " y:" << y << " z:" << z);
        */
        //ROS_INFO_STREAM("End effector set to " << group.getEndEffectorLink());
        //ROS_INFO_STREAM("Joint 0 has value " << group.getCurrentJointValues()[0]);

        publishSphere(x, y, z);
        publishMesh(x, y, z + x_offset, qx, qy, qz, qw );


        // -------------------------------------------------------------------------------------------
        // Plan
        //move_group_interface::MoveGroup::Plan plan;
        movegroup_action.sendGoal(goal);
        sleep(5);

        if(!movegroup_action.waitForResult(ros::Duration(5.0)))
        {
          ROS_INFO_STREAM("Apparently returned early");
        }
        if (movegroup_action.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
        {
          ROS_INFO("It worked!");


          /*
          // -----------------------------------------------------------------------------------------------
          // Close gripper
          ROS_INFO("[coverage test] Closing gripper");
          clam_arm_goal_.command = clam_msgs::ClamArmGoal::END_EFFECTOR_CLOSE;
          clam_arm_client_.sendGoal(clam_arm_goal_);
          clam_arm_client_.waitForResult(ros::Duration(10.0)); // has a timeout

          // Error check
          if( !clam_arm_client_.getState().isDone() ||
          !clam_arm_client_.getResult()->success )
          {
          ROS_ERROR("[coverage test] Timeout: Unable to close end effector");
          }
          */
        }
        else
        {
          ROS_WARN_STREAM("Fail: " << movegroup_action.getState().toString() << ": " << movegroup_action.getState().getText());
        }
        //        std::cout << *movegroup_action.getResult() << std::endl;

        /*
          if( group.plan(plan) )
          {
          // -----------------------------------------------------------------------------------------
          // Save to file
          data_file << x << "," << y <<  "," << z << "\n";

          // -----------------------------------------------------------------------------------------
          // Execute plan
          ROS_INFO("[coverage test] Executing...");
          group.execute(plan);

          ros::Duration(1.0).sleep();

          // -----------------------------------------------------------------------------------------------
          // Go to home position
          ROS_WARN("[coverage test] Resetting arm to home position");
          clam_arm_goal_.command = clam_msgs::ClamArmGoal::RESET;
          clam_arm_client_.sendGoal(clam_arm_goal_);
          clam_arm_client_.waitForResult(ros::Duration(20.0));
          if(!clam_arm_client_.getState().isDone())
          {
          ROS_ERROR("[gripper test] Timeout: Unable to move to home position");
          }

          }
          else
          {
          ROS_WARN("[coverage test] Failed to find a plan");
          }
        */
      }
    }

    // -----------------------------------------------------------------------------------------------
    // Close down
    data_file.close();


    ROS_INFO("[coverage test] Node exiting");
  }

  // *********************************************************************************************************
  // Deconstructor
  // *********************************************************************************************************
  ~CoverageTest()
  {

  }

  // *********************************************************************************************************
  // Helper Function
  // *********************************************************************************************************
  void publishMesh(double x, double y, double z, double qx, double qy, double qz, double qw )
  {
    visualization_msgs::Marker marker;
    // Set the frame ID and timestamp.  See the TF tutorials for information on these.
    marker.header.frame_id = "/base_link";
    marker.header.stamp = ros::Time::now();

    // Set the namespace and id for this marker.  This serves to create a unique ID
    marker.ns = "Mesh";

    // Set the marker type.
    marker.type = visualization_msgs::Marker::MESH_RESOURCE;
    marker.mesh_resource = "package://clam_description/stl/gripper_base_link.STL";

    // Set the marker action.  Options are ADD and DELETE
    marker.action = visualization_msgs::Marker::ADD;
    marker.id = 0;

    marker.pose.position.x = x;
    marker.pose.position.y = y;
    marker.pose.position.z = z;

    marker.pose.orientation.x = qx;
    marker.pose.orientation.y = qy;
    marker.pose.orientation.z = qz;
    marker.pose.orientation.w = qw;

    marker.scale.x = 0.001;
    marker.scale.y = 0.001;
    marker.scale.z = 0.001;

    marker.color.r = 1.0;
    marker.color.g = 0.0;
    marker.color.b = 0.0;
    marker.color.a = 1.0;

    // Make line color
    std_msgs::ColorRGBA color;
    color.r = 0.8;
    color.g = 0.1;
    color.b = 0.1;
    color.a = 1.0;


    // Point
    geometry_msgs::Point point_a;
    point_a.x = x;
    point_a.y = y;
    point_a.z = z;
    //ROS_INFO_STREAM("Publishing marker \n" << point_a );

    // Add the point pair to the line message
    marker.points.push_back( point_a );
    marker.colors.push_back( color );


    marker_pub_.publish( marker );
  }

  void publishSphere(double x, double y, double z)
  {
    visualization_msgs::Marker marker;
    // Set the frame ID and timestamp.  See the TF tutorials for information on these.
    marker.header.frame_id = "/base_link";
    marker.header.stamp = ros::Time::now();

    // Set the namespace and id for this marker.  This serves to create a unique ID
    marker.ns = "Sphere";

    // Set the marker type.
    marker.type = visualization_msgs::Marker::SPHERE_LIST;

    // Set the marker action.  Options are ADD and DELETE
    marker.action = visualization_msgs::Marker::ADD;
    marker.id = 0;

    marker.pose.position.x = 0;
    marker.pose.position.y = 0;
    marker.pose.position.z = 0;

    marker.pose.orientation.x = 0.0;
    marker.pose.orientation.y = 0.0;
    marker.pose.orientation.z = 0.0;
    marker.pose.orientation.w = 1.0;

    marker.scale.x = 0.01;
    marker.scale.y = 0.01;
    marker.scale.z = 0.01;

    marker.color.r = 0.0;
    marker.color.g = 0.0;
    marker.color.b = 1.0;
    marker.color.a = 1.0;

    // Make line color
    std_msgs::ColorRGBA color;
    color.r = 0.1;
    color.g = 0.1;
    color.b = 0.8;
    color.a = 1.0;


    // Point
    geometry_msgs::Point point_a;
    point_a.x = x;
    point_a.y = y;
    point_a.z = z;
    //ROS_INFO_STREAM("Publishing marker \n" << point_a );

    // Add the point pair to the line message
    marker.points.push_back( point_a );
    marker.colors.push_back( color );


    marker_pub_.publish( marker );
  }

}; // end of class

} // namespace

// Simple test program
int main(int argc, char **argv)
{
  // -----------------------------------------------------------------------------------------------
  // Initialize node
  ros::init(argc, argv, "clam_coverage_test", ros::init_options::AnonymousName);
  ros::AsyncSpinner spinner(1);
  spinner.start();

  coverage_test::CoverageTest test;

  return 0;
}
