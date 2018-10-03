/*
Copyright (c) 2015, Helen Oleynikova, ETH Zurich, Switzerland
You can contact the author at <helen dot oleynikova at mavt dot ethz dot ch>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of ETHZ-ASL nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ETHZ-ASL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
* Modified by Tung Dang, University of Nevada, Reno.
* The provided code is an implementation of the visual saliency-aware
* exploration algorithm.
*/

#ifndef OCTOMAP_WORLD_OCTOMAP_WORLD_H_
#define OCTOMAP_WORLD_OCTOMAP_WORLD_H_

#include <string>

#include <octomap/SaliencyOcTree.h>
#include <octomap/octomap.h>

#include <cv_bridge/cv_bridge.h>
#include <octomap_msgs/Octomap.h>
#include <std_msgs/ColorRGBA.h>
#include <vector>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <volumetric_map_base/world_base.h>

#define LOGODDS(x)                                                             \
  (((x != 1) && (x != 0))                                                      \
       ? log(x / (1 - x))                                                      \
       : ((x == 1) ? 10 : -10)) // CAUTION:haven't tried with 0 or 1
#define LOGODDS_INVERSE(l) (1.0 / (1 + exp(-l)))

namespace volumetric_mapping {

struct SaliencyParameters {
  SaliencyParameters()
      : alpha(0.7), beta(-0.0008), saliency_threshold(125), timestamp(0),
        projection_limit(5.0), yaw_lb(-180.0), yaw_ub(180.0), yaw_bins(0),
        r_lb(1.0), r_ub(3.0), r_bins(0), logodds_abnormal(LOGODDS(0.7)),
        logodds_normal(LOGODDS(0.1)), logodds_prior(LOGODDS(0.5)),
        logodds_abnormal_thres(LOGODDS(0.85)),
        logodds_normal_thres(LOGODDS(0.4)), ground_limit(-10) {}
  double alpha;            // ratio to mix 2 saliency values
  double beta;             // decay coefficient for IOR
  int saliency_threshold;  // threshold to consider as a saliency voxel
  int timestamp;           // hold the time tick of the system
  double projection_limit; // max proj distance of ray casting (-1: unlimited)
  double ground_limit;
  float yaw_lb;
  float yaw_ub;
  int yaw_bins;
  float r_lb;
  float r_ub;
  int r_bins;
  float logodds_abnormal;
  float logodds_normal;
  float logodds_prior;
  float logodds_abnormal_thres;
  float logodds_normal_thres;
};

struct OctomapParameters {
  OctomapParameters()
      : resolution(0.15), probability_hit(0.65), probability_miss(0.4),
        threshold_min(0.12), threshold_max(0.97), threshold_occupancy(0.7),
        filter_speckles(true), max_free_space(0.0), min_height_free_space(0.0),
        sensor_max_range(5.0),
        visualize_min_z(-std::numeric_limits<double>::max()),
        visualize_max_z(std::numeric_limits<double>::max()),
        treat_unknown_as_occupied(true), change_detection_enabled(false) {
    // Set reasonable defaults here...
  }

  // Resolution for the Octree. It is not possible to change this without
  // creating a new Octree.
  double resolution;
  // Hit probabilities for pointcloud data.
  double probability_hit;
  double probability_miss;
  // Clamping thresholds for pruning: above and below these thresholds, all
  // values are treated the same.
  double threshold_min;
  double threshold_max;
  // Threshold considered for a cell to be occupied.
  double threshold_occupancy;

  // Filter neighbor-less nodes as 'speckles'.
  bool filter_speckles;

  // Maximum range to allow a free space update.
  double max_free_space;

  // Minimum height below sensor to allow a free space update.
  double min_height_free_space;

  // Maximum range to allow a sensor measurement. Negative values to not
  // filter.
  double sensor_max_range;

  // Minimum and maximum z to visualize. Only used for marker, not full
  // octomap, visualization.
  double visualize_min_z;
  double visualize_max_z;

  // Collision checking.
  bool treat_unknown_as_occupied;

  // Whether to track changes -- must be set to true to use getChangedPoints().
  bool change_detection_enabled;
};

// A wrapper around octomap that allows insertion from various ROS message
// data sources, given their transforms from sensor frame to world frame.
// Does not need to run within a ROS node, does not do any TF look-ups, and
// does not publish/subscribe to anything (though provides serialization
// and deserialization functions to and from ROS messages).
class OctomapWorld : public WorldBase {
  typedef std::shared_ptr<OctomapWorld> Ptr;

public:
  // Default constructor - creates a valid octree using parameter defaults.
  OctomapWorld();

  // Creates an octomap with the correct parameters.
  OctomapWorld(const OctomapParameters &params);
  virtual ~OctomapWorld() {}

  // General map management.
  void resetMap();
  void prune();
  // Creates an octomap if one is not yet created or if the resolution of the
  // current varies from the parameters requested.
  void setOctomapParameters(const OctomapParameters &params);

  // Virtual functions for manually manipulating map probabilities.
  virtual void setFree(const Eigen::Vector3d &position,
                       const Eigen::Vector3d &bounding_box_size,
                       const Eigen::Vector3d &bounding_box_offset =
                           Eigen::Vector3d(0.0, 0.0, 0.0));
  virtual void setOccupied(const Eigen::Vector3d &position,
                           const Eigen::Vector3d &bounding_box_size);

  // Virtual functions for outputting map status.
  virtual CellStatus
  getCellStatusBoundingBox(const Eigen::Vector3d &point,
                           const Eigen::Vector3d &bounding_box_size) const;
  virtual CellStatus getCellStatusPoint(const Eigen::Vector3d &point) const;
  virtual CellStatus getCellProbabilityPoint(const Eigen::Vector3d &point,
                                             double *probability) const;
  virtual CellStatus getCuriousGain(const Eigen::Vector3d &point,
                                    double *gain) const;

  void setVoxelToEval(const Eigen::Vector3d &origin,
                      const Eigen::Vector3d &point, float z);

  CellStatus getEstEntropy(const Eigen::Vector3d &origin,
                           const Eigen::Vector3d &point, float z, double *gain);
  float getDiffEntropyFromPath(const std::vector<Eigen::Vector3d> &origins,
                               const Eigen::Vector3d &point);
  bool isAbnormalVoxel(const Eigen::Vector3d &point);
  float getEntropy(octomap::SaliencyOcTreeNode::Saliency &sal);
  float getPotentialEntropy(octomap::SaliencyOcTreeNode::Saliency &sal,
                            int r_ind, int yaw_ind);

  virtual CellStatus getLineStatus(const Eigen::Vector3d &start,
                                   const Eigen::Vector3d &end) const;
  virtual CellStatus getVisibility(const Eigen::Vector3d &view_point,
                                   const Eigen::Vector3d &voxel_to_test,
                                   bool stop_at_unknown_cell) const;
  virtual CellStatus
  getLineStatusBoundingBox(const Eigen::Vector3d &start,
                           const Eigen::Vector3d &end,
                           const Eigen::Vector3d &bounding_box_size) const;
  virtual void
  getOccupiedPointCloud(pcl::PointCloud<pcl::PointXYZ> *output_cloud) const;
  virtual void getOccupiedPointcloudInBoundingBox(
      const Eigen::Vector3d &center, const Eigen::Vector3d &bounding_box_size,
      pcl::PointCloud<pcl::PointXYZ> *output_cloud) const;

  // Structure: vector of pairs, key is the cube center and double is the
  // dimension of each side.
  void getAllFreeBoxes(
      std::vector<std::pair<Eigen::Vector3d, double>> *free_box_vector) const;
  void getAllOccupiedBoxes(std::vector<std::pair<Eigen::Vector3d, double>>
                               *occupied_box_vector) const;

  virtual double getResolution() const;
  virtual Eigen::Vector3d getMapCenter() const;
  virtual Eigen::Vector3d getMapSize() const;
  virtual void getMapBounds(Eigen::Vector3d *min_bound,
                            Eigen::Vector3d *max_bound) const;

  // Collision checking with robot model. Implemented as a box with our own
  // implementation.
  virtual void setRobotSize(const Eigen::Vector3d &robot_size);
  virtual Eigen::Vector3d getRobotSize() const;
  virtual bool checkCollisionWithRobot(const Eigen::Vector3d &robot_position);
  // Checks a path (assumed to be time-ordered) for collision.
  // Sets the second input to the index at which the collision occurred.
  virtual bool checkPathForCollisionsWithRobot(
      const std::vector<Eigen::Vector3d> &robot_positions,
      size_t *collision_index);

  // Serialization and deserialization from ROS messages.
  bool getOctomapBinaryMsg(octomap_msgs::Octomap *msg) const;
  bool getOctomapFullMsg(octomap_msgs::Octomap *msg) const;
  // Clears the current octomap and replaces it with one from the message.
  void setOctomapFromMsg(const octomap_msgs::Octomap &msg);

  // Loading and writing to disk.
  bool loadOctomapFromFile(const std::string &filename);
  bool writeOctomapToFile(const std::string &filename);

  // Helpers for publishing.
  void generateMarkerArray(const std::string &tf_frame,
                           visualization_msgs::MarkerArray *occupied_nodes,
                           visualization_msgs::MarkerArray *free_nodes);

  // Add voxel information and some APIs
  pcl::PointCloud<pcl::PointXYZ> ProjCloud;
  void generateProjectionMarker(const std::string &tf_frame,
                                visualization_msgs::Marker *line_list);
  void updateIOR(void);

  void updateSaliency(octomap::SaliencyOcTreeNode *n, unsigned char sal_val);

  // Change detection -- when this is called, this resets the change detection
  // tracking within the map. So 2 consecutive calls will produce first the
  // change set, then nothing.
  // If not NULL, changed_states contains the new state of the node -- 1 is
  // occupied, 0 is free.
  // IMPORTANT NOTE: change_detection MUST be set to true in the parameters in
  // order for this to work!
  void getChangedPoints(std::vector<Eigen::Vector3d> *changed_points,
                        std::vector<bool> *changed_states);

  Eigen::Matrix4d getCameraPose(void);

  void clearBBX(const Eigen::Vector3d &point,
                const Eigen::Vector3d &bounding_box_size);

  float getAreaOverPixel(float z);
  float getPixelOverArea(float z);
  bool getExplorationRate(double *e);
  double getVolumePercentage(double v);
  void setWorkspaceBox(double minx, double miny, double minz, double maxx,
                       double maxy, double maxz) {
    minx_ = minx;
    miny_ = miny;
    minz_ = minz;
    maxx_ = maxx;
    maxy_ = maxy;
    maxz_ = maxz;
  }

  void setIOR(float alpha, float beta, float sal_thres) {
    salconfig_.alpha = alpha;
    salconfig_.beta = beta;
    salconfig_.saliency_threshold = (unsigned char)sal_thres;
  }

  void setGroundRemoval(double level) { z_ground = level; }

  bool getHeatMapColor(float value, float &red, float &green, float &blue);
  bool getSalMapColor(float value, float &red, float &green, float &blue);

protected:
  // Actual implementation for inserting disparity data.
  virtual void
  insertProjectedDisparityIntoMapImpl(const Transformation &sensor_to_world,
                                      const cv::Mat &projected_points);

  // Actual implementation for inserting pointclouds.
  virtual void insertPointcloudIntoMapImpl(
      const Transformation &T_G_sensor,
      const pcl::PointCloud<pcl::PointXYZ>::Ptr &pointcloud);

  virtual void insertPointcloudColorIntoMapImpl(
      const Transformation &T_G_sensor,
      const pcl::PointCloud<pcl::PointXYZRGB>::Ptr &pointcloud);

  virtual void insertSaliencyImageIntoMapImpl(const Transformation &T_G_sensor,
                                              const cv_bridge::CvImagePtr &img);

  image_geometry::PinholeCameraModel CamModel;
  virtual void setCameraModelImpl(image_geometry::PinholeCameraModel &camInfo);

  // Check if the node at the specified key has neighbors or not.
  bool isSpeckleNode(const octomap::OcTreeKey &key) const;

  // Manually affect the probabilities of areas within a bounding box.
  /* void setLogOddsBoundingBox(const Eigen::Vector3d& position,
                              const Eigen::Vector3d& bounding_box_size,
                              double log_odds_value); */
  // Manually affect the probabilities of areas within a bounding box.
  void setLogOddsBoundingBox(
      const Eigen::Vector3d &position, const Eigen::Vector3d &bounding_box_size,
      double log_odds_value,
      const Eigen::Vector3d &offset = Eigen::Vector3d(0.0, 0.0, 0.0));

  void getAllBoxes(
      bool occupied_boxes,
      std::vector<std::pair<Eigen::Vector3d, double>> *box_vector) const;

  // Helper functions for building up a map from sensor data.
  int castRay(const octomap::point3d &sensor_origin,
              const octomap::point3d &point, octomap::KeySet *free_cells,
              octomap::KeySet *occupied_cells) const;
  void updateOccupancy(octomap::KeySet *free_cells,
                       octomap::KeySet *occupied_cells);
  bool isValidPoint(const cv::Vec3f &point) const;

  void setOctomapFromBinaryMsg(const octomap_msgs::Octomap &msg);
  void setOctomapFromFullMsg(const octomap_msgs::Octomap &msg);

  double colorizeMapByHeight(double z, double min_z, double max_z) const;

  // Collision checking methods.
  bool checkSinglePoseCollision(const Eigen::Vector3d &robot_position) const;

  std_msgs::ColorRGBA percentToColor(double h) const;
  std_msgs::ColorRGBA getEncodedColor(octomap::SaliencyOcTree::iterator it);

  std::shared_ptr<octomap::SaliencyOcTree> octree_;

  OctomapParameters params_;
  SaliencyParameters salconfig_;
  Transformation camerapose_;
  // For collision checking.
  Eigen::Vector3d robot_size_;

  double exp_percent_ = 0;
  double exp_percent_rate_ = 0;
  double exp_percent_prev_ = 0;
  ros::Time time_last_;
  double time_past_ = 0;
  bool start_timing_ = true;
  double minx_ = 0;
  double miny_ = 0;
  double minz_ = 0;
  double maxx_ = 0;
  double maxy_ = 0;
  double maxz_ = 0;

  double z_ground = -100;
};

} // namespace volumetric_mapping

#endif // OCTOMAP_WORLD_OCTOMAP_WORLD_H_
