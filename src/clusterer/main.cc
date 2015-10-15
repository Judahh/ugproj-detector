#include "../structure.h"
#include "clusterer.h"
#include "visualizer.h"

#include <opencv2/highgui/highgui.hpp>
#include <math.h>
#include <complex>

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
    std::vector<int> labelSet;

    // Change to grey scale image
    int faceCnt = 0;
    for(ugproj::FaceTracklet tracklet : tracklets){
      const auto iterators = tracklet.face_iterators();
      for(auto it = iterators.first; it != iterators.second; ++it){
        const ugproj::Face& f = *it;
        cv::Mat greyFace, colorFace;
        colorFace = f.get_image();
        cv::cvtColor(colorFace, greyFace, CV_BGR2GRAY);

        faceSet.push_back(greyFace);
        labelSet.push_back(faceCnt++);
      }
    }

    // Find EigenFace
    cv::Ptr<cv::FaceRecognizer> model = cv::createEigenFaceRecognizer();
    model->train(faceSet, labelSet);

    cv::Mat eigenvalues = model->getMat("eigenvalues");
    cv::Mat W = model->getMat("eigenvectors");
    cv::Mat mean = model->getMat("mean");

    // Convert the face images into a (n x d) matrix.

    // Number of face images.
    const auto n_faces = faceSet.size();
    // Length of one side of face images.
    const auto len_side = cfg.output.face_size;
    // Dimensionality of (reshaped) samples.
    const auto d = len_side * len_side;
    // resulting data matrix
    cv::Mat faces(n_faces, d, CV_64FC1);
    for (size_t i = 0; i < n_faces; ++i) {
      // A row of matrix faces where convert result should be stored.
      if (faceSet[i].empty()) {
        std::string error_message = cv::format("Image number %d was empty",i);
        CV_Error(CV_StsBadArg, error_message);
      }
      if (faceSet[i].total() != d) {
        std::string error_message = cv::format("Wrong number of elements in matrix #%d!",i);
        CV_Error(CV_StsBadArg, error_message);
      }
      cv::Mat xi = faces.row(i);
      if (faceSet[i].isContinuous()) {
        faceSet[i].reshape(1, 1).convertTo(xi, CV_64FC1);
      } else {
        faceSet[i].clone().reshape(1, 1).convertTo(xi, CV_64FC1);
      }
    }

    // Compute weights for eigenvectors of faces.
    cv::Mat evs = cv::Mat(W, cv::Range::all(), cv::Range(0, std::min(W.cols, 32)));
    cv::Mat weights = faces * evs;

    // Compute affinity matrix.

    // Affinity matrix.
    cv::Mat affinity;
    // Parameter that controls the width of neighborhoods.
    const double sigma = 2000.0;
    affinity.create(n_faces, n_faces, CV_64FC1);
    for (int i = 0; i < weights.rows; ++i) {
      affinity.at<double>(i, i) = 0.0;
      for (int j = i + 1; j < weights.rows; ++j) {
        double norm;
        cv::Mat diff = weights.row(i) - weights.row(j);
        norm = cv::norm(diff);
        affinity.at<double>(i, j) = affinity.at<double>(j, i) =
            std::exp(-(norm * norm) / (2 * sigma * sigma));
      }
    }

    // Run normalized spectral clustering according to Ng, Jordan, and Weiss
    // (2002).
    // == Start of normalized spectral clustering. ============================

    // Compute degree matrix D.

    // Degree vector deg.
    cv::Mat deg(1, affinity.cols, CV_64FC1);
    // Degree matrix D, which is diagonalized version of deg.
    cv::Mat D;
    for (int i = 0; i < affinity.cols; ++i) {
      // Degree for node i.
      double deg_i = 0.0;
      for (int j = 0; j < affinity.rows; ++j) {
        deg_i += affinity.at<double>(i, j);
      }
      deg.at<double>(0, i) = deg_i;
    }
    D = cv::Mat::diag(deg);
    deg.release();

    // Calculate normalized Laplacian matrix.

    // Laplacian matrix L.
    cv::Mat L = D - affinity;
    affinity.release();
    // D^-(1/2). m stands for 'minus', and o stands for point. (.)
    cv::Mat D_mo5;
    // Normalized Laplacian matrix L_sym.
    cv::Mat L_sym;
    // Eigenvalues and corresponding eigenvectors of L_sym. Eigenvalues are
    // stored in the descending order, and eigenvectors are stored in the same
    // order.
    cv::Mat eval_L_sym, evec_L_sym;
    // Compute D^-(1/2).
    cv::pow(D, 0.5, D_mo5);
    D_mo5 = D_mo5.inv();
    D.release();
    // Normalize L.
    L_sym = (D_mo5 * L) * D_mo5;
    L.release();
    // Compute eigenvalues and eigenvectors of L_sym.
    cv::eigen(L_sym, eval_L_sym, evec_L_sym);

    // Compute matrix U whose columns are top k eigenvectors of L_sym, then
    // form a matrix T from U by normalizing the rows to norm 1.

    // Matrix U.
    cv::Mat U = evec_L_sym.rowRange(0, 64).t();
    // Matrix T.
    cv::Mat T;
    // Temporary matrix for each row of T.
    cv::Mat T_row;
    T.create(U.size(), CV_64FC1);
    T_row.create(1, U.cols, CV_64FC1);
    // L2-normalize each row of U and stores them into T.
    for (int i = U.rows - 1; i >= 0; --i) {
      cv::normalize(U.row(i), T_row);
      T_row.copyTo(T.row(i));
    }
    U.release();
    T_row.release();

    // Make clusterer accept tracklet information and assign a cluster
    // label to each tracklet by voting.

    // Matrix T in float-typed real numbers.
    cv::Mat T_float;
    // List of cluster IDs for face tracklets.
    std::vector<int> cluster_ids;
    // Clusterer object for faces.
    ugproj::FaceClusterer clusterer(cfg);

    // Since cv::kmeans() only accepts matrix in CV_32F, T should be converted.
    T_float.create(T.size(), CV_32FC1);
    T.convertTo(T_float, CV_32FC1);
    clusterer.do_clustering(T_float, tracklets, &cluster_ids);

    // == End of normalized spectral clustering. ==============================

    // Writes the visualization of result of clustering to multiple files.
    ugproj::FaceClustersVisualizer visualizer(cfg, &writer);
    visualizer.visualize(tracked_positions, tracklets,
                         cfg.clustering.k, cluster_ids);
  } catch (std::exception*) {
    return 1;
  }
  return 0;
}
