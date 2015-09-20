#include "../structure.h"
#include "clusterer.h"
#include "visualizer.h"


int main(int argc, const char** argv) {
  try {
    ugproj::Configuration cfg;
    if (cfg.load(argc, argv)) {
      throw new std::exception();
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

    // Matrix that includes the result of PCA. Each row represents a face. The
    // order of faces should follow that of face tracklets and that of faces in
    // a tracklet.
    // i.e. Given tracklet #1 {face #1, face #2}, tracklet #2 {face #3},
    // repr_faces_reduced should have dimensionality-reduced vector for
    // face #1, face #2, and face #3 in order, from the first row.
    cv::Mat faces_reduced;

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
