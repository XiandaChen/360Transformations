/**********************************
 * Institut Mine-Telecom / Telecom Bretagne
 * Author: Xavier CORBILLON
 *
 * Program developped to test the 360° coding methods
 */

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <array>
#include <memory>
#include <math.h>
#include <chrono>

#include "boost/program_options.hpp"
#include <boost/config.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <sstream>
#include <fstream>

#include <opencv2/opencv.hpp>

#include "Picture.hpp"
#include "Layout.hpp"
#include "ConfigParser.hpp"
#include "VideoWriter.hpp"
#include "VideoReader.hpp"

#define DEBUG 0
#if DEBUG
#define PRINT_DEBUG(x) std::cout << x << std::endl;
#else
#define PRINT_DEBUG(x) {}
#endif // DEBUG

using namespace IMT;

int main( int argc, const char* argv[] )
{
   namespace po = boost::program_options;
   namespace pt = boost::property_tree;
   po::options_description desc("Options");
   desc.add_options()
      ("help,h", "Produce this help message")
      //("inputVideo,i", po::value<std::string>(), "path to the input video")
      ("config,c", po::value<std::string>(),"Path to the configuration file")
      ;

   po::variables_map vm;
   try
   {
      po::store(po::parse_command_line(argc, argv, desc),
            vm);

      //--help
      if ( vm.count("help") || !vm.count("config"))
      {
         std::cout << "Help: trans -c config"<< std::endl
            <<  desc << std::endl;
         return 0;
      }

      po::notify(vm);

      //Get the path to the configuration file
      std::string pathToIni = vm["config"].as<std::string>();

      std::cout << "Path to the ini file: " <<pathToIni << std::endl;

      //read the ini file the feed the GlobalArgsStructure
      pt::ptree ptree;
      pt::ptree ptree_json;
      pt::ini_parser::read_ini(pathToIni, ptree);

      std::vector<std::vector<std::string>> layoutFlowSections; //contains the layoutFlow information
      std::vector<std::string> pathToInputVideos; //contains the path to the input videos. index i from layoutFlow i
      //Parse the layoutFlow line from the configuration configuration file
      try {
        std::stringstream ss(ptree.get<std::string>("Global.layoutFlow"));
        pt::json_parser::read_json(ss, ptree_json);
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, ptree_json.get_child(""))
        {
            std::vector<std::string> lfsv;
            bool first = true;
            BOOST_FOREACH(boost::property_tree::ptree::value_type & u, v.second)
            {
                if (first)
                {
                    first = false;
                    pathToInputVideos.push_back(u.second.data());
                }
                else
                {
                    lfsv.push_back(u.second.data());
                }
            }
            if (lfsv.size()>0)
            {
                layoutFlowSections.push_back(std::move(lfsv));
            }
        }
      }
      catch (std::exception &e)
      {
          std::cout << "Error while parsing the Global.layoutFlow: " << e.what() << std::endl;
          exit(1);
      }

      constexpr unsigned int mask_msssim = 1<<0;
      constexpr unsigned int mask_ssim = 1<<1;
      constexpr unsigned int mask_psnr = 1<<2;
      constexpr unsigned int mask_spsnrnn = 1<<3;
      constexpr unsigned int mask_spsnri = 1<<4;
      constexpr unsigned int mask_wspsnr = 1<<5;

      unsigned int qualityToMeasure = 0;

      try {
        std::stringstream ss(ptree.get<std::string>("Global.qualityToComputeList"));
        pt::json_parser::read_json(ss, ptree_json);
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, ptree_json.get_child(""))
        {
            if (v.second.data() == "MS-SSIM")
            {
              qualityToMeasure |= mask_msssim;
            }
            else if (v.second.data() == "SSIM")
            {
              qualityToMeasure |= mask_ssim;
            }
            else if (v.second.data() == "PSNR")
            {
              qualityToMeasure |= mask_psnr;
            }
            else if (v.second.data() == "S-PSNR-NN")
            {
              qualityToMeasure |= mask_spsnrnn;
            }
            else if (v.second.data() == "S-PSNR-I")
            {
              qualityToMeasure |= mask_spsnri;
            }
            else if (v.second.data() == "WS-PSNR")
            {
              qualityToMeasure |= mask_wspsnr;
            }
        }
      }
      catch (std::exception &e)
      {
          std::cout << "Error while parsing the Global.layoutFlow: " << e.what() << std::endl;
          exit(1);
      }


      //Parse the rest of the Global Section from the configuration file
      std::string pathToOutputVideo = ptree.get<std::string>("Global.videoOutputName");
      auto outputVideoCodecOpt = ptree.get_optional<std::string>("Global.videoOutputCodec");
      std::string outputVideoCodec = "libx265";
      if (outputVideoCodecOpt && outputVideoCodecOpt.get().size() > 0)
      {
        outputVideoCodec = outputVideoCodecOpt.get();
      }
      auto processingStepOpt = ptree.get_optional<int>("Global.processingStep");
      int processingStep = 1;
      if (processingStepOpt && processingStepOpt.get() > 1)
      {
          processingStep = processingStepOpt.get();
      }
      std::string pathToOutputQuality = ptree.get<std::string>("Global.qualityOutputName");
      bool displayFinalPict = ptree.get<bool>("Global.displayFinalPict");
      auto nbFrames = ptree.get<unsigned int>("Global.nbFrames");
      auto startFrame = ptree.get<unsigned int>("Global.startFrame");
      auto videoOutputBitRate = ptree.get<unsigned int>("Global.videoOutputBitRate")*1000;
      double fps = ptree.get<double>("Global.fps");
      auto interpolTechOpt = ptree.get_optional<std::string>("Global.interpolation");
      Picture::InterpolationTech interpol = Picture::InterpolationTech::BILINEAR;
      if (interpolTechOpt && interpolTechOpt.get().size() > 0)
      {
        if (interpolTechOpt.get() == "NEAREST_NEIGHTBOOR")
        {
            interpol = Picture::InterpolationTech::NEAREST_NEIGHTBOOR;
        }
        else if (interpolTechOpt.get() == "BILINEAR")
        {
            interpol = Picture::InterpolationTech::BILINEAR;
        }
        else if (interpolTechOpt.get() == "BICUBIC")
        {
            interpol = Picture::InterpolationTech::BICUBIC;
        }
        else
        {
            std::cout << "Interpolation " << interpolTechOpt.get() << " not recognized; BILINEAR interpolation will be used instead" << std::endl;
        }
      }

      //This vector contains the shared pointer of each layout named in the LayoutFlowSections
      std::vector<std::vector<std::shared_ptr<Layout>>> layoutFlowVect;

      //Populate the layoutFlowVect. Will read the configuration file to get information about each layout named in the LayoutFlowSections
      unsigned j = 0;
      for(auto& lfsv: layoutFlowSections)
      {
          LayoutStatus layoutStatus = LayoutStatus::Input;
          layoutFlowVect.push_back(std::vector<std::shared_ptr<Layout>>());
          CoordI refResolution ( 0, 0 );
          unsigned k = 0;
          for(auto& lfs: lfsv)
          {
              layoutFlowVect.back().push_back(InitialiseLayout(lfs, ptree, layoutStatus, refResolution.x, refResolution.y));
              layoutFlowVect.back().back()->Init();
              layoutFlowVect.back().back()->SetInterpolationTech(interpol);
              refResolution = layoutFlowVect.back().back()->GetReferenceResolution();
              ++k;
              if (layoutStatus == LayoutStatus::Input)
              {
                layoutStatus = LayoutStatus::Intermediate;
              }
              if (k == lfsv.size()-1)
              {
                layoutStatus = LayoutStatus::Output;
              }
          }
          ++j;
      }

      //Initilise input video for each first layout in the layoutFlowVect
      j = 0;
      for (auto& inputPath:pathToInputVideos)
      {
        PRINT_DEBUG("Start init input video for flow "<<j+1)
        layoutFlowVect[j][0]->InitInputVideo(inputPath, nbFrames);
        PRINT_DEBUG("Done init input video for flow "<<j+1)
        ++j;
      }

      //Init ouput video for each last video in the layoutFlowVect
      if (!pathToOutputVideo.empty())
      {
          size_t lastindex = pathToOutputVideo.find_last_of(".");
          std::string pathToOutputVideoExtension = pathToOutputVideo.substr(lastindex, pathToOutputVideo.size());
          pathToOutputVideo = pathToOutputVideo.substr(0, lastindex);
          unsigned int j = 0;
          for(auto& lfsv: layoutFlowSections)
          {
              const auto& l = layoutFlowVect[j].back();
              std::string path = pathToOutputVideo+std::to_string(j+1)+lfsv.back()+pathToOutputVideoExtension;
              std::cout << "Output video path for flow "<< j+1 <<": " << path << std::endl;

              auto bitrate = GetBitrateVector(lfsv.back(), ptree, videoOutputBitRate);
              l->InitOutputVideo(path, outputVideoCodec, fps/processingStep, int(fps/(2*processingStep)), bitrate);
              ++j;
          }
      }
      //Init the quality ouput files
      std::vector<std::shared_ptr<std::ofstream>> qualityWriterVect;
      if (!pathToOutputQuality.empty())
      {
          size_t lastindex = pathToOutputQuality.find_last_of(".");
          std::string pathToOutputQualityExtension = pathToOutputQuality.substr(lastindex, pathToOutputQuality.size());
          pathToOutputQuality = pathToOutputQuality.substr(0, lastindex);
          unsigned int j = 0;
          for(auto& lfsv: layoutFlowSections)
          {
              if (j != 0)
              {
                  const auto& l = layoutFlowVect[j].back();
                  std::string path = pathToOutputQuality+std::to_string(j+1)+lfsv.back()+pathToOutputQualityExtension;
                  std::cout << "Quality path for flow "<< j+1 <<": " << path << std::endl;
                  qualityWriterVect.push_back(std::make_shared<std::ofstream>(path));
              }
              ++j;
          }
      }
      //      cv::VideoWriter vwriter(pathToOutputVideo, cv::VideoWriter::fourcc('D','A','V','C'), sga.fps, cv::Size(lcm.GetWidth(), lcm.GetHeight()));

      cv::Mat img;
      int count = 0;
      double averageDuration = 0;
      while (count < nbFrames+startFrame)
      {
        auto startTime = std::chrono::high_resolution_clock::now();

        std::cout << (count >= startFrame ? "Read" : "Skip") << " image " << count << std::endl;

        unsigned int j = 0;
        std::shared_ptr<Picture> firstPict(nullptr);
        for(auto& lf: layoutFlowVect)
        {
          std::shared_ptr<Picture> pict = lf[0]->ReadNextPictureFromVideo();
          if (count >= startFrame && (count - startFrame)%processingStep == 0)
          {//start processing when count >= startFrame

            auto pictOut = pict;
            std::cout << "Flow " << j << ": " << layoutFlowSections[j][0];
            for (unsigned int i = 1; i < lf.size(); ++i)
            {
                std::cout << " -> " << layoutFlowSections[j][i];
                lf[i]->NextStep(double(count-startFrame)/fps);
                pictOut = lf[i]->FromLayout(*pictOut, *lf[i-1]);
            }
            std::cout << std::endl;
            if (firstPict == nullptr)
            {
                firstPict = pictOut;
            }
            if (displayFinalPict)
            {
                pictOut->ImgShowWithLimit("Output"+std::to_string(j)+": "+layoutFlowSections[j][lf.size()-1], cv::Size(1200,900));
            }
            if (!qualityWriterVect.empty() && qualityToMeasure != 0  && j != 0)
            {
                std::cout << "Flow " << j << ": ";
                bool first = true;
                if (count == 0)
                {
                  if (qualityToMeasure & mask_msssim)
                  {
                    if (!first)
                    {
                      *qualityWriterVect[j-1] << " ";
                    }
                    else
                    {
                      first = false;
                    }
                    *qualityWriterVect[j-1] <<"MS-SSIM";
                  }
                  if (qualityToMeasure & mask_ssim)
                  {
                    if (!first)
                    {
                      *qualityWriterVect[j-1] << " ";
                    }
                    else
                    {
                      first = false;
                    }
                    *qualityWriterVect[j-1] <<"SSIM";
                  }
                  if (qualityToMeasure & mask_psnr)
                  {
                    if (!first)
                    {
                      *qualityWriterVect[j-1] << " ";
                    }
                    else
                    {
                      first = false;
                    }
                    *qualityWriterVect[j-1] <<"PSNR";
                  }
                  if (qualityToMeasure & mask_spsnrnn)
                  {
                    if (!first)
                    {
                      *qualityWriterVect[j-1] << " ";
                    }
                    else
                    {
                      first = false;
                    }
                    *qualityWriterVect[j-1] <<"S-PSNR-NN";
                  }
                  if (qualityToMeasure & mask_spsnri)
                  {
                    if (!first)
                    {
                      *qualityWriterVect[j-1] << " ";
                    }
                    else
                    {
                      first = false;
                    }
                    *qualityWriterVect[j-1] <<"S-PSNR-I";
                  }
                  if (qualityToMeasure & mask_wspsnr)
                  {
                    if (!first)
                    {
                      *qualityWriterVect[j-1] << " ";
                    }
                    else
                    {
                      first = false;
                    }
                    *qualityWriterVect[j-1] <<"WS-PSNR";
                  }
                  *qualityWriterVect[j-1] << std::endl;
                }
                first = true;
                if (qualityToMeasure & mask_msssim)
                {
                  auto msssim = firstPict->GetMSSSIM(*pictOut);
                  std::cout << "MS-SSIM = " << msssim <<";";
                  if (!first)
                  {
                    *qualityWriterVect[j-1] << " ";
                  }
                  else
                  {
                    first = false;
                  }
                  *qualityWriterVect[j-1] << msssim;
                }
                if (qualityToMeasure & mask_ssim)
                {
                  auto ssim = firstPict->GetSSIM(*pictOut);
                  std::cout << " SSIM = " << ssim <<";";
                  if (!first)
                  {
                    *qualityWriterVect[j-1] << " ";
                  }
                  else
                  {
                    first = false;
                  }
                  *qualityWriterVect[j-1] << ssim;
                }
                if (qualityToMeasure & mask_psnr)
                {
                  auto psnr = firstPict->GetPSNR(*pictOut);
                  std::cout << " PSNR = " << psnr <<";";
                  if (!first)
                  {
                    *qualityWriterVect[j-1] << " ";
                  }
                  else
                  {
                    first = false;
                  }
                  *qualityWriterVect[j-1] << psnr;
                }
                if (qualityToMeasure & mask_spsnrnn)
                {
                  auto spsnrnn = firstPict->GetSPSNR(*pictOut, *layoutFlowVect[0].back(), *lf.back(), Picture::InterpolationTech::NEAREST_NEIGHTBOOR);
                  std::cout << " S-PSNR-NN = " << spsnrnn <<";";
                  if (!first)
                  {
                    *qualityWriterVect[j-1] << " ";
                  }
                  else
                  {
                    first = false;
                  }
                  *qualityWriterVect[j-1] << spsnrnn;
                }
                if (qualityToMeasure & mask_spsnri)
                {
                  auto spsnri = firstPict->GetSPSNR(*pictOut, *layoutFlowVect[0].back(), *lf.back(), Picture::InterpolationTech::BICUBIC);
                  std::cout << " S-PSNR-I = "<< spsnri <<";";
                  if (!first)
                  {
                    *qualityWriterVect[j-1] << " ";
                  }
                  else
                  {
                    first = false;
                  }
                  *qualityWriterVect[j-1] << spsnri;
                }
                if (qualityToMeasure & mask_wspsnr)
                {
                  auto wspsnr = firstPict->GetWSPSNR(*pictOut, *layoutFlowVect[0].back(), *lf.back());
                  std::cout << " WS-PSNR = "<< wspsnr <<";";
                  if (!first)
                  {
                    *qualityWriterVect[j-1] << " ";
                  }
                  else
                  {
                    first = false;
                  }
                  *qualityWriterVect[j-1] << wspsnr;
                }
                std::cout <<std::endl;
                *qualityWriterVect[j-1] << std::endl;
            }
            if (!pathToOutputVideo.empty())
            {
                PRINT_DEBUG("Send picture to encoder "<<j+1)
                lf.back()->WritePictureToVideo(pictOut);
            }
            ++j;
          }
        }

        if (count >= startFrame)
        {
          if (displayFinalPict && (count - startFrame)%processingStep == 0)
          {
            cv::waitKey(0);
            cv::destroyAllWindows();
          }

          auto endTime = std::chrono::high_resolution_clock::now();
          auto duration = std::chrono::duration_cast<std::chrono::microseconds>( endTime - startTime ).count();
          averageDuration = (averageDuration*count + duration)/(count+1);
          std::cout << "Elapsed time for this picture: "<< print_time(long(float(duration)/1000.f)) << " "
            "estimated remaining time = " << print_time(long((nbFrames-count-1)*averageDuration/1000.f)) << " "  << std::endl;
        }
        if (++count >= nbFrames+startFrame)
        {
            break;
        }
      }
   }
   catch(const po::error& e)
   {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl
         << desc << std::endl;
      return 1;
   }
   catch(std::exception& e)
   {
      std::cerr << "Uncatched exception: " << e.what() << std::endl
         << desc << std::endl;
      return 1;

   }
   catch(...)
   {
      std::cerr << "Uncatched exception" << std::endl
        << desc << std::endl;
      return 1;

   }

   return 0;
}
