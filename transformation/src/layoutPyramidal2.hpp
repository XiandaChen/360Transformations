#pragma once
#include "layoutPyramidalBased.hpp"

namespace IMT {
class LayoutPyramidal2 : public LayoutPyramidalBased
{
    public:
        /** \brief Contructor of a LayoutPyramidal2 (with black pixels)
         *
         *            h
         *   +-----+-----+-----+
         *   |        .        |
         *   |       / \       |
         *   |      / T \      |
         *   |------------------
         *   |    /|     |\    |
         *   |  /  |     |  \  |
         *   |/  L |  Ba |  R \|  h
         *   |\    |     |    /|
         *   |  \  |     |  /  |
         *   |    \|     |/    |
         *   |-----------------|
         *   |      \ Bo/      |
         *   |       \ /       |
         *   |        .        |
         *   +-----+-----+-----+
         * \param baseEdge double  size of the edge of the base in the 3D space
         * \param yaw double
         * \param pitch double
         * \param roll double
         * \param pixelBaseEdge unsigned int Number of pixel of the edge of the base
         *
         */
        LayoutPyramidal2(double baseEdge,double yaw, double pitch, double roll, unsigned int pixelBaseEdge):
            LayoutPyramidalBased(baseEdge, yaw, pitch, roll, 3*pixelBaseEdge, 3*pixelBaseEdge), m_edge(pixelBaseEdge) {};
        virtual ~LayoutPyramidal2(void) = default;

        virtual NormalizedFaceInfo From2dToNormalizedFaceInfo(const CoordI& pixel) const override;
        virtual CoordF FromNormalizedInfoTo2d(const NormalizedFaceInfo& ni) const override;

    protected:

    private:
        unsigned int m_edge;
};
}