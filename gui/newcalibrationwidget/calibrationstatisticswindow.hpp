/*****************************************************************
 * File:			calibrationstatisticswindow.hpp
 * Created: 	05. August 2021
 * Author:   	Timo Hüser
 * Contact: 	timo.hueser@gmail.com
 ******************************************************************/

#ifndef CALIBRATIONSTATISTICSWINDOW_H
#define CALIBRATIONSTATISTICSWINDOW_H

#include "globals.hpp"
#include "calibrationchartview.hpp"


#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QDialog>
#include <QStackedWidget>

class CalibrationStatisticsWindow : public QDialog {
	Q_OBJECT
	public:
		explicit CalibrationStatisticsWindow(QList<QString> cameraNames, QList<QList<QString>> cameraPairs, QMap<int,double> intrinsicsReproErrors,
						QMap<int,double> extrinsicsReproErrors, QWidget *parent = nullptr);

	public slots:

	signals:

	private:
		CalibrationChartView *intrinsicsChartView;
		CalibrationChartView *extrinsicsChartView;

		QPushButton *closeButton;

		QList<QString> m_cameraNames;
		QList<QList<QString>> m_cameraPairs;
		QMap<int,double> m_intrinsicsReproErrors;
		QMap<int,double> m_extrinsicsReproErrors;


	private slots:

};

#endif