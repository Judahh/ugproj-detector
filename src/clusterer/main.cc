#include "../structure.h"
#include "clusterer.h"
#include "visualizer.h"

#include <opencv2/highgui/highgui.hpp>

cv::Mat norm_0_255(const cv::Mat& src){
    cv::Mat dst;
    switch(src.channels()){
        case 1:
            cv::normalize(src,dst,0,255,cv::NORM_MINMAX, CV_8UC1);
            break;
        case 3:
            cv::normalize(src,dst,0,255,cv::NORM_MINMAX, CV_8UC3);
            break;
        default:
            src.copyTo(dst);
            break;
    }
    return dst;
}

int main(int argc, const char** argv) {
  try {
    ugproj::Configuration cfg;
    if (cfg.load(argc, argv)) {
      return 1;
    }

    ugproj::FileWriter writer;
    if (writer.init(cfg)) {
      throw new std::exception();
    }

    // TODO: Load face tracklets and frame index - frame position mapping from
    // output files from tracker.
    //
    // Assigned to: Naing0126
    ugproj::ClustererFileInput input;
    if (input.open(cfg)) {
      throw new std::exception();
    }

    // Mapping between frame indices and frame positions.
    const std::vector<unsigned long>& tracked_positions =
        input.tracked_positions();
    // Face tracklets generated by tracker.
    const std::vector<ugproj::FaceTracklet>& tracklets =
        input.tracklets();

    // TODO: Run dimensionality reduction on faces in face tracklets using PCA.
    //
    // Assigned to: Naing0126

    std::vector<cv::Mat> faceSet;

    // Change to grey scale image
    for(ugproj::FaceTracklet tracklet : tracklets){
      const auto iterators = tracklet.face_iterators();
      for(auto it = iterators.first; it != iterators.second; ++it){
        const ugproj::Face& f = *it;
        cv::Mat greyFace, colorFace;
        colorFace = f.get_image();
        cv::cvtColor(colorFace, greyFace, CV_BGR2GRAY);

        faceSet.push_back(greyFace);
      }
    }

    // Converts the images into a (n x d) matrix
    int rtype = CV_32FC1;
    double alpha = 1, beta = 0;
    // Number of samples
    size_t n = faceSet.size();
    // dimensionality of (reshaped) samples
    size_t d = faceSet[0].total();
    // resulting data matrix
    cv::Mat faces(n, d, rtype);
    int i;
    for(i = 0; i < n; i++){
      if(faceSet[i].empty()){
        std::string error_message = cv::format("Image number %d was empty",i);
        CV_Error(CV_StsBadArg, error_message);
      }
      if(faceSet[i].total() != d){
        std::string error_message = cv::format("Wrong number of elements in matrix #%d!",i);
        CV_Error(CV_StsBadArg, error_message);
      }
      cv::Mat xi = faces.row(i);
      if(faceSet[i].isContinuous()){
        faceSet[i].reshape(1,1).convertTo(xi, rtype, alpha, beta);
      }else{
        faceSet[i].clone().reshape(1,1).convertTo(xi, rtype, alpha, beta);
      }
    }

    std::cout << faces.size() << std::endl;

    // Matrix that includes the result of PCA. Each row represents a face. The
    // order of faces should follow that of face tracklets and that of faces in
    // a tracklet.
    // i.e. Given tracklet #1 {face #1, face #2}, tracklet #2 {face #3},
    // repr_faces_reduced should have dimensionality-reduced vector for
    // face #1, face #2, and face #3 in order, from the first row.
    cv::Mat faces_reduced;
    cv::PCA pca(faces, cv::noArray(), CV_PCA_DATA_AS_ROW, 2);
    faces_reduced = pca.project(faces);

    std::cout << faces_reduced.size() << std::endl;

    // TODO: Make clusterer accept tracklet information and assign a cluster
    // label to each tracklet by voting.
    //
    // Assigned to: kyukyukyu

    // List of cluster IDs for face tracklets.
    std::vector<int> cluster_ids;
    // Clusterer object for faces.
    ugproj::FaceClusterer clusterer(cfg);
    clusterer.do_clustering(faces_reduced, tracklets, &cluster_ids);

    // Writes the visualization of result of clustering to multiple files.
    ugproj::FaceClustersVisualizer visualizer(&writer);
    visualizer.visualize(tracked_positions, tracklets,
                         cfg.clustering.k, cluster_ids);
  
  } catch (std::exception&) {
    return 1;
  }
  return 0;
}
