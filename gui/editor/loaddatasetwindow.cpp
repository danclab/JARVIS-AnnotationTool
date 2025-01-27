/*****************************************************************************
 * File:		loaddatasetwindow.cpp
 * Created:		23. October 2020
 * Author:		Timo Hueser
 * Contact:		timo.hueser@gmail.com
 * Copyright:	2022 Timo Hueser
 * License:		LGPL v2.1
 *****************************************************************************/

#include "loaddatasetwindow.hpp"
#include "configurableitemlist.hpp"

#include <QGridLayout>
#include <QLineEdit>
#include <QFileDialog>
#include <QErrorMessage>


LoadDatasetWindow::LoadDatasetWindow(QWidget *parent) : QDialog(parent) {
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
	setWindowTitle("Load Dataset");
	this->resize(700,500);
	this->setMinimumSize(600,200);
	settings = new QSettings();
	QGridLayout *loaddatasetlayout = new QGridLayout(this);

	QGroupBox *datasetFileBox = new QGroupBox("Select Dataset");
	QGridLayout *datasetfilelayout = new QGridLayout(datasetFileBox);
	datasetFileEdit = new QLineEdit();
	settings->beginGroup("DatasetLoader");
	connect(datasetFileEdit, &QLineEdit::textEdited,
			this, &LoadDatasetWindow::datasetFileEditedSlot);
	datasetFileButton = new QPushButton();
	datasetFileButton->setMinimumSize(40,40);
	datasetFileButton->setIcon(QIcon::fromTheme("folder"));
	connect(datasetFileButton, &QPushButton::clicked,
			this, &LoadDatasetWindow::datasetFileClickedSlot);
	datasetfilelayout->addWidget(datasetFileEdit,0,0);
	datasetfilelayout->addWidget(datasetFileButton,0,1);


	QGroupBox *datasetSegmentBox = new QGroupBox("Select Segment");
	QGridLayout *datasetsegmentboxlayout = new QGridLayout(datasetSegmentBox);
	datasetSegmentsTree = new QTreeWidget(this);
	datasetSegmentsTree->setColumnCount(1);
	datasetSegmentsTree->setExpandsOnDoubleClick(false);
	datasetSegmentsTree->setHeaderLabels({""});
	connect(datasetSegmentsTree, &QTreeWidget::currentItemChanged, this, &LoadDatasetWindow::datasetSegmentChangedSlot);
	QLabel *selectedSegmentLabel = new QLabel("Selected Segment");
	selectedSegmentEdit = new QLineEdit(datasetSegmentBox);
	selectedSegmentEdit->setPlaceholderText("Select a Segment above...");
	selectedSegmentEdit->setReadOnly(true);
	datasetsegmentboxlayout->addWidget(datasetSegmentsTree,0,0,1,2);
	datasetsegmentboxlayout->addWidget(selectedSegmentLabel,1,0);
	datasetsegmentboxlayout->addWidget(selectedSegmentEdit,1,1);


	QGroupBox *cameraOrderBox = new QGroupBox("Camera Order");
	QGridLayout *cameraorderlayout = new QGridLayout(cameraOrderBox);
	cameraOrderList = new QListWidget(cameraOrderBox);
	connect(cameraOrderList, &QListWidget::currentItemChanged,
			this, &LoadDatasetWindow::currentItemChangedSlot);
	upButton = new QPushButton();
	upButton->setMinimumSize(30,30);
	upButton->setIcon(QIcon::fromTheme("up"));
	connect(upButton, &QPushButton::clicked,
			this, &LoadDatasetWindow::moveLabelUpSlot);
	downButton = new QPushButton();
	downButton->setMinimumSize(30,30);
	downButton->setIcon(QIcon::fromTheme("down"));
	connect(downButton, &QPushButton::clicked,
			this, &LoadDatasetWindow::moveLabelDownSlot);
	cameraorderlayout->addWidget(cameraOrderList,0,0,1,2);
	cameraorderlayout->addWidget(upButton, 1,0);
	cameraorderlayout->addWidget(downButton,1,1);

	QWidget *buttonWidget = new QWidget(this);
	buttonWidget->setMaximumSize(10000,50);
	QGridLayout *buttonlayout = new QGridLayout(buttonWidget);
	buttonlayout->setContentsMargins(3,3,0,0);
	annotateSetupButton = new QPushButton("Annotate Setup", this);
	annotateSetupButton->setMinimumSize(40,40);
	annotateSetupButton->setIcon(QIcon::fromTheme("floor"));
	annotateSetupButton->setEnabled(false);
	connect(annotateSetupButton, &QPushButton::clicked,
		this, &LoadDatasetWindow::annotateSetupButtonClicked);

	QWidget *spacer = new QWidget(buttonWidget);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	loadDatasetButton = new QPushButton("Load Dataset");
	loadDatasetButton->setMinimumSize(40,40);
	loadDatasetButton->setIcon(QIcon::fromTheme("download"));
	loadDatasetButton->setEnabled(false);
	loadDatasetButton->setToolTip("Load a dataset and select a segment to annotate first!");
	connect(loadDatasetButton, &QPushButton::clicked,
			this, &LoadDatasetWindow::loadDatasetClickedSlot);
	cancelButton = new QPushButton("Cancel");
	cancelButton->setMinimumSize(40,40);
	cancelButton->setIcon(QIcon::fromTheme("discard"));
	connect(cancelButton, &QPushButton::clicked,
			this, &LoadDatasetWindow::reject);

	buttonlayout->addWidget(annotateSetupButton,0,0);
	buttonlayout->addWidget(spacer,0,1);
	buttonlayout->addWidget(cancelButton,0,2);
	buttonlayout->addWidget(loadDatasetButton,0,3);

	loaddatasetlayout->addWidget(datasetFileBox,0,0,1,2);
	loaddatasetlayout->addWidget(datasetSegmentBox,1,0);
	loaddatasetlayout->addWidget(cameraOrderBox,1,1);
	loaddatasetlayout->addWidget(buttonWidget,2,0,1,2);

}


void LoadDatasetWindow::datasetFileClickedSlot() {
	QString currentPath;
	if (datasetFileEdit->text() != "") {
		QDir curDir = QDir(datasetFileEdit->text());
		curDir.cdUp();
		currentPath = curDir.path();
	}
	else {
		currentPath = QDir::homePath();
	}

	QString path = QFileDialog::getOpenFileName(
				      this, "Load Dataset YAML File", currentPath, "YAML Files (*.yaml)");

	if (path != "") {
		m_datasetYaml = YAML::LoadFile(path.toStdString());
		datasetFileEdit->setText(path);
		updateCameraOrderList();
		updateDatasetSegmentTree();
	}
	m_skeleton.clear();
	for (const auto& compYaml : m_datasetYaml["Skeleton"]) {
		SkeletonComponent comp;
		comp.name = QString::fromStdString(compYaml.first.as<std::string>());
		comp.keypointA = QString::fromStdString(compYaml.second["Keypoints"][0].as<std::string>());
		comp.keypointB = QString::fromStdString(compYaml.second["Keypoints"][1].as<std::string>());
		comp.length = compYaml.second["Length"][0].as<float>();
		m_skeleton.append(comp);
	}
}


void LoadDatasetWindow::datasetFileEditedSlot(const QString &path) {
	QFileInfo file(path);
	if (path != "" && file.exists() && file.completeSuffix() == "yaml") {
		m_datasetYaml = YAML::LoadFile(path.toStdString());
		updateCameraOrderList();
		updateDatasetSegmentTree();
		m_skeleton.clear();
		for (const auto& compYaml : m_datasetYaml["Skeleton"]) {
			SkeletonComponent comp;
			comp.name = QString::fromStdString(compYaml.first.as<std::string>());
			comp.keypointA = QString::fromStdString(compYaml.second["Keypoints"][0].as<std::string>());
			comp.keypointB = QString::fromStdString(compYaml.second["Keypoints"][1].as<std::string>());
			comp.length = compYaml.second["Length"][0].as<float>();
			m_skeleton.append(comp);
		}
	}
	else {
		cameraOrderList->clear();
		m_cameraNames.clear();
		m_segments.clear();
		selectedSegmentEdit->setText("");
		loadDatasetButton->setEnabled(false);
		annotateSetupButton->setEnabled(false);
		loadDatasetButton->setToolTip("Load a dataset and select a segment to annotate first!");
		datasetSegmentsTree->blockSignals(true);
		datasetSegmentsTree->clear();
		m_datasetFolder = "";
	}

}

void LoadDatasetWindow::loadDatasetClickedSlot() {
	if (m_datasetFolder != "") {
		QDir baseDir = QFileInfo(datasetFileEdit->text()).absoluteDir();
		QString baseDirPath = baseDir.absolutePath();
		Dataset::dataset = new Dataset(m_datasetFolder, baseDirPath,
									   m_cameraNames, m_skeleton, m_segments, false);
		if (Dataset::dataset->loadSuccessfull()) {
			emit datasetLoaded(false, selectedSegmentEdit->text());
			this->close();
		}
		else {
			QErrorMessage *msg = new QErrorMessage(this);
			msg->showMessage("Folder is not a valid Dataset Folder.");
		}
	}
	else {
		QErrorMessage *msg = new QErrorMessage(this);
		msg->showMessage("Plase select a segment to load first.");
	}
}


void LoadDatasetWindow::updateCameraOrderList() {
	cameraOrderList->clear();
	m_cameraNames.clear();
	for (const auto& camera : m_datasetYaml["Cameras"]) {
		addItem(QString::fromStdString(camera.as<std::string>()));
		m_cameraNames.append(QString::fromStdString(camera.as<std::string>()));
	}
}

void LoadDatasetWindow::updateDatasetSegmentTree() {
	m_segments.clear();
	selectedSegmentEdit->setText("");
	loadDatasetButton->setEnabled(false);
	annotateSetupButton->setEnabled(false);
	loadDatasetButton->setToolTip("Load a dataset and select a segment to annotate first!");
	datasetSegmentsTree->blockSignals(true);
	datasetSegmentsTree->clear();
	m_datasetFolder = "";
	for (const auto& recording : m_datasetYaml["Recordings"]) {
		QTreeWidgetItem *recordingItem = new QTreeWidgetItem(static_cast<QTreeWidget *>(nullptr), QStringList(QString::fromStdString(recording.first.as<std::string>())));
		if (recording.second.size() != 0) {
			recordingItem->setFlags(recordingItem->flags() & ~Qt::ItemIsSelectable);
		}
		else {
			m_segments.append(QString::fromStdString(recording.first.as<std::string>()));
		}
		for (const auto& segment : recording.second) {
			QTreeWidgetItem *segmentItem = new QTreeWidgetItem(recordingItem, QStringList(QString::fromStdString(segment.as<std::string>())));
			recordingItem->addChild(segmentItem);
			m_segments.append(recordingItem->text(0) + "/" + QString::fromStdString(segment.as<std::string>()));

		}
		datasetSegmentsTree->addTopLevelItem(recordingItem);
		recordingItem->setExpanded(true);
	}
	datasetSegmentsTree->blockSignals(false);
}

void LoadDatasetWindow::datasetSegmentChangedSlot(QTreeWidgetItem *current, QTreeWidgetItem *previous) {
	loadDatasetButton->setEnabled(true);
	annotateSetupButton->setEnabled(true);
	loadDatasetButton->setToolTip("");
	if (current->flags() & Qt::ItemIsSelectable) {
		if (current->parent() != nullptr) {
			selectedSegmentEdit->setText(current->parent()->text(0) + "/" + current->text(0));
			QFileInfo a(datasetFileEdit->text());
			m_datasetFolder = a.absolutePath() + "/" + current->parent()->text(0) + "/" + current->text(0);
		}
		else {
				selectedSegmentEdit->setText(current->text(0));
				QFileInfo a(datasetFileEdit->text());
				m_datasetFolder = a.absolutePath() + "/" + current->text(0);
		}
	}

}

void LoadDatasetWindow::moveLabelUpSlot() {
	int row = cameraOrderList->currentRow();
	if (row == -1) return;
	QListWidgetItem *item = cameraOrderList->takeItem(row);
	QListWidgetItem *seperatorItem = cameraOrderList->takeItem(row);
	int newRow = std::max(row-2,0);
	cameraOrderList->insertItem(newRow,item);
	cameraOrderList->insertItem(newRow+1,seperatorItem);
	cameraOrderList->setCurrentRow(newRow);
	m_cameraNames.move(row/2, newRow/2);
	cameraOrderList->setCurrentRow(newRow);
}


void LoadDatasetWindow::moveLabelDownSlot() {
	int row = cameraOrderList->currentRow();
	if (row == -1) return;
	QListWidgetItem *item = cameraOrderList->takeItem(row);
	QListWidgetItem *seperatorItem = cameraOrderList->takeItem(row);
	int newRow = std::min(row+2,cameraOrderList->count());
	cameraOrderList->insertItem(newRow,item);
	cameraOrderList->insertItem(newRow+1,seperatorItem);
	cameraOrderList->setCurrentRow(newRow);
	m_cameraNames.move(row/2, newRow/2);
	cameraOrderList->setCurrentRow(newRow);
}


void LoadDatasetWindow::addItem(const QString &text) {
	QListWidgetItem * item = new QListWidgetItem();
	item->setSizeHint(QSize (100, 27));
	item->setText(text);
	item->setFlags(item->flags() ^ Qt::ItemIsSelectable);
	cameraOrderList->addItem(item);
	QListWidgetItem * seperatorItem = new QListWidgetItem();
	seperatorItem->setSizeHint(QSize (100, 3));
	seperatorItem->setFlags(Qt::NoItemFlags);
	seperatorItem->setBackground(QColor(46, 50, 60));
	cameraOrderList->addItem(seperatorItem);
}


void LoadDatasetWindow::currentItemChangedSlot(QListWidgetItem *current,
											   QListWidgetItem *previous) {
	if (current != nullptr)  {
		current->setBackground(QColor(100,164,32));
	}
	if (previous != nullptr) {
		previous->setBackground(QColor(34, 36, 40));
	}
}


void LoadDatasetWindow::annotateSetupButtonClicked() {
	QDialog *dialog = new QDialog(this);
	dialog->setWindowTitle("Configure Setup Keypoints");
	dialog->setMinimumSize(500,500);

	QGridLayout *dialoglayout = new QGridLayout(dialog);
	QPushButton *acceptButton = new QPushButton("Load Dataset", dialog);
	acceptButton->setMinimumSize(40,40);
	acceptButton->setIcon(QIcon::fromTheme("download"));
	connect(acceptButton, &QPushButton::clicked,
		dialog, &QDialog::accept);
	QPushButton *cancelButton = new QPushButton("Cancel", dialog);
	cancelButton->setMinimumSize(40,40);
	cancelButton->setIcon(QIcon::fromTheme("discard"));
	connect(cancelButton, &QPushButton::clicked,
			dialog, &QDialog::reject);
	QGroupBox *listBox = new QGroupBox("Keypoints");
	QGridLayout *listboxlayout = new QGridLayout(listBox);
	listboxlayout->setContentsMargins(0,0,0,0);
	ConfigurableItemList * keypointList = new ConfigurableItemList("Setup Keypoints", listBox);
	listboxlayout->addWidget(keypointList,0,0);
	dialoglayout->addWidget(listBox,0,0,1,2);
	dialoglayout->addWidget(cancelButton,1,0);
	dialoglayout->addWidget(acceptButton,1,1);

	int success = dialog->exec();
	if (success) {
		QList<QString> keypoints = keypointList->getItems();
		if (keypoints.size() == 0) {
			QErrorMessage errorMessage(dialog);
     		errorMessage.showMessage("Please specify at least one Keypoint!");
      		errorMessage.exec();
			return;
		}
		QDir baseDir = QFileInfo(datasetFileEdit->text()).absoluteDir();
		QString baseDirPath = baseDir.absolutePath();
		Dataset::dataset = new Dataset(m_datasetFolder, baseDirPath,
									   m_cameraNames, {}, m_segments, true, keypoints);
		if (Dataset::dataset->loadSuccessfull()) {
			emit datasetLoaded(true, selectedSegmentEdit->text());
			this->close();
		}
		else {
			QErrorMessage *msg = new QErrorMessage(this);
			msg->showMessage("Folder is not a valid Dataset Folder.");
		}
	}
}