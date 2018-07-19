#include "Layout.hpp"
#include <stdexcept>


using namespace IMT;

Coord3dCart LayoutView::From2dTo3d(const CoordI& pixelCoord) const
{
    //return m_vectorialTrans->FromBeforeTrans3dToAfterTrans3d(FromNormalizedInfoTo3d(From2dToNormalizedFaceInfo(pixelCoord)));
    return FromNormalizedInfoTo3d(From2dToNormalizedFaceInfo(pixelCoord));
}

CoordF LayoutView::FromSphereTo2d(const Coord3dSpherical& sphericalCoord) const
{
    //return FromNormalizedInfoTo2d(From3dToNormalizedFaceInfo(m_vectorialTrans->FromAfterTrans3dToBeforeTrans3d(sphericalCoord)));
    return FromNormalizedInfoTo2d(From3dToNormalizedFaceInfo(sphericalCoord));
}

std::shared_ptr<Picture> LayoutView::ToLayout(const Picture& layoutPic, const LayoutView& destLayout) const
{
    //if (!m_isInit)
    //{
    //    throw std::logic_error("Layout have to be initialized first before using it");
    //}
    cv::Mat picMat = cv::Mat::zeros(destLayout.GetHeight(), destLayout.GetWidth(), layoutPic.GetMat().type());
    auto pic = std::make_shared<Picture>(picMat);
    #pragma omp parallel for collapse(2) shared(pic, layoutPic, destLayout) schedule(dynamic)
    for (auto i = 0; i < pic->GetMat().cols; ++i)
    {
        for (auto j = 0; j < pic->GetMat().rows; ++j)
        {
            Coord3dSpherical thisPixel3dPolar = destLayout.From2dTo3d(CoordI(i,j)); // coordinate of the pixel (i, j) in the output picture in the 3d space
            if (thisPixel3dPolar.Norm() != 0 && !std::isnan(thisPixel3dPolar.Norm()))
            {//Keep the pixel black (i.e. do nothing) if == 0
                auto coordPixelOriginalPic = FromSphereTo2d(thisPixel3dPolar); //coordinate of the corresponding pixel in the input picture
                if (inInterval(coordPixelOriginalPic.x, 0, layoutPic.GetMat().cols) && inInterval(coordPixelOriginalPic.y, 0, layoutPic.GetMat().rows))
                {
                    pic->SetValue(CoordI(i,j), layoutPic.GetInterPixel(coordPixelOriginalPic, m_interpol));
                }
                else
                {
                //std::cout << "(i,j)= (" << i << "(max = "<< pic->GetMat().cols << "), "<<j <<") : theta = " << thisPixel3dPolar.GetTheta() << "; phi=" << thisPixel3dPolar.GetPhi() << "; iOrr = " << coordPixelOriginalPic.x << "; jOrr = " << coordPixelOriginalPic.y << "Max (i,j)orr = " << layoutPic.GetMat().cols << ", " << layoutPic.GetMat().rows << std::endl;
                }
            }
        }
    }
    return pic;
}

double Layout::GetSurfacePixel(const CoordI& pixelCoord)
{
  NormalizedFaceInfo nfi_0_0 = From2dToNormalizedFaceInfo(pixelCoord);
  NormalizedFaceInfo nfi_1_0 = From2dToNormalizedFaceInfo(CoordI(pixelCoord.x+1, pixelCoord.y));
  NormalizedFaceInfo nfi_0_1 = From2dToNormalizedFaceInfo(CoordI(pixelCoord.x, pixelCoord.y+1));
  NormalizedFaceInfo nfi_1_1 = From2dToNormalizedFaceInfo(CoordI(pixelCoord.x+1, pixelCoord.y+1));
  bool ok_1_0(true), ok_0_1(true);
  if (nfi_1_0.m_faceId != nfi_0_0.m_faceId)
  {
    ok_1_0 = false;
    nfi_1_0.m_faceId = nfi_0_0.m_faceId;
    nfi_1_0.m_normalizedFaceCoordinate.x = 1;
    nfi_1_0.m_normalizedFaceCoordinate.y = nfi_0_0.m_normalizedFaceCoordinate.y;
  }
  if (nfi_0_1.m_faceId != nfi_0_0.m_faceId)
  {
    ok_0_1 = false;
    nfi_0_1.m_faceId = nfi_0_0.m_faceId;
    nfi_0_1.m_normalizedFaceCoordinate.x = nfi_0_0.m_normalizedFaceCoordinate.x;
    nfi_0_1.m_normalizedFaceCoordinate.y = 1;
  }
  if (nfi_1_1.m_faceId != nfi_0_0.m_faceId)
  {
    nfi_1_1.m_faceId = nfi_0_0.m_faceId;
    nfi_1_1.m_normalizedFaceCoordinate.x = !ok_1_0 ? 1 : nfi_1_1.m_normalizedFaceCoordinate.x;
    nfi_1_1.m_normalizedFaceCoordinate.y = !ok_0_1 ? 1 : nfi_0_0.m_normalizedFaceCoordinate.y;
  }

  Coord3dCart v_0_0 = FromNormalizedInfoTo3d(nfi_0_0);
  Coord3dCart v_1_0 = FromNormalizedInfoTo3d(nfi_1_0);
  Coord3dCart v_0_1 = FromNormalizedInfoTo3d(nfi_0_1);
  Coord3dCart v_1_1 = FromNormalizedInfoTo3d(nfi_1_1);
  //First approximation (if all vector are close)
  return ((v_1_0-v_0_0)^(v_1_1-v_0_0)).Norm()/2.0 + ((v_0_1-v_0_0)^(v_1_1-v_0_0)).Norm()/2.0;
}
