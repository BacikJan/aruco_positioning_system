/*********************************************************************************************//**
* @file estimator.cpp
*
* ArUco Positioning System source file
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

#ifndef ESTIMATOR_CPP
#define ESTIMATOR_CPP
#endif

////////////////////////////////////////////////////////////////////////////////////////////////

#include <estimator.h>

////////////////////////////////////////////////////////////////////////////////////////////////

ViewPoint_Estimator::ViewPoint_Estimator(ros::NodeHandle *myNode, float paramMakerSize) :
    markerSize(paramMakerSize),                          // Marker size in m
    filename("empty"),                                   // Initial filenames
    myListener (new tf::TransformListener),              // Listener of TFs
    regionOfInterest (false),                            // switiching ROI
    numberOfAllMarkers (35),                             // Number of used markers
    StartNow (false),                                    // switching when start image processing
    StartNowFromParameter (false),                       // switching when start image processing
    type_of_space ("plane")                              // default space - plane
{
    // Path to calibration file of camera
    //--------------------------------------------------
    myNode->getParam("calibration_file",filename);
    std::cout << "Calibration file path: " << filename << std::endl;
    //--------------------------------------------------
    // Parameter - number of all markers
    myNode->getParam("markers_number",numberOfAllMarkers);
    //--------------------------------------------------
    // Parameter - region of interest
    myNode->getParam("region_of_interest",regionOfInterest);
    //--------------------------------------------------
    // Parameter - starting ArUco with message or from beggining of programm
    myNode->getParam("start_now",StartNowFromParameter);
    if(StartNowFromParameter==true)
        StartNow=true;
    //--------------------------------------------------
    // Parameter - type of space, plane or 3D space
    myNode->getParam("type_of_markers_space",type_of_space);
    //--------------------------------------------------
    // Parameter - region of interest - parameters
    //--------------------------------------------------
    ROIx=0;
    ROIy=0;
    ROIw=10;
    ROIh=5;
    myNode->getParam("region_of_interest_x",ROIx);
    myNode->getParam("region_of_interest_y",ROIy);
    myNode->getParam("region_of_interest_width",ROIw);
    myNode->getParam("region_of_interest_height",ROIh);
    //--------------------------------------------------

    // Publishers
    my_markers_pub=myNode->advertise<aruco_positioning_system::ArUcoMarkers>("ArUcoMarkersPose",1);
    marker_pub=myNode->advertise<visualization_msgs::Marker>("aruco_marker",1);
    //--------------------------------------------------

    // Loading calibration parameters
    //--------------------------------------------------
    load_calibration_file(filename);
    //--------------------------------------------------

    // Inicialization of marker
    //--------------------------------------------------
    cv::namedWindow("Mono8", CV_WINDOW_AUTOSIZE);
    //--------------------------------------------------


    // Inicialization of variables
    //--------------------------------------------------
    lookingForFirst=false;
    lowestIDMarker=-1;
    markersCounter=0;
    indexActualCamera=0;
    // Dynamics array of markers
    AllMarkers=new MarkerInfo[numberOfAllMarkers];

    // Inicialization of array - all informations about each marker
    for(int j=0;j<numberOfAllMarkers;j++)
    {
        AllMarkers[j].relatedMarkerID=-1;
        AllMarkers[j].active=false;
        AllMarkers[j].markerID=-1;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////

ViewPoint_Estimator::~ViewPoint_Estimator()
{
    delete intrinsics;
    delete distortion_coeff;
    delete image_size;
    delete myListener;
    delete [] AllMarkers;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void
ViewPoint_Estimator::image_callback(const sensor_msgs::ImageConstPtr &original_image)
{
    // ROS Image to Mat structure
    //--------------------------------------------------
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
        cv_ptr=cv_bridge::toCvCopy(original_image, sensor_msgs::image_encodings::MONO8);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("Error open image %s", e.what());
        return;
    }
    I=cv_ptr->image;
    //--------------------------------------------------

    // Region Of Interest
    if(regionOfInterest==true)
        I=cv_ptr->image(cv::Rect(ROIx,ROIy,ROIw,ROIh));

    // Marker detection
    //--------------------------------------------------
    if(StartNow==true)
        bool found=markers_find_pattern(I,I);
    //--------------------------------------------------

    // Show image
    cv::imshow("Mono8", I);
    cv::waitKey(10);
}


////////////////////////////////////////////////////////////////////////////////////////////////

bool
ViewPoint_Estimator::markers_find_pattern(cv::Mat input_image,cv::Mat output_image)
{
    aruco::MarkerDetector MDetector;
    std::vector<aruco::Marker> markers;

    // Initialization, all markers sign of visibility is set to false
    for(int j=0;j<numberOfAllMarkers;j++)
        AllMarkers[j].active=false;

    // Actual count of found markers before new image processing
    markerCounter_before=markersCounter;

    // Actual ID of marker, which is processed
    int MarrkerArrayID;

    // Markers Detector, if return marker.size() 0, it dint finf any marker in image
    MDetector.detect(input_image,markers,arucoCalibParams,markerSize);

    // Any marker wasnt find
    if(markers.size()==0)
        std::cout << "Any marker is not in the actual image!" << std::endl;

    //------------------------------------------------------
    // Initialization of First Marker - begining of the path
    //------------------------------------------------------
    // First marker is detected
    if((markers.size()>0)&&(lookingForFirst==false))
    {
        size_t low_ID;

        // Marker with lowest ID is my begining of the path
        lowestIDMarker=markers[0].id;
        low_ID=0;
        for(size_t i=0;i<markers.size();i++)
        {
            if(markers[i].id<lowestIDMarker)
            {
                lowestIDMarker=markers[i].id;
                low_ID=i;
            }
        }
        std::cout << "The lowest Id marker " << lowestIDMarker << std::endl;

        // Position of my beginning - origin [0,0,0]
        AllMarkers[0].markerID=lowestIDMarker;
        AllMarkers[0].AllMarkersPose.position.x=0;
        AllMarkers[0].AllMarkersPose.position.y=0;
        AllMarkers[0].AllMarkersPose.position.z=0;
        // And orientation
        AllMarkers[0].AllMarkersPose.orientation.x=0;
        AllMarkers[0].AllMarkersPose.orientation.y=0;
        AllMarkers[0].AllMarkersPose.orientation.z=0;
        AllMarkers[0].AllMarkersPose.orientation.w=1;

        // Relative position and Global position of Origin is same
        AllMarkers[0].AllMarkersPoseGlobe.position.x=0;
        AllMarkers[0].AllMarkersPoseGlobe.position.y=0;
        AllMarkers[0].AllMarkersPoseGlobe.position.z=0;
        AllMarkers[0].AllMarkersPoseGlobe.orientation.x=0;
        AllMarkers[0].AllMarkersPoseGlobe.orientation.y=0;
        AllMarkers[0].AllMarkersPoseGlobe.orientation.z=0;
        AllMarkers[0].AllMarkersPoseGlobe.orientation.w=1;

        // Transformation Pose to TF

        tf::Vector3 origin;
        origin.setX(0);
        origin.setY(0);
        origin.setZ(0);

        tf::Quaternion quat;
        quat.setX(0);
        quat.setY(0);
        quat.setZ(0);
        quat.setW(1);

        AllMarkers[0].AllMarkersTransform.setOrigin(origin);
        AllMarkers[0].AllMarkersTransform.setRotation(quat);
        // Relative position and Global position of first marker - Origin is same
        AllMarkers[0].AllMarkersTransformGlobe=AllMarkers[0].AllMarkersTransform;

        // Increasing count of actual markers
        markersCounter++;

        // Sign of visibility of first marker
        lookingForFirst=true;
        std::cout << "First marker [origin] was found" << std::endl;

        // Position of origin is relative to global position, no relative position to any marker
        AllMarkers[0].relatedMarkerID=-2;
        // Sign of visibilitys
        AllMarkers[0].active=true;
    }
    //------------------------------------------------------

    //------------------------------------------------------
    // Addings new markers and mapping
    //------------------------------------------------------
    // Markers are always sorted in ascending
    for(size_t i=0;i<markers.size();i++)
    {
        int currentMarkerID=markers[i].id;

        // Use only marker with modulo 10
        if(currentMarkerID%10==0)
        {
            //------------------------------------------------------
            //Draw marker convex, ID, cube and axis
            //------------------------------------------------------
            markers[i].draw(output_image, cv::Scalar(0,0,255),2);
            aruco::CvDrawingUtils::draw3dCube(output_image,markers[i],arucoCalibParams);
            aruco::CvDrawingUtils::draw3dAxis(output_image,markers[i],arucoCalibParams);

            //------------------------------------------------------
            // Check, if it see new marker or it has already known
            //------------------------------------------------------
            bool find=false;
            int m_j=0;
            while((find==false)&&(m_j<markersCounter))
            {
                if(AllMarkers[m_j].markerID==currentMarkerID)
                {
                    MarrkerArrayID=m_j;
                    find=true;
                    std::cout << "Existing ID was assigned" << std::endl;
                }
                m_j++;
            }
            if(find==false)
            {
                MarrkerArrayID=markersCounter;
                AllMarkers[MarrkerArrayID].markerID=currentMarkerID;
                find=true;
                std::cout << "New marker" << std::endl;
            }

            //------------------------------------------------------
            std::cout << "Actuak marker " << MarrkerArrayID << " and its ID " << AllMarkers[MarrkerArrayID].markerID << std::endl;
            //------------------------------------------------------

            //------------------------------------------------------
            // Sign of visivility of marker
            // If new marker is found, sign of visibility must be changed
            //------------------------------------------------------
            for(int j=0;j<markersCounter;j++)
            {
                for(size_t im=0;im<markers.size();im++)
                {
                    if(AllMarkers[j].markerID==markers[im].id)
                        AllMarkers[j].active=true;
                }
            }
            //------------------------------------------------------

            //------------------------------------------------------
            // Old marker was found in the image
            //------------------------------------------------------
            if((MarrkerArrayID<markersCounter)&&(lookingForFirst==true))
            {
                AllMarkers[MarrkerArrayID].CurrentCameraTf=arucoMarker2Tf(markers[i]);
                AllMarkers[MarrkerArrayID].CurrentCameraTf=AllMarkers[MarrkerArrayID].CurrentCameraTf.inverse();

                const tf::Vector3 marker_origin=AllMarkers[MarrkerArrayID].CurrentCameraTf.getOrigin();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.position.x=marker_origin.getX();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.position.y=marker_origin.getY();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.position.z=marker_origin.getZ();

                const tf::Quaternion marker_quaternion=AllMarkers[MarrkerArrayID].CurrentCameraTf.getRotation();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.x=marker_quaternion.getX();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.y=marker_quaternion.getY();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.z=marker_quaternion.getZ();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.w=marker_quaternion.getW();
            }

            //------------------------------------------------------
            // New marker was found
            // Global and relative position must be calculated
            //------------------------------------------------------
            if((MarrkerArrayID==markersCounter)&&(lookingForFirst==true))
            {
                AllMarkers[MarrkerArrayID].CurrentCameraTf=arucoMarker2Tf(markers[i]);

                tf::Vector3 marker_origin=AllMarkers[MarrkerArrayID].CurrentCameraTf.getOrigin();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.position.x=marker_origin.getX();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.position.y=marker_origin.getY();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.position.z=marker_origin.getZ();

                tf::Quaternion marker_quaternion=AllMarkers[MarrkerArrayID].CurrentCameraTf.getRotation();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.x=marker_quaternion.getX();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.y=marker_quaternion.getY();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.z=marker_quaternion.getZ();
                AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.w=marker_quaternion.getW();

                // Naming - TFs
                std::stringstream cameraTFID;
                cameraTFID << "camera_" << MarrkerArrayID;
                std::stringstream cameraTFID_old;
                std::stringstream markerTFID_old;

                // Sing if any known marker is visible
                bool anyMarker=false;
                // Array ID of markers, which position of new marker is calculated
                int lastMarlerID;

                // Testing, if is possible calculate position of a new marker to old known marker
                for(int k=0;k<MarrkerArrayID;k++)
                {
                    if((AllMarkers[k].active==true)&&(anyMarker==false))
                    {
                        if(AllMarkers[k].relatedMarkerID!=-1)
                        {
                            anyMarker=true;
                            cameraTFID_old << "camera_" << k;
                            markerTFID_old << "marker_" << k;
                            AllMarkers[MarrkerArrayID].relatedMarkerID=k;
                            lastMarlerID=k;
                        }
                    }
                }

                // New position can be calculated
                if(anyMarker==true)
                {
                    // Generating TFs for listener
                    for(char k=0;k<10;k++)
                    {
                        // TF from old marker and its camera
                        myBroadcaster.sendTransform(tf::StampedTransform(AllMarkers[lastMarlerID].CurrentCameraTf,ros::Time::now(),markerTFID_old.str(),cameraTFID_old.str()));
                          // TF from old camera to new camera
                        myBroadcaster.sendTransform(tf::StampedTransform(AllMarkers[MarrkerArrayID].CurrentCameraTf,ros::Time::now(),cameraTFID_old.str(),cameraTFID.str()));
                        usleep(100);
                    }

                    // Calculate TF between two markers
                    myListener->waitForTransform(markerTFID_old.str(),cameraTFID.str(),ros::Time(0),ros::Duration(2.0));
                    try
                    {
                        myBroadcaster.sendTransform(tf::StampedTransform(AllMarkers[lastMarlerID].CurrentCameraTf,ros::Time::now(),markerTFID_old.str(),cameraTFID_old.str()));
                        myBroadcaster.sendTransform(tf::StampedTransform(AllMarkers[MarrkerArrayID].CurrentCameraTf,ros::Time::now(),cameraTFID_old.str(),cameraTFID.str()));

                        myListener->lookupTransform(markerTFID_old.str(),cameraTFID.str(),ros::Time(0),AllMarkers[MarrkerArrayID].AllMarkersTransform);
                    }
                    catch(tf::TransformException &ex)
                    {
                        ROS_ERROR("Error - 1\n");
                        ros::Duration(2.0).sleep();
                    }

                    // Saving quaternion and origin of calculated TF to pose
                    marker_quaternion=AllMarkers[MarrkerArrayID].AllMarkersTransform.getRotation();
                    // If all markers are in the plane, for better accuracy, we known that roll and pitch are zero
                    if(type_of_space=="plane")
                    {
                        double roll,pitch,yaw;
                        tf::Matrix3x3(marker_quaternion).getRPY(roll,pitch,yaw);
                        roll=0;
                        pitch=0;
                        marker_quaternion.setRPY(pitch,roll,yaw);
                    }
                    AllMarkers[MarrkerArrayID].AllMarkersTransform.setRotation(marker_quaternion);

                    marker_origin=AllMarkers[MarrkerArrayID].AllMarkersTransform.getOrigin();
                    // If all markers are in the plane, for better accuracy, we known that Z is zero
                    if(type_of_space=="plane")
                        marker_origin.setZ(0);
                    AllMarkers[MarrkerArrayID].AllMarkersTransform.setOrigin(marker_origin);

                    marker_origin=AllMarkers[MarrkerArrayID].AllMarkersTransform.getOrigin();
                    AllMarkers[MarrkerArrayID].AllMarkersPose.position.x=marker_origin.getX();
                    AllMarkers[MarrkerArrayID].AllMarkersPose.position.y=marker_origin.getY();
                    AllMarkers[MarrkerArrayID].AllMarkersPose.position.z=marker_origin.getZ();

                    marker_quaternion=AllMarkers[MarrkerArrayID].AllMarkersTransform.getRotation();
                    AllMarkers[MarrkerArrayID].AllMarkersPose.orientation.x=marker_quaternion.getX();
                    AllMarkers[MarrkerArrayID].AllMarkersPose.orientation.y=marker_quaternion.getY();
                    AllMarkers[MarrkerArrayID].AllMarkersPose.orientation.z=marker_quaternion.getZ();
                    AllMarkers[MarrkerArrayID].AllMarkersPose.orientation.w=marker_quaternion.getW();

                    // increasing count of markers
                    markersCounter++;

                    //--------------------------------------
                    // Position of new marker have to be inversed, because i need for calculating of a next new marker position inverse TF of old markers
                    //--------------------------------------
                    AllMarkers[MarrkerArrayID].CurrentCameraTf=AllMarkers[MarrkerArrayID].CurrentCameraTf.inverse();
                    marker_origin=AllMarkers[MarrkerArrayID].CurrentCameraTf.getOrigin();
                    AllMarkers[MarrkerArrayID].CurrentCameraPose.position.x=marker_origin.getX();
                    AllMarkers[MarrkerArrayID].CurrentCameraPose.position.y=marker_origin.getY();
                    AllMarkers[MarrkerArrayID].CurrentCameraPose.position.z=marker_origin.getZ();
                    marker_quaternion=AllMarkers[MarrkerArrayID].CurrentCameraTf.getRotation();
                    AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.x=marker_quaternion.getX();
                    AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.y=marker_quaternion.getY();
                    AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.z=marker_quaternion.getZ();
                    AllMarkers[MarrkerArrayID].CurrentCameraPose.orientation.w=marker_quaternion.getW();

                    // Publishing of all TFs and markers
                    publish_tfs(false);
                }
            }
        }


        //------------------------------------------------------
        // New Marker was found, its global position is calculated
        //------------------------------------------------------
        if((markerCounter_before<markersCounter)&&(lookingForFirst==true))
        {
            // Publishing all TF for five times for listener
            for(char k=0;k<5;k++)
                publish_tfs(false);

            std::stringstream markerGlobe;
            markerGlobe << "marker_" << MarrkerArrayID;

            myListener->waitForTransform("world",markerGlobe.str(),ros::Time(0),ros::Duration(2.0));
            try
            {
                myListener->lookupTransform("world",markerGlobe.str(),ros::Time(0),AllMarkers[MarrkerArrayID].AllMarkersTransformGlobe);
            }
            catch(tf::TransformException &ex)
            {
                ROS_ERROR("Error - 2 Global position of marker\n");
                ros::Duration(2.0).sleep();
            }

            // Saving TF to Pose
            const tf::Vector3 marker_origin=AllMarkers[MarrkerArrayID].AllMarkersTransformGlobe.getOrigin();
            AllMarkers[MarrkerArrayID].AllMarkersPoseGlobe.position.x=marker_origin.getX();
            AllMarkers[MarrkerArrayID].AllMarkersPoseGlobe.position.y=marker_origin.getY();
            AllMarkers[MarrkerArrayID].AllMarkersPoseGlobe.position.z=marker_origin.getZ();
            tf::Quaternion marker_quaternion=AllMarkers[MarrkerArrayID].AllMarkersTransformGlobe.getRotation();
            AllMarkers[MarrkerArrayID].AllMarkersPoseGlobe.orientation.x=marker_quaternion.getX();
            AllMarkers[MarrkerArrayID].AllMarkersPoseGlobe.orientation.y=marker_quaternion.getY();
            AllMarkers[MarrkerArrayID].AllMarkersPoseGlobe.orientation.z=marker_quaternion.getZ();
            AllMarkers[MarrkerArrayID].AllMarkersPoseGlobe.orientation.w=marker_quaternion.getW();
        }
    }
    //------------------------------------------------------


    //------------------------------------------------------
    // Calculating ot the shortest camera distance to some marker
    // Camera with the shortest distance is used as reference of global position of object (camera)
    //------------------------------------------------------

    bool someMarkersAreVisible=false;
    int numberOfVisibleMarkers=0;

    if(lookingForFirst==true)
    {
        double minSize=999999;
        for(int k=0;k<numberOfAllMarkers;k++)
        {
            double a;
            double b;
            double c;
            double size;
            // If marker is active, distance is calculated
            if(AllMarkers[k].active==true)
            {
                a=AllMarkers[k].CurrentCameraPose.position.x;
                b=AllMarkers[k].CurrentCameraPose.position.y;
                c=AllMarkers[k].CurrentCameraPose.position.z;
                size=std::sqrt((a*a)+(b*b)+(c*c));
                if(size<minSize)
                {
                    minSize=size;
                    indexActualCamera=k;
                }

                someMarkersAreVisible=true;
                numberOfVisibleMarkers++;
            }
        }
    }
    //------------------------------------------------------

    //------------------------------------------------------
    // Publish all known markers
    //------------------------------------------------------
    if(lookingForFirst==true)
        publish_tfs(true);
    //------------------------------------------------------

    //------------------------------------------------------
    // Calculating TF od closer camera
    // Camera with the shortest distance is used as reference of global position of object (camera)
    //------------------------------------------------------
    //---
    if((lookingForFirst==true)&&(someMarkersAreVisible==true))
    {
        std::stringstream closestCamera;
        closestCamera << "camera_" << indexActualCamera;

        myListener->waitForTransform("world",closestCamera.str(),ros::Time(0),ros::Duration(2.0));
        try
        {
            myListener->lookupTransform("world",closestCamera.str(),ros::Time(0),myWorldPosition);
        }
        catch(tf::TransformException &ex)
        {
            ROS_ERROR("Error - 3\n");
        }

        // Saving TF to Pose
        const tf::Vector3 marker_origin=myWorldPosition.getOrigin();
        myWorldPositionPose.position.x=marker_origin.getX();
        myWorldPositionPose.position.y=marker_origin.getY();
        myWorldPositionPose.position.z=marker_origin.getZ();
        tf::Quaternion marker_quaternion=myWorldPosition.getRotation();
        myWorldPositionPose.orientation.x=marker_quaternion.getX();
        myWorldPositionPose.orientation.y=marker_quaternion.getY();
        myWorldPositionPose.orientation.z=marker_quaternion.getZ();
        myWorldPositionPose.orientation.w=marker_quaternion.getW();
    }
    //------------------------------------------------------

    //------------------------------------------------------
    // Publish all known markers
    //------------------------------------------------------
    if(lookingForFirst==true)
        publish_tfs(true);
    //------------------------------------------------------

    //------------------------------------------------------
    // Publisging ArUcoMarkersPose message
    //------------------------------------------------------
    if((someMarkersAreVisible==true))
    {
        ArUcoMarkersMsgs.header.stamp=ros::Time::now();
        ArUcoMarkersMsgs.header.frame_id="world";
        ArUcoMarkersMsgs.numberOfMarkers=numberOfVisibleMarkers;
        ArUcoMarkersMsgs.visibility=true;
        ArUcoMarkersMsgs.globalPose=myWorldPositionPose;
        ArUcoMarkersMsgs.markersID.clear();
        ArUcoMarkersMsgs.markersPose.clear();
        ArUcoMarkersMsgs.cameraPose.clear();
        for(int j=0;j<markersCounter;j++)
        {
            if(AllMarkers[j].active==true)
            {
                ArUcoMarkersMsgs.markersID.push_back(AllMarkers[j].markerID);
                ArUcoMarkersMsgs.markersPose.push_back(AllMarkers[j].AllMarkersPoseGlobe);
                ArUcoMarkersMsgs.cameraPose.push_back(AllMarkers[j].CurrentCameraPose);
            }
        }
    }
    else
    {
        ArUcoMarkersMsgs.header.stamp=ros::Time::now();
        ArUcoMarkersMsgs.header.frame_id="world";
        ArUcoMarkersMsgs.numberOfMarkers=numberOfVisibleMarkers;
        ArUcoMarkersMsgs.visibility=false;
        ArUcoMarkersMsgs.markersID.clear();
        ArUcoMarkersMsgs.markersPose.clear();
    }
    //------------------------------------------------------

    // Publish
    my_markers_pub.publish(ArUcoMarkersMsgs);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void
ViewPoint_Estimator::publish_tfs(bool world_option)
{
    for(int j=0;j<markersCounter;j++)
    {
        // Actual Marker
        std::stringstream markerTFID;
        markerTFID << "marker_" << j;
        // Older marker - or World
        std::stringstream markerTFID_old;
        if(j==0)
            markerTFID_old << "world";
        else
            markerTFID_old << "marker_" << AllMarkers[j].relatedMarkerID;
        myBroadcaster.sendTransform(tf::StampedTransform(AllMarkers[j].AllMarkersTransform,ros::Time::now(),markerTFID_old.str(),markerTFID.str()));

        // Position of camera to its marker
        std::stringstream cameraTFID;
        cameraTFID << "camera_" << j;
        myBroadcaster.sendTransform(tf::StampedTransform(AllMarkers[j].CurrentCameraTf,ros::Time::now(),markerTFID.str(),cameraTFID.str()));

        if(world_option==true)
        {
            // Global position of marker TF
            std::stringstream markerGlobe;
            markerGlobe << "marker_globe_" << j;
            myBroadcaster.sendTransform(tf::StampedTransform(AllMarkers[j].AllMarkersTransformGlobe,ros::Time::now(),"world",markerGlobe.str()));
        }

        // Cubes for RVIZ - markers
        publish_marker(AllMarkers[j].AllMarkersPose,AllMarkers[j].markerID,j);
    }

    // Global Position of object
    if(world_option==true)
        myBroadcaster.sendTransform(tf::StampedTransform(myWorldPosition,ros::Time::now(),"world","myGlobalPosition"));
}

////////////////////////////////////////////////////////////////////////////////////////////////

void
ViewPoint_Estimator::publish_marker(geometry_msgs::Pose markerPose, int MarkerID, int rank)
{
    visualization_msgs::Marker myMarker;

    if(rank==0)
        myMarker.header.frame_id="world";
    else
    {
        std::stringstream markerTFID_old;
        markerTFID_old << "marker_" << AllMarkers[rank].relatedMarkerID;
        myMarker.header.frame_id=markerTFID_old.str();
    }

    myMarker.header.stamp=ros::Time::now();
    myMarker.ns="basic_shapes";
    myMarker.id=MarkerID;
    myMarker.type=visualization_msgs::Marker::CUBE;
    myMarker.action=visualization_msgs::Marker::ADD;

    myMarker.pose=markerPose;
    myMarker.scale.x=markerSize;
    myMarker.scale.y=markerSize;
    myMarker.scale.z=0.01;

    myMarker.color.r=1.0f;
    myMarker.color.g=1.0f;
    myMarker.color.b=1.0f;
    myMarker.color.a=1.0f;

    myMarker.lifetime=ros::Duration(0.2);

    marker_pub.publish(myMarker);
}

////////////////////////////////////////////////////////////////////////////////////////////////

tf::Transform
ViewPoint_Estimator::arucoMarker2Tf(const aruco::Marker &marker)
{
    cv::Mat marker_rotation(3,3, CV_32FC1);
    cv::Rodrigues(marker.Rvec, marker_rotation);
    cv::Mat marker_translation = marker.Tvec;

    cv::Mat rotate_to_ros(3,3,CV_32FC1);
    rotate_to_ros.at<float>(0,0) = -1.0;
    rotate_to_ros.at<float>(0,1) = 0;
    rotate_to_ros.at<float>(0,2) = 0;
    rotate_to_ros.at<float>(1,0) = 0;
    rotate_to_ros.at<float>(1,1) = 0;
    rotate_to_ros.at<float>(1,2) = 1.0;
    rotate_to_ros.at<float>(2,0) = 0.0;
    rotate_to_ros.at<float>(2,1) = 1.0;
    rotate_to_ros.at<float>(2,2) = 0.0;

    marker_rotation = marker_rotation * rotate_to_ros.t();

    // Origin solution
    tf::Matrix3x3 marker_tf_rot(marker_rotation.at<float>(0,0),marker_rotation.at<float>(0,1),marker_rotation.at<float>(0,2),
                                marker_rotation.at<float>(1,0),marker_rotation.at<float>(1,1),marker_rotation.at<float>(1,2),
                                marker_rotation.at<float>(2,0),marker_rotation.at<float>(2,1),marker_rotation.at<float>(2,2));

    tf::Vector3 marker_tf_tran(marker_translation.at<float>(0,0),
                               marker_translation.at<float>(1,0),
                               marker_translation.at<float>(2,0));

    return tf::Transform(marker_tf_rot, marker_tf_tran);
}

////////////////////////////////////////////////////////////////////////////////

bool
ViewPoint_Estimator::load_calibration_file(std::string filename)
{
    std::cout << "Reading calibration file from: " << filename << std::endl;
    try
    {
        //Searching camera matrix and distortion in calibration textfile
        //# oST version 5.0 parameters
        string camera_matrix_str("camera matrix");
        string distortion_str("distortion");

        // Object of reading file
        ifstream file;
        file.open(filename.c_str());

        // Alocation of memory
        intrinsics=new(cv::Mat)(3,3,CV_64F);
        distortion_coeff=new(cv::Mat)(5,1,CV_64F);
        image_size=new(cv::Size);

        //  Reading of calibration file lines
        std::string line;
        int line_counter = 0;
        while(getline(file, line))
        {
            // Camera matrix 3x3
            if(line==camera_matrix_str)
            {
                for(size_t i=0;i<3;i++)
                    for(size_t j=0;j<3;j++)
                        file >> intrinsics->at<double>(i,j);
                std::cout << "Intrinsics:" << std::endl << *intrinsics << std::endl;
            }
            // Distortion 5x1
            if(line==distortion_str)
            {
                for(size_t i=0; i<5;i++)
                file >> distortion_coeff->at<double>(i,0);
                std::cout << "Distortion: " << *distortion_coeff << std::endl;
            }
            line_counter++;
        }
        arucoCalibParams.setParams(*intrinsics, *distortion_coeff, *image_size);
        if ((intrinsics->at<double>(2,2)==1)&&(distortion_coeff->at<double>(0,4)==0))
            ROS_INFO_STREAM("Calibration file loaded successfully");
        else
            ROS_WARN("WARNING: Suspicious calibration data");
    }
    catch(int e)
    {
        std::cout << "An exception n." << e << "occured";
    }
}

////////////////////////////////////////////////////////////////////////////////
