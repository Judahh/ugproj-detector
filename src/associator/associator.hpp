#ifndef UGPROJ_ASSOCIATOR_HEADER
#define UGPROJ_ASSOCIATOR_HEADER

#include "../structure.hpp"
#include "../optflow/manager.hpp"

#include <opencv2/opencv.hpp>
#include <vector>

namespace ugproj {
    class FaceAssociator {
        public:
            typedef std::vector<FaceCandidate*> fc_v;

        protected:
            std::vector<Face>& faces;
            fc_v& prevCandidates;
            fc_v& nextCandidates;
            double **prob;
            double threshold;

            void matchCandidates();

        public:
            FaceAssociator(
                    std::vector<Face>& faces,
                    fc_v& prevCandidates,
                    fc_v& nextCandidates,
                    double threshold):
                faces(faces),
                prevCandidates(prevCandidates),
                nextCandidates(nextCandidates),
                threshold(threshold) {
                    // probability array allocation
                    fc_v::size_type rows, cols;
                    rows = prevCandidates.size();
                    cols = nextCandidates.size();

                    prob = new double *[rows];
                    while (rows--) {
                        prob[rows] = new double[cols];
                    }
                }
            virtual ~FaceAssociator() {
                fc_v::size_type row = prevCandidates.size();
                while (row--) {
                    delete[] prob[row];
                }
                delete[] prob;
            }
            void associate();
            virtual void calculateProb() = 0;
    };

    class IntersectionFaceAssociator : public FaceAssociator {
        public:
            void calculateProb();
    };

    class OpticalFlowFaceAssociator : public FaceAssociator {
        private:
            OpticalFlowManager& flowManager;
            const temp_pos_t prevFramePos;
            const temp_pos_t nextFramePos;

        public:
            OpticalFlowFaceAssociator(
                    std::vector<Face>& faces,
                    fc_v& prevCandidates,
                    fc_v& nextCandidates,
                    OpticalFlowManager& flowManager,
                    const temp_pos_t prevFramePos,
                    const temp_pos_t nextFramePos,
                    double threshold);
            void calculateProb();
    };
} // ugproj

#endif