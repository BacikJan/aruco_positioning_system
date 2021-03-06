/*********************************************************************************************//**
* @file main.cpp
*
* ArUco Positioning System main file
*
* Copyright (c)
* Jan Bacik
* Smart Robotic Systems
* www.smartroboticsys.eu
* March 2015
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

// Standarc C++ libraries
#include    <iostream>
// Standard ROS libraries
#include    <ros/ros.h>
#include    <image_transport/image_transport.h>
#include    <std_msgs/Empty.h>
// My libraries
#include    <estimator.h>

////////////////////////////////////////////////////////////////////////////////

int
main(int argc, char **argv)
{
    // ROS initialization
    ros::init(argc,argv,"ArUco_positioning_system");
    // New node
    ros::NodeHandle myNode;
    std::cout << "ArUco_positioning_system is running..." << std::endl;

    // Parameter - marker size [m]
    double p_MarkerSize=0.1;
    myNode.getParam("MarkerSize",p_MarkerSize);

    // New Object ViewPoint_Estimator
    ViewPoint_Estimator myEstimator(&myNode,(float)p_MarkerSize);

    // Image node and subscriber
    image_transport::ImageTransport it(myNode);
    image_transport::Subscriber videoSub = it.subscribe("/image_raw", 1, &ViewPoint_Estimator::image_callback, &myEstimator);

    // Start message for ArUco
    ros::Subscriber startAruco;
    startAruco=myNode.subscribe("arucoPositioningSystem/startArUco",1,&ViewPoint_Estimator::wait_for_start,&myEstimator);

    ros::spin();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
