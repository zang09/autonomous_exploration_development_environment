#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ros/ros.h>

#include <nav_msgs/Odometry.h>
#include <sensor_msgs/PointCloud2.h>

#include <tf/transform_datatypes.h>
#include <tf/transform_broadcaster.h>

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <message_filters/subscriber.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/synchronizer.h>

using namespace std;

pcl::PointCloud<pcl::PointXYZI>::Ptr laserCloudIn(new pcl::PointCloud<pcl::PointXYZI>());
pcl::PointCloud<pcl::PointXYZI>::Ptr laserCLoudInSensorFrame(new pcl::PointCloud<pcl::PointXYZI>());

double robotX = 0;
double robotY = 0;
double robotZ = 0;
double roll = 0;
double pitch = 0;
double yaw = 0;

bool newTransformToMap = false;

nav_msgs::Odometry odometryIn;
ros::Publisher *pubOdometryPointer = NULL;
tf::StampedTransform transformToMap;
tf::TransformBroadcaster *tfBroadcasterPointer = NULL;

ros::Publisher pubLaserCloud;

void laserCloudAndOdometryHandler(const nav_msgs::Odometry::ConstPtr& odometry,
                                  const sensor_msgs::PointCloud2ConstPtr& laserCloud2)
{
  laserCloudIn->clear();
  laserCLoudInSensorFrame->clear();

  pcl::fromROSMsg(*laserCloud2, *laserCloudIn);

  odometryIn = *odometry;

  transformToMap.setOrigin(
      tf::Vector3(odometryIn.pose.pose.position.x, odometryIn.pose.pose.position.y, odometryIn.pose.pose.position.z));
  transformToMap.setRotation(tf::Quaternion(odometryIn.pose.pose.orientation.x, odometryIn.pose.pose.orientation.y,
                                            odometryIn.pose.pose.orientation.z, odometryIn.pose.pose.orientation.w));

  int laserCloudInNum = laserCloudIn->points.size();

  pcl::PointXYZI p1;
  tf::Vector3 vec;

  for (int i = 0; i < laserCloudInNum; i++)
  {
    p1 = laserCloudIn->points[i];

    float dist = sqrt(p1.x*p1.x + p1.y*p1.y);
    if(dist < 1.0) //0.8
        continue;

    vec.setX(p1.x);
    vec.setY(p1.y);
    vec.setZ(p1.z);

    vec = transformToMap * vec;

    p1.x = vec.x();
    p1.y = vec.y();
    p1.z = vec.z();

    laserCLoudInSensorFrame->points.push_back(p1);
  }

  odometryIn.header.stamp = laserCloud2->header.stamp;
  odometryIn.header.frame_id = "/map";
  pubOdometryPointer->publish(odometryIn);

  sensor_msgs::PointCloud2 scan_data;
  pcl::toROSMsg(*laserCLoudInSensorFrame, scan_data);
  scan_data.header.stamp = laserCloud2->header.stamp;
  scan_data.header.frame_id = "/map";
  pubLaserCloud.publish(scan_data);
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "sensor_scan");
  ros::NodeHandle nh;
  ros::NodeHandle nhPrivate = ros::NodeHandle("~");

  // ROS message filters
  message_filters::Subscriber<nav_msgs::Odometry> subOdometry;
  message_filters::Subscriber<sensor_msgs::PointCloud2> subLaserCloud;
  typedef message_filters::sync_policies::ApproximateTime<nav_msgs::Odometry, sensor_msgs::PointCloud2> syncPolicy;
  typedef message_filters::Synchronizer<syncPolicy> Sync;
  boost::shared_ptr<Sync> sync_;
  subOdometry.subscribe(nh, "/hdl_localization/ndt_pose", 1);
  subLaserCloud.subscribe(nh, "/hdl_localization/raw_points", 1);
  sync_.reset(new Sync(syncPolicy(100), subOdometry, subLaserCloud));
  sync_->registerCallback(boost::bind(laserCloudAndOdometryHandler, _1, _2));

  ros::Publisher pubOdometry = nh.advertise<nav_msgs::Odometry> ("/state_estimation", 5);
  pubOdometryPointer = &pubOdometry;

  tf::TransformBroadcaster tfBroadcaster;
  tfBroadcasterPointer = &tfBroadcaster;

  pubLaserCloud = nh.advertise<sensor_msgs::PointCloud2>("/registered_scan", 2);

  ros::spin();

  return 0;
}
