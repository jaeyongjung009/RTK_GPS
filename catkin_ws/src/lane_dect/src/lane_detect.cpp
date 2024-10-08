#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <string>
#include <vector>
#include "opencv2/opencv.hpp"
#include "RoadLaneDetector.h"
#include <fstream>  // 파일 입출력을 위한 헤더 추가
#include <ros/ros.h>
#include <geometry_msgs/Twist.h>
#include <std_msgs/Float64.h> // Float64 메시지 타입 추가

using namespace cv;
using namespace std;

void on_trackbar(int, void*) {}

int l_h = 0, l_s = 0, l_v = 100; // 초기값 설정
int u_h = 255, u_s = 255, u_v = 255;
int l_h_hls = 0, l_l = 0, l_s_hls = 100; // HLS 하한 값
int u_h_hls = 255, u_l = 255, u_s_hls = 255; // HLS 상한 값
double steering_left_angle = 0;
double steering_left_angle_degrees = 0;
double steering_right_angle = 0;
double steering_right_angle_degrees = 0;
double steering_angle = 0;

void createTrackbars() {
    // 트랙바 생성
    namedWindow("Trackbars");
    createTrackbar("L - H", "Trackbars", &l_h, 179, on_trackbar);
    createTrackbar("L - S", "Trackbars", &l_s, 255, on_trackbar);
    createTrackbar("L - V", "Trackbars", &l_v, 255, on_trackbar);
    createTrackbar("U - H", "Trackbars", &u_h, 179, on_trackbar);
    createTrackbar("U - S", "Trackbars", &u_s, 255, on_trackbar);
    createTrackbar("U - V", "Trackbars", &u_v, 255, on_trackbar);

	createTrackbar("L - H1", "Trackbars", &l_h, 179, on_trackbar);
    createTrackbar("L - L1", "Trackbars", &l_s, 255, on_trackbar);
    createTrackbar("L - S1", "Trackbars", &l_v, 255, on_trackbar);
    createTrackbar("U - H1", "Trackbars", &u_h, 179, on_trackbar);
    createTrackbar("U - L1", "Trackbars", &u_s, 255, on_trackbar);
    createTrackbar("U - S1", "Trackbars", &u_v, 255, on_trackbar);
}

int main(int argc , char **argv)
{
    createTrackbars();
    int imageCenterX = 640; 

	RoadLaneDetector roadLaneDetector;

	Mat img_frame, img_filter, img_edges, img_mask, img_lines, img_result;
	Mat bird_of_eye_view;
	vector<Vec4i> lines;
	vector<vector<Vec4i>> separated_lines;
	vector<Point> lane;
	string dir;
	string turnDirection;

	vector<double> polyregre;
    //VideoCapture video("/dev/video2", cv::CAP_V4L2);
	//VideoCapture video("test_video.mp4");
    VideoCapture video("/home/adg/catkin_ws/src/lane_detect/src/halla_trail.mp4");
		if (!video.isOpened())
	{
		cout << "동영상 파일을 열 수 없습니다. \n" << endl;
		getchar();
		return -1;
	}

	video.read(img_frame);
	
	if (img_frame.empty())
	{
		cout << "영상 프레임이 비어 있습니다.\n";
		return -1;
	}

	VideoWriter writer;
	int codec = VideoWriter::fourcc('X', 'V', 'I', 'D'); // 원하는 코덱 선택
	double fps = 25.0;                                    // 프레임
	string filename = "./result.avi";                     // 결과 파일

	writer.open(filename, codec, fps, img_frame.size(), true);
	if (!writer.isOpened())
	{
		cout << "출력을 위한 비디오 파일을 열 수 없습니다. \n";
		return -1;
	}
	
    ros::init(argc, argv, "lane_detect");
    ros::NodeHandle nh;
    ros::Rate loop_rate(60);
    
    std_msgs::Float64 angle_msg;
    std_msgs::Float64 velocity_msg;
    ros::Publisher goal_angle_pub = nh.advertise<std_msgs::Float64>("/camera_angle_goal", 10);
    ros::Publisher goal_velocity_pub = nh.advertise<std_msgs::Float64>("/camera_velocity_goal", 10);

    int img_center = 640;

	while (ros::ok)
	{       
        if (!video.read(img_frame))
            break;

        cv::resize(img_frame, img_frame, cv::Size(1280, 720));
        Mat transformed_frame = roadLaneDetector.bird_eye_view(img_frame);
        Mat mask = roadLaneDetector.img_filter(transformed_frame);
        
        vector<int> l = roadLaneDetector.Start_lane_detection(mask);
        Mat msk = mask.clone();

        Mat result = roadLaneDetector.sliding_window(transformed_frame, mask, l[0], l[1]);

        const double WHEELBASE = 0.75;

        if (roadLaneDetector.lx.size() != 0 && roadLaneDetector.lx.size() > 4) {
            vector<double> left_coeffs = roadLaneDetector.polyfit(roadLaneDetector.lx, roadLaneDetector.y_vals_left, 2);
            vector<double> right_coeffs = roadLaneDetector.polyfit(roadLaneDetector.rx, roadLaneDetector.y_vals_right, 2);

            for (double x : roadLaneDetector.lx) {
                double curvature = roadLaneDetector.calculateCurvature(left_coeffs, static_cast<double>(x));
                steering_left_angle = atan(WHEELBASE / curvature);
                steering_left_angle_degrees = steering_left_angle * (180.0 / M_PI);
                cout.precision(4);
                circle(transformed_frame , Point(roadLaneDetector.lx[x], 0) ,5 , (0,0,255),-1);

                double middleX = (roadLaneDetector.rx[x] + roadLaneDetector.lx[x]) / 2;

                if (middleX < img_center) {
                    turnDirection = "Left Turn";
                } else if (middleX > img_center) {
                    turnDirection = "Right Turn";
                } else {
                    turnDirection = "Straight";
                }

                cout << "Detected turn direction: " << turnDirection << endl;
            }

            for (double x : roadLaneDetector.rx) {
                double curvature = roadLaneDetector.calculateCurvature(right_coeffs, static_cast<double>(x));
                steering_right_angle = atan(WHEELBASE / curvature);
                steering_right_angle_degrees = steering_right_angle * (180.0 / M_PI);

                circle(transformed_frame , Point((roadLaneDetector.rx[x]+roadLaneDetector.lx[x])/2, 0) ,5 , (0,0,255),-1);
            }

       
            
        steering_angle = (steering_left_angle_degrees + steering_right_angle_degrees) / 2;
            
        }

        if (turnDirection == "Right Turn") {
                steering_angle = steering_angle*-1;
            
        }
        // angle_msg와 velocity_msg 설정
        angle_msg.data = steering_angle;
        velocity_msg.data = 0.5; // 예시로 속도를 0.5로 설정

        goal_angle_pub.publish(angle_msg);
        goal_velocity_pub.publish(velocity_msg);

        imshow("img_frame", img_frame);
        imshow("transformed_frame", transformed_frame);
        imshow("mask", mask);
        imshow("result", result);

        writer.write(img_frame);

        if (waitKey(100) == 27){
            break; 
        }
    }

	video.release();
	writer.release();
	
	return 0;
}
