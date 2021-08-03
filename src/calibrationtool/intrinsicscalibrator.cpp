/*------------------------------------------------------------
 *  intrinsicscalibrator.cpp
 *  Created: 30. July 2021
 *  Author:  Timo Hüser
 * Contact: 	timo.hueser@gmail.com
 *------------------------------------------------------------*/

#include "intrinsicscalibrator.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <QThreadPool>
#include <QDir>

IntrinsicsCalibrator::IntrinsicsCalibrator(CalibrationConfig *calibrationConfig, const QString& cameraName, int threadNumber) :
  m_calibrationConfig(calibrationConfig), m_cameraName(cameraName.toStdString()), m_threadNumber(threadNumber){
  QDir dir;
  dir.mkpath(m_calibrationConfig->calibrationSetPath + "/" + m_calibrationConfig->calibrationSetName + "/Intrinsics");
  m_parametersSavePath = (m_calibrationConfig->calibrationSetPath + "/" + m_calibrationConfig->calibrationSetName).toStdString();
}


void IntrinsicsCalibrator::run() {
  std::cout << "Detecting Checkerboards for Camera_" << m_cameraName << "..." << std::endl;
  std::vector<cv::Point3f> checkerBoardPoints;
  for (int i = 0; i < m_calibrationConfig->patternHeight; i++)
    for (int j = 0; j < m_calibrationConfig->patternWidth; j++)
      checkerBoardPoints.push_back(cv::Point3f((float)j * m_calibrationConfig->patternSideLength, (float)i * m_calibrationConfig->patternSideLength, 0));
  std::vector<std::string> fileNames;
  cv::VideoCapture cap(m_calibrationConfig->intrinsicsPath.toStdString() + "/" + m_cameraName + ".avi");
  int frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
  std::cout << frameCount << std::endl;


  std::vector< std::vector< cv::Point3f > > objectPointsAll, objectPoints;
  std::vector< std::vector< cv::Point2f > > imagePointsAll, imagePoints;
  std::vector< cv::Point2f > corners;
  cv::Size size;
  cbdetect::Corner cbCorners;
  std::vector<cbdetect::Board> boards;
  cbdetect::Params params;
  params.corner_type = cbdetect::SaddlePoint;
  params.show_processing = false;
  params.show_debug_image = false;

  bool read_success = true;
  int counter = 0;
  while (read_success) {
    cv::Mat img;
    read_success = cap.read(img);
    if (read_success) {
      corners.clear();
      size = img.size();
      int frameIndex = cap.get(cv::CAP_PROP_POS_FRAMES);
      std::cout << frameIndex << std::endl;
      cap.set(cv::CAP_PROP_POS_FRAMES, frameIndex+40);
      if (frameIndex+40 > frameCount) read_success = false;
      cbdetect::find_corners(img, cbCorners, params);
      bool patternFound = (cbCorners.p.size() >= m_calibrationConfig->patternHeight*m_calibrationConfig->patternWidth);

      if (patternFound) {
        cbdetect::boards_from_corners(img, cbCorners, boards, params);
        patternFound = boardToCorners(boards[0], cbCorners, corners);
        checkRotation(corners, img);
      }
      if (patternFound) {
        imagePointsAll.push_back(corners);
        objectPointsAll.push_back(checkerBoardPoints);
      }
      emit intrinsicsProgress(counter*40, frameCount, m_threadNumber);
      counter++;
    }
  }

  double keep_ratio = imagePointsAll.size() / (double)std::min(m_calibrationConfig->framesForIntrinsics, (int)imagePointsAll.size());
  for (double k = 0; k < imagePointsAll.size(); k += keep_ratio) {
    imagePoints.push_back(imagePointsAll[(int)k]);
    objectPoints.push_back(objectPointsAll[(int)k]);
  }

  std::cout << "Calibrating Camera_" << m_cameraName << " using "<< imagePoints.size() << " Images..." << std::endl;
  cv::Mat K, D;
  std::vector< cv::Mat > rvecs, tvecs;
  double repro_error = calibrateCamera(objectPoints, imagePoints, size, K, D, rvecs, tvecs,
    cv::CALIB_FIX_K3 | cv::CALIB_ZERO_TANGENT_DIST, cv::TermCriteria(cv::TermCriteria::MAX_ITER | cv::TermCriteria::EPS, 80, 1e-6));
  std::cout << "Camera_" << m_cameraName <<  " calibrated with Reprojection Error: " << repro_error << std::endl;

  cv::FileStorage fs1(m_parametersSavePath + "/Intrinsics/Intrinsics_" + m_cameraName + ".yaml", cv::FileStorage::WRITE);
  fs1 << "intrinsicMatrix" << K.t();
  fs1 << "distortionCoefficients" << D;
}


void IntrinsicsCalibrator::checkRotation(std::vector< cv::Point2f> &corners1, cv::Mat &img1) {
  int width = m_calibrationConfig->patternWidth;
  int height = m_calibrationConfig->patternHeight;
	cv::Point2i ctestd;
	cv::Point2f p1 = corners1[width*height-1];
	cv::Point2f p2 = corners1[width*height-2];
	cv::Point2f p3 = corners1[width*(height-1)-1];
	cv::Point2f p4 = corners1[width*(height-1)-2];
	ctestd.x = (p1.x + p2.x + p3.x + p4.x) / 4;
	ctestd.y = (p1.y + p2.y + p3.y + p4.y) / 4;

	cv::Point2i ctestl;
	p1 = corners1[0];
	p2 = corners1[1];
	p3 = corners1[width];
	p4 = corners1[width+1];
	ctestl.x = (p1.x + p2.x + p3.x + p4.x) / 4;
	ctestl.y = (p1.y + p2.y + p3.y + p4.y) / 4;

	cv::Vec3b colord = img1.at<cv::Vec3b>(ctestd.y,ctestd.x);
	int color_sum_d = colord[0]+colord[1]+colord[2];
	cv::Vec3b colorl = img1.at<cv::Vec3b>(ctestl.y,ctestl.x);
	int color_sum_l = colorl[0]+colorl[1]+colorl[2];

	if (color_sum_d > color_sum_l) {
		std::reverse(corners1.begin(),corners1.end());
	}
}


bool IntrinsicsCalibrator::boardToCorners(cbdetect::Board &board, cbdetect::Corner &cbCorners, std::vector<cv::Point2f> &corners) {
  if (board.idx.size()-2 == 6) {
    for(int i = 1; i < board.idx.size() - 1; ++i) {
      for(int j = 1; j < board.idx[i].size() - 1; ++j) {
        if(board.idx[i][j] < 0) {
          return false;
        }
        corners.push_back(static_cast<cv::Point2f>(cbCorners.p[board.idx[i][j]]));
      }
    }
  }
  else {
    for(int j = 1; j < board.idx[0].size() - 1; ++j) {
      for(int i = 1; i < board.idx.size() - 1; ++i) {
        if(board.idx[board.idx.size() - 1 -i][j] < 0) {
          return false;
        }
        corners.push_back(static_cast<cv::Point2f>(cbCorners.p[board.idx[board.idx.size() - 1 -i][j]]));
      }
    }
  }
  return true;
}
