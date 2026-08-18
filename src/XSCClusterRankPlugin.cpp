#include "XSCClusterRankPlugin.h"

#include "ChartWidget.h"

#include <DatasetsMimeData.h>
#include <util/Serialization.h>
#include <vector>
#include <random>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QMimeData>
#include <QDebug>
#include "ClusterData/ClusterData.h"
#include <XSCTreeData/XSCTreeData.h>
#include "PointData/PointData.h"
#include "lib/JSONnlohmann/json.hpp"
#include "lib/Clustering/fastcluster.h"
#include "lib/JSONnlohmann/json.hpp"
#include "lib/Clustering/fastcluster.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <stack>
#include "lib/Distance/annoylib.h"
#include "lib/Distance/kissrandom.h"

Q_PLUGIN_METADATA(IID "studio.manivault.XSCClusterRankPlugin")

using namespace mv;

struct PointClusterNames
{
    QString topHierarchy;
    QString middleHierarchy;
    QString bottomHierarchy;
};

XSCClusterRankPlugin::XSCClusterRankPlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _chartWidget(nullptr),
    _dropWidget(nullptr),
    _currentDataSet(nullptr),
    _settingsAction(*this)
{
}

void XSCClusterRankPlugin::init()
{
    auto& shortcuts = getShortcuts();
    shortcuts.add({ QKeySequence(Qt::Key_Escape), "Deselect", "Remove all selection" });
    shortcuts.add({ QKeySequence(Qt::SHIFT), "Multi-Selection" ,"Mouse left click"});
    
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "Hover", "Count details" });
    shortcuts.add({ QKeySequence(Qt::ALT), "Zoom in","Mouse left click on parent rectangle"});
    shortcuts.add({ QKeySequence(Qt::ALT), "Zoom out", "Mouse left click on parent rectangle" });
    //getLearningCenterAction().setPluginTitle("Cluster Hierarchy View");
    
    getWidget().setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

     
    
    // Create layout
    auto layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);

    // Create chart widget and set html contents of webpage 
    _chartWidget = new ChartWidget(this);
    _chartWidget->setPage(":XSCClusterRank_chart/icicle_chart.html", "qrc:/XSCClusterRank_chart/");

   
    auto mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto mainOptionsLayout = new QHBoxLayout();
    mainOptionsLayout->setSpacing(0);
    mainOptionsLayout->setContentsMargins(0, 0, 0, 0);
    auto extraOptionsGroup = new VerticalGroupAction(this, "Settings");

    extraOptionsGroup->setIcon(mv::util::StyledIcon("cog"));

    extraOptionsGroup->addAction(&_settingsAction.getSelectedClusterNames());
    extraOptionsGroup->addAction(&_settingsAction.getOptionSelectionAction());
    extraOptionsGroup->addAction(&_settingsAction.getCreatePointSelectTree());

    extraOptionsGroup->addAction(&_settingsAction.getFilterEditTreeDataset());
    extraOptionsGroup->addAction(&_settingsAction.getTopHierarchyRelativeClusterCountInclusion());
    extraOptionsGroup->addAction(&_settingsAction.getReferenceTreeDataset());
    extraOptionsGroup->addAction(&_settingsAction.getGeneNamesConnection());
    extraOptionsGroup->addAction(&_settingsAction.getGenerateTreeDataFilesPerClusterStart());


    auto subsamplingOptionsGroup = new VerticalGroupAction(this, "Subsampling");

    subsamplingOptionsGroup->setIcon(mv::util::StyledIcon("cog"));

    subsamplingOptionsGroup->addAction(&_settingsAction.getSubsampleByLevel());
    subsamplingOptionsGroup->addAction(&_settingsAction.getSubsamplePercentValue());
    subsamplingOptionsGroup->addAction(&_settingsAction.getSubsampleInplace());
    subsamplingOptionsGroup->addAction(&_settingsAction.getSubsampleDataStart());

    //auto mainOptionsGroup = new HorizontalGroupAction(this, "Trigger");
    auto mainOptionsGroup = new VerticalGroupAction(this, "Linking Options");
    mainOptionsGroup->setIcon(mv::util::StyledIcon("link"));

    auto dividerActionMain1 = new QAction("Dataset");
    dividerActionMain1->setEnabled(false);
    auto wrappedDividerActionMain1 = new mv::gui::WidgetAction(nullptr /* parent */, "datasetParameters");
    wrappedDividerActionMain1->setText(dividerActionMain1->text());
    wrappedDividerActionMain1->setEnabled(dividerActionMain1->isEnabled());
    mainOptionsGroup->addAction(wrappedDividerActionMain1);

    mainOptionsGroup->addAction(&_settingsAction.getHierarchyTopClusterDataset());
    mainOptionsGroup->addAction(&_settingsAction.getHierarchyMiddleClusterDataset());
    mainOptionsGroup->addAction(&_settingsAction.getHierarchyBottomClusterDataset());
    mainOptionsGroup->addAction(&_settingsAction.getSpeciesNamesDataset());
    mainOptionsGroup->addAction(&_settingsAction.getMainPointsDataset());
    mainOptionsGroup->addAction(&_settingsAction.getEmbeddingDataset());

    auto dividerActionMain2 = new QAction("Additional");
    dividerActionMain2->setEnabled(false);
    auto wrappedDividerActionMain2 = new mv::gui::WidgetAction(nullptr /* parent */, "additionalParameters");
    wrappedDividerActionMain2->setText(dividerActionMain2->text());
    wrappedDividerActionMain2->setEnabled(dividerActionMain2->isEnabled());
    mainOptionsGroup->addAction(wrappedDividerActionMain2);


    
    mainOptionsGroup->addAction(&_settingsAction.getStatusChangedAction());
    mainOptionsGroup->addAction(&_settingsAction.getClusterOrder());
    mainOptionsGroup->addAction(&_settingsAction.getRightClickedCluster());
    mainOptionsGroup->addAction(&_settingsAction.getClearRightClickedCluster());
    mainOptionsGroup->addAction(&_settingsAction.getTopSelectedHierarchyStatus());
    mainOptionsGroup->addAction(&_settingsAction.getRemoveTableSelection());

    //mainOptionsLayout->addWidget(subsamplingOptionsGroup->createCollapsedWidget(&getWidget()), 3);
    //mainOptionsLayout->addWidget(mainOptionsGroup->createCollapsedWidget(&getWidget()), 2);
    //mainOptionsLayout->addWidget(extraOptionsGroup->createCollapsedWidget(&getWidget()), 1);

    mainLayout->addLayout(mainOptionsLayout);
    mainLayout->addWidget(_chartWidget, 1);

    getWidget().setLayout(mainLayout);
   

    const auto mainPointsDatasetUpdate = [this]() -> void
        {
            _currentDataSet = _settingsAction.getMainPointsDataset().getCurrentDataset();
            if (_currentDataSet.isValid())
            {
                auto children = _currentDataSet->getChildren();
                for (auto child : children)
                {
                    child->setGroupIndex(1);
                }
                computeHierarchy();
            }
        };
    connect(&_settingsAction.getMainPointsDataset(), &DatasetPickerAction::currentIndexChanged, this, mainPointsDatasetUpdate);

    const auto embeddingDatasetUpdate = [this]() -> void
        {
            _embeddingDataset = _settingsAction.getEmbeddingDataset().getCurrentDataset();
        };
    connect(&_settingsAction.getEmbeddingDataset(), &DatasetPickerAction::currentIndexChanged, this, embeddingDatasetUpdate);



    const auto clearRightClickClusterUpdate = [this]() -> void
        {
            
            emit _chartWidget->getCommunicationObject().qt_js_removeRightClickIcon("Remove");
            //_settingsAction.getRightClickedCluster().setString("");
        };
    connect(&_settingsAction.getClearRightClickedCluster(), &TriggerAction::triggered, this, clearRightClickClusterUpdate);

    const auto statusComplete = [this]() -> void
        {
            if (_settingsAction.getStatusChangedAction().getString()=="C")
            {
                hierarchyStrokeforSelected();
            }
        };
    connect(&_settingsAction.getStatusChangedAction(), &StringAction::stringChanged, this, statusComplete);

    const auto clusterSelectionFromPopulationPyramidDatasetChange = [this]() -> void
        {
            //qDebug() << "Item selected in Population Pyramid";
            if (!_pauseSelectionEvent)
            {
                emit _chartWidget->getCommunicationObject().qt_js_removeClusterSelectionHighlight("Remove");
                _settingsAction.getStatusChangedAction().setString("M");
            }
            
        };
    connect(&_embeddingDataset, &mv::Dataset<Points>::dataSelectionChanged, this, clusterSelectionFromPopulationPyramidDatasetChange);

    const auto hierarchyTopClusterDatasetUpdate = [this]() -> void
        {
            auto hierarchyTopClusterDataset = _settingsAction.getHierarchyTopClusterDataset().getCurrentDataset();
            QStringList Options;
            if (hierarchyTopClusterDataset.isValid() && _currentDataSet.isValid())
            {
                if (hierarchyTopClusterDataset->getParent() == _currentDataSet)
                {
                    auto TopClusterFull= mv::data().getDataset<Clusters>(hierarchyTopClusterDataset.getDatasetId());
                    for (auto cluster : TopClusterFull->getClusters())
                    {
                        
                        auto clusterName = cluster.getName();
                        if (clusterName!="")
                        {
                            Options.append(clusterName);
                        }                        
                    }
                    if (!Options.isEmpty())
                    {
                        _settingsAction.getTopHierarchyRelativeClusterCountInclusion().setOptions(Options);
                        _settingsAction.getTopHierarchyRelativeClusterCountInclusion().setSelectedOptions(Options);
                    }
                    
                }
                computeHierarchy();
            }
            //&_settingsAction.getTopHierarchyRelativeClusterCountInclusion()
   
        };
    connect(&_settingsAction.getHierarchyTopClusterDataset(), &DatasetPickerAction::currentIndexChanged, this, hierarchyTopClusterDatasetUpdate);

    const auto hierarchyMiddleClusterDatasetUpdate = [this]() -> void
        {
            auto hierarchyMiddleClusterDataset = _settingsAction.getHierarchyMiddleClusterDataset().getCurrentDataset();
            if (hierarchyMiddleClusterDataset.isValid())
            {
                computeHierarchy();
            }
     };
     connect(&_settingsAction.getHierarchyMiddleClusterDataset(), &DatasetPickerAction::currentIndexChanged, this, hierarchyMiddleClusterDatasetUpdate);

     const auto hierarchyBottomClusterDatasetUpdate = [this]() -> void
         {
             auto hierarchyBottomClusterDataset = _settingsAction.getHierarchyBottomClusterDataset().getCurrentDataset();
             if (hierarchyBottomClusterDataset.isValid())
             {
                 computeHierarchy();
             }
         };
     connect(&_settingsAction.getHierarchyBottomClusterDataset(), &DatasetPickerAction::currentIndexChanged, this, hierarchyBottomClusterDatasetUpdate);

    const auto getTopHierarchyRelativeClusterCountInclusionUpdate = [this]() -> void {
        //auto hierarchyTopClusterDataset = _settingsAction.getHierarchyTopClusterDataset().getCurrentDataset();
        //auto options = _settingsAction.getTopHierarchyRelativeClusterCountInclusion().getSelectedOptions();
        //if (hierarchyTopClusterDataset.isValid() && _currentDataSet.isValid())
        //{
        //    if (hierarchyTopClusterDataset->getParent() == _currentDataSet)
        //    {
        //        auto TopClusterFull = mv::data().getDataset<Clusters>(hierarchyTopClusterDataset.getDatasetId());
        //        for (auto cluster : TopClusterFull->getClusters())
        //        {

        //            auto clusterName = cluster.getName();
        //            if (options.contains(clusterName))
        //            {
        //                int clusterIndices = cluster.getIndices().size();
        //                _relativeCellcount = _relativeCellcount + clusterIndices;
        //            }

        //        }

        //    }

        //}
        };
    connect(&_settingsAction.getTopHierarchyRelativeClusterCountInclusion(), &OptionsAction::selectedOptionsChanged, this, getTopHierarchyRelativeClusterCountInclusionUpdate);
    //connect(&mv::projects(), &AbstractProjectManager::projectOpened, this, hierarchyStrokeforSelected);
    const auto getCreatePointSelectTreeUpdate = [this]() -> void
        {
            
            auto clusterDataset = _settingsAction.getHierarchyBottomClusterDataset().getCurrentDataset();
            auto pointsDataset = _settingsAction.getMainPointsDataset().getCurrentDataset();
            //auto speciesDataset= _settingsAction.getSpeciesNamesDataset().getCurrentDataset();
            std::vector<std::uint32_t> selectedIndices= pointsDataset->getSelectionIndices();

            auto hierarchyTopClusterDataset = _settingsAction.getHierarchyTopClusterDataset().getCurrentDataset();


            auto options = _settingsAction.getTopHierarchyRelativeClusterCountInclusion().getSelectedOptions();


            int relativeCellcount = 0;
            if (clusterDataset.isValid() && pointsDataset.isValid() && hierarchyTopClusterDataset.isValid() && _currentDataSet.isValid() )
            {
                if (hierarchyTopClusterDataset->getParent() == _currentDataSet)
                {
                    auto TopClusterFull = mv::data().getDataset<Clusters>(hierarchyTopClusterDataset.getDatasetId());
                    for (auto cluster : TopClusterFull->getClusters())
                    {

                        auto clusterName = cluster.getName();
                        if (options.contains(clusterName))
                        {
                            int clusterIndices = cluster.getIndices().size();
                            relativeCellcount = relativeCellcount + clusterIndices;
                        }

                    }

                }

            }
           

            if (_settingsAction.getFilterEditTreeDataset().getCurrentDataset().isValid() && _settingsAction.getSpeciesNamesDataset().getCurrentDataset().isValid() && selectedIndices.size() > 0)
            {


                auto speciesDataset = mv::data().getDataset<Clusters>(_settingsAction.getSpeciesNamesDataset().getCurrentDataset().getDatasetId());
                std::map<QString, int> speciesSelectedIndicesCounter;
                auto speciesData = speciesDataset->getClusters();
                for (auto species : speciesData)
                {
                    auto speciesNameKey = species.getName();
                    int speciescellCountValue = 0;
                    auto indices = species.getIndices();
                    speciescellCountValue = std::count_if(selectedIndices.begin(), selectedIndices.end(), [&](const auto& element) {
                        return std::find(indices.begin(), indices.end(), element) != indices.end();
                        });
                    if (relativeCellcount != 0)
                    {
                        speciescellCountValue = (speciescellCountValue) / relativeCellcount;
                    }
                    speciesSelectedIndicesCounter.insert({ speciesNameKey, speciescellCountValue });
                }

                if (speciesSelectedIndicesCounter.size() > 0)
                {
                    // qDebug() << "XSCClusterRankPlugin::publishSelection: Send selection to core";
                    QJsonObject valueStringReference = createJsonTree(speciesSelectedIndicesCounter);
                    //print speciesSelectedIndicesCounter
                    /*
                    qDebug() << "*******************";
                    int i = 1;
                    for (auto& [key, value] : speciesSelectedIndicesCounter)
                    {
                        qDebug() << i<< key << " : " << value;
                        i++;
                    }
                    qDebug() << "*******************";
                    */

                    auto mainTreeDataset = mv::data().getDataset<XSCTree>(_settingsAction.getFilterEditTreeDataset().getCurrentDataset().getDatasetId());
                    mainTreeDataset->setTreeData(valueStringReference);

                    events().notifyDatasetDataChanged(mainTreeDataset);
                    _settingsAction.getGeneNamesConnection().setString("");
                }

            }
            else
            {
                qDebug() << "Datasets not valid";
            }

        };
    connect(&_settingsAction.getCreatePointSelectTree(), &TriggerAction::triggered, this, getCreatePointSelectTreeUpdate);


    // Instantiate new drop widget: See XSCClusterRank for details
    _dropWidget = new DropWidget(_chartWidget);
    _dropWidget->setDropIndicatorWidget(new DropWidget::DropIndicatorWidget(&getWidget(), "No data loaded", "Drag the Point data which has child cluster datasets to this view"));

    _dropWidget->initialize([this](const QMimeData* mimeData) -> DropWidget::DropRegions {

        // A drop widget can contain zero or more drop regions
        DropWidget::DropRegions dropRegions;

        const auto datasetsMimeData = dynamic_cast<const DatasetsMimeData*>(mimeData);


        if (datasetsMimeData == nullptr)
            return dropRegions;

        if (datasetsMimeData->getDatasets().count() > 1)
            return dropRegions;

        const auto dataset = datasetsMimeData->getDatasets().first();
        
        auto children = dataset->getChildren({ ClusterType }, false);
        if (children.size() < 2)
            return dropRegions;
        
        const auto datasetGuiName = dataset->text();
        const auto datasetId = dataset->getId();
        const auto dataType = dataset->getDataType();
        const auto dataTypes = DataTypes({ PointType });


        if (dataTypes.contains(dataType) ) {

            if (datasetId == getCurrentDataSetID()) {
                dropRegions << new DropWidget::DropRegion(this, "Warning", "Data already loaded", "exclamation-circle", false);
            }
            else {
                auto candidateDataset = mv::data().getDataset<Points>(datasetId);

                dropRegions << new DropWidget::DropRegion(this, "Points", QString("Visualize %1 as cluster rank").arg(datasetGuiName), "map-marker-alt", true, [this, candidateDataset]() {
                    loadData({ candidateDataset });
                   
                    });

            }
        }
        else {
            dropRegions << new DropWidget::DropRegion(this, "Incompatible data", "This type of data is not supported", "exclamation-circle", false);
        }

        return dropRegions;
        });
    computeHierarchy();
    // update data when data set changed
    connect(&_currentDataSet, &Dataset<Points>::dataChanged, this, &XSCClusterRankPlugin::computeHierarchy);

    // Update the selection (coming from PCP) in core
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passSelectionToCore, this, &XSCClusterRankPlugin::publishSelection);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passClusterOrderToCore, this, &XSCClusterRankPlugin::publishClusterOrder);
    connect(&_chartWidget->getCommunicationObject(), &ChartCommObject::passRightClickToCore, this, &XSCClusterRankPlugin::publishRightClickCluster);
    _settingsAction.getStatusChangedAction().setString("M");

}

void XSCClusterRankPlugin::loadData(const mv::Datasets& datasets)
{
    // Exit if there is nothing to load
    if (datasets.isEmpty())
        return;

    //qDebug() << "XSCClusterRankPlugin::loadData: Load data set from ManiVault core";


    _currentDataSet = datasets.first();
    events().notifyDatasetDataChanged(_currentDataSet);
}
QVariantMap createNode(const QString& name, const QString& color, const QVariantList& children = QVariantList())
{
    QVariantMap node;
    node.insert("name", name);
    node.insert("color", color);
    if (!children.isEmpty())
        node.insert("children", children);
    return node;
}

void XSCClusterRankPlugin::hierarchyStrokeforSelected()
{
    {
        auto clusterDataset = _settingsAction.getHierarchyBottomClusterDataset().getCurrentDataset();
        auto pointsDataset = _settingsAction.getMainPointsDataset().getCurrentDataset();
        auto embeddingDataset = _settingsAction.getEmbeddingDataset().getCurrentDataset();
        QStringList partialClusterNames;
        QStringList fullClusterNames;

        if (clusterDataset.isValid() && embeddingDataset.isValid() && pointsDataset.isValid()) {
            std::vector<std::uint32_t> selectedIndices = pointsDataset->getSelectionIndices();
            if (selectedIndices.size() > 0) { // Get the selection indices from the points dataset

                // Sort the selected indices
                std::sort(selectedIndices.begin(), selectedIndices.end());

                // Get the clusters dataset
                auto clustersDataset = mv::data().getDataset<Clusters>(clusterDataset->getId());
                auto clusters = clustersDataset->getClusters();

                for (auto cluster : clusters) {
                    auto clusterName = cluster.getName();
                    auto clusterIndices = cluster.getIndices();
                    // Sort the cluster indices
                    std::sort(clusterIndices.begin(), clusterIndices.end());

                    // Debug: Print selectedIndices and clusterIndices
                   // qDebug() << "Selected Indices: " << selectedIndices;
                    //qDebug() << "Cluster Indices for " << clusterName << ": " << clusterIndices;

                    // Check if all indices of clusterIndices are present in selectedIndices
                    bool allPresent = std::includes(selectedIndices.begin(), selectedIndices.end(), clusterIndices.begin(), clusterIndices.end());

                    // Check if at least one index of clusterIndices is present in selectedIndices
                    bool atLeastOnePresent = std::any_of(clusterIndices.begin(), clusterIndices.end(), [&selectedIndices](uint32_t index) {
                        return std::binary_search(selectedIndices.begin(), selectedIndices.end(), index);
                    });

                    // If all indices are present
                    if (allPresent) {
                        fullClusterNames.append(clusterName);
                    }
                    // Else if at least one index is present
                    else if (atLeastOnePresent) {
                        partialClusterNames.append(clusterName);
                    }
                }
                //qDebug() << "Full Cluster Names: " << fullClusterNames;
                //qDebug() << "Partial Cluster Names: " << partialClusterNames;
                QString fullList="";
                QString partialList = "";
                if (fullClusterNames.size() > 0)
                {
                    fullList = fullClusterNames.join("@$%%$@");
                }
                if (partialClusterNames.size() > 0)
                {
                    partialList = partialClusterNames.join("@$%%$@");
                }
                QString totalList = "";
                if (fullClusterNames.size() > 0 && partialClusterNames.size() > 0)
                {
                    totalList = fullList + "##%%$joinHere$%%##" + partialList;
                }
                else if (fullClusterNames.size() > 0)
                {
                    totalList = fullList + "##%%$joinHere$%%##";
                }
                else if (partialClusterNames.size() > 0)
                {
                    totalList = "##%%$joinHere$%%##" + partialList;
                }
                else
                {
                    totalList = "##%%$joinHere$%%##";
                }
                
                emit _chartWidget->getCommunicationObject().qt_js_setClusterMarks(totalList);

            }
            else
            {
                //qDebug() << "No indices present";
            }
        }

    }
}

QVariantMap createLeaf(const QString& name, const QString& color, int value)
{
    QVariantMap leaf;
    leaf.insert("name", name);
    leaf.insert("color", color);
    leaf.insert("value", value);
    return leaf;
}

QVariantList buildChartData(const std::map<std::pair<QString, QString>, std::map<std::pair<QString, QString>, std::map<std::pair<QString, QString>, int>>>& fullhierarchyMap) {
    QVariantList chartData;
    QVariantList topLevelNodes;

    for (const auto& topPair : fullhierarchyMap) {
        QVariantList middleLevelNodes;

        for (const auto& middlePair : topPair.second) {
            QVariantList bottomLevelNodes;

            for (const auto& bottomPair : middlePair.second) {
                bottomLevelNodes.append(createLeaf(bottomPair.first.first, bottomPair.first.second, bottomPair.second));
            }
            middleLevelNodes.append(createNode(middlePair.first.first, middlePair.first.second, bottomLevelNodes));
        }
        topLevelNodes.append(createNode(topPair.first.first, topPair.first.second, middleLevelNodes));
    }

    chartData.append(createNode("All", "white", topLevelNodes));
    return chartData;
}

void XSCClusterRankPlugin::computeHierarchy()
{
    auto topHierarchyDataset = _settingsAction.getHierarchyTopClusterDataset().getCurrentDataset();
    auto middleHierarchyDataset = _settingsAction.getHierarchyMiddleClusterDataset().getCurrentDataset();
    auto bottomHierarchyDataset = _settingsAction.getHierarchyBottomClusterDataset().getCurrentDataset();
    auto mainPointsDataset = _settingsAction.getMainPointsDataset().getCurrentDataset();

    if (topHierarchyDataset.isValid() && middleHierarchyDataset.isValid() && bottomHierarchyDataset.isValid() && mainPointsDataset.isValid())
    {
        auto topHierarchyClusters = mv::data().getDataset<Clusters>(topHierarchyDataset.getDatasetId());
        auto middleHierarchyClusters = mv::data().getDataset<Clusters>(middleHierarchyDataset.getDatasetId());
        auto bottomHierarchyClusters = mv::data().getDataset<Clusters>(bottomHierarchyDataset.getDatasetId());
        auto mainPoints = mv::data().getDataset<Points>(mainPointsDataset.getDatasetId());

        bool valid = topHierarchyClusters->getParent() == mainPoints && middleHierarchyClusters->getParent() == mainPoints && bottomHierarchyClusters->getParent() == mainPoints;

        if (valid)
        {
            std::map<int, PointClusterNames> pointClusterNamesMap;
            std::map<QString, std::pair<QColor, int>> topHierarchyMap;
            std::map<QString, std::pair<QColor, int>> middleHierarchyMap;
            std::map<QString, std::pair<QColor, int>> bottomHierarchyMap;
            std::map<QString, std::pair<QString, QString>> hierarchyMap;

            auto updateClusterNamesAndHierarchyMap = [&](const auto& clusters, auto updateFunc, auto& hierarchyMap) {
                for (const auto& cluster : clusters->getClusters())
                {
                    auto clusterName = cluster.getName();
                    auto clusterIndices = cluster.getIndices();
                    for (auto index : clusterIndices)
                    {
                        updateFunc(pointClusterNamesMap[index], clusterName);
                    }
                    auto clusterColor = cluster.getColor();
                    if (clusterColor.isValid()) {
                        hierarchyMap.emplace(clusterName, std::make_pair(clusterColor.name(), clusterIndices.size()));
                    }
                    else {
                        hierarchyMap.emplace(clusterName, std::make_pair("grey", clusterIndices.size()));
                    }
                }
                };

            updateClusterNamesAndHierarchyMap(topHierarchyClusters, [](PointClusterNames& names, const QString& name) { names.topHierarchy = name; }, topHierarchyMap);
            updateClusterNamesAndHierarchyMap(middleHierarchyClusters, [](PointClusterNames& names, const QString& name) { names.middleHierarchy = name; }, middleHierarchyMap);
            updateClusterNamesAndHierarchyMap(bottomHierarchyClusters, [](PointClusterNames& names, const QString& name) { names.bottomHierarchy = name; }, bottomHierarchyMap);

            for (const auto& pair : pointClusterNamesMap) {
                auto bottomString = pair.second.bottomHierarchy;
                auto middleString = pair.second.middleHierarchy;
                auto topString = pair.second.topHierarchy;

                hierarchyMap[bottomString].first = middleString;
                hierarchyMap[bottomString].second = topString;
            }

            std::map<std::pair<QString, QString>, std::map<std::pair<QString, QString>, std::map<std::pair<QString, QString>, int>>> finalHierarchyMap;

            for (const auto& [bottom, middleTop] : hierarchyMap)
            {
                auto middle = middleTop.first;
                auto top = middleTop.second;
                std::pair<QString, QString> topPair = std::make_pair(top, topHierarchyMap[top].first.name());
                std::pair<QString, QString> middlePair = std::make_pair(middle, middleHierarchyMap[middle].first.name());
                std::pair<QString, QString> bottomPair = std::make_pair(bottom, bottomHierarchyMap[bottom].first.name());

                finalHierarchyMap[topPair][middlePair][bottomPair] = bottomHierarchyMap[bottom].second;
            }

            _dataForChart = buildChartData(finalHierarchyMap);

            convertDataAndUpdateChart();
        }
        else
        {
            qDebug() << "top, middle and bottom hierarchy datasets are not children of main points dataset";
        }
    }
    else
    {
        //qDebug() << "XSCClusterRankPlugin::computeHierarchy: Not all datasets are valid";
    }

}

void XSCClusterRankPlugin::convertDataAndUpdateChart()
{

    _dropWidget->setShowDropIndicator(false);

    //QVariantList dataForChart1 = {
    //createNode("All", "white", {
    //    createNode("Non-Neuronal", "#7fffff", {
    //        createNode("Micro-PVM", "#94af97", {
    //            createLeaf("Microglia/PVM", "#94af97", 20566)
    //        }),
    //        createNode("VLMC", "#697255", {
    //            createLeaf("No Agreement", "#C0C0C0", 133072),
    //            createLeaf("VLMC", "#697255", 7968),
    //            createLeaf("Unknown", "#C0C0C0", 50)
    //        }),
    //        createNode("Endo", "#8d6c62", {
    //            createLeaf("Endo", "#8d6c62", 9123)
    //        }),
    //        createNode("Oligo", "#53776c", {
    //            createLeaf("Oligo_1", "#88a19a", 63386),
    //            createLeaf("Oligo_2", "#3a544c", 90484)
    //        }),
    //        createNode("Astro", "#665c47", {
    //            createLeaf("Astro_1", "#958f80", 5583),
    //            createLeaf("Astro_2", "#484132", 60795)
    //        }),
    //        createNode("OPC", "#374a45", {
    //            createLeaf("OPC", "#374a45", 28445)
    //        }),
    //    }),
    //    createNode("GABAergic", "#ff7f7f", {
    //        createNode("Sst Chodl", "#b1b10c", {
    //            createLeaf("Sst Chodl", "#b1b10c", 5133)
    //        }),
    //        createNode("Sst", "#ff9900", {
    //            createLeaf("Sst_1", "#ffb84f", 11487),
    //            createLeaf("Sst_2", "#ffb342", 12334),
    //            createLeaf("Sst_3", "#ffb03c", 74776),
    //            createLeaf("Sst_4", "#ffae36", 15488),
    //            createLeaf("Sst_5", "#ffac30", 6547),
    //            createLeaf("Sst_6", "#ffa92a", 9323),
    //            createLeaf("Sst_7", "#ffa724", 1607)
    //        }),
    //        createNode("Pvalb", "#d93137", {
    //            createLeaf("Pvalb_1", "#e47175", 143140),
    //            createLeaf("Pvalb_2", "#e2686c", 10764)
    //        }),
    //        createNode("Chandelier", "#f641a8", {
    //            createLeaf("Chandelier", "#f641a8", 16199)
    //        }),
    //        createNode("Vip", "#a45fbf", {
    //            createLeaf("Vip_1", "#c091d3", 15946),
    //            createLeaf("Vip_2", "#bd8cd1", 42074),
    //            createLeaf("Vip_3", "#ba87cf", 26303),
    //            createLeaf("Vip_4", "#b883cd", 10681)

    //        }),
    //        createNode("Sncg", "#df70ff", {
    //            createLeaf("Sncg_1", "#e99cff", 7342),
    //            createLeaf("Sncg_2", "#e691ff", 4291),
    //            createLeaf("Sncg_3", "#e386ff", 4459),
    //            createLeaf("Sncg_4", "#e17bff", 13487),
    //        }),
    //        createNode("Lamp5", "#da808c", {
    //            createLeaf("Lamp5_1", "#e5a7b0", 4557),
    //            createLeaf("Lamp5_2", "#e097a1", 40821),
    //            createLeaf("Lamp5_3", "#dc8793", 22716),
    //            createLeaf("Lamp5_4", "#cd7883", 6217),
    //            createLeaf("Lamp5_5", "#b36972", 16354)
    //        })
    //    }),
    //    createNode("Glutamatergic", "#bfff7f", {
    //        createNode("L6 IT", "#a19922", {
    //            createLeaf("L6 IT_1", "#beb867", 24546),
    //            createLeaf("L6 IT_2", "#716c18", 136534),
    //            createLeaf("L6 IT_3", "#ffd700", 17004)
    //        }),
    //        createNode("L2/3 IT", "#b1ec30", {
    //            createLeaf("L2/3 IT", "#b1ec30", 486221)
    //        }),
    //        createNode("L5 ET", "#0d5b78", {
    //            createLeaf("L5 ET_1", "#588ea2", 52299),
    //            createLeaf("L5 ET_2", "#0d5b78", 313)

    //        }),
    //        createNode("L5 IT", "#50b2ad", {
    //            createLeaf("L5 IT_1", "#86cac6", 316754),
    //            createLeaf("L5 IT_2", "#67bcb7", 191918),
    //            createLeaf("L5 IT_3", "#beb867", 18024)
    //        }),
    //        createNode("L6b", "#7044aa", {
    //            createLeaf("L6b", "#7044aa", 61326)
    //        }),
    //        createNode("L6 CT", "#2d8cb8", {
    //            createLeaf("L6 CT_1", "#6eb0ce", 77283),
    //            createLeaf("L6 CT_2", "#4d9ec3", 119532)
    //        }),
    //        createNode("L6 IT Car3", "#5100ff", {
    //            createLeaf("MC", "#5100ff", 7074)
    //        }),
    //        createNode("L5/6 NP", "#3e9e64", {
    //            createLeaf("L5/6 NP", "#3e9e64", 47918)
    //        })
    //    })
    //})
    //};
    //if (dataForChart1 == _dataForChart)
    //{
    //    qDebug() << "Data is the same";
    //}
    //else
    //{
    //    qDebug() << "Data is different";
    //}
    if (!_dataForChart.isEmpty())
    {
        QJsonDocument doc = QJsonDocument::fromVariant(QVariant::fromValue(_dataForChart));
        {
            QString jsonString = doc.toJson(QJsonDocument::Compact);
            
            if (!jsonString.isEmpty() && jsonString != "")
            {

                emit _chartWidget->getCommunicationObject().qt_js_setDataAndPlotInJS(jsonString);
                hierarchyStrokeforSelected();
            }
            else
            {
                qDebug() << "Json string is empty";
            }
        }

    }
    else
    {
        qDebug() << "Data is empty";
    }

    
}

QJsonObject XSCClusterRankPlugin::createJsonTree(std::map<QString, int> speciesSelectedIndicesCounter)
{
    QJsonObject valueStringReference;

    {
    
        std::vector<float> numbers;
        std::vector<QString> leafnames;

        const std::unordered_map<std::string, int> clusteringTypeMap = {
    {"Complete", HCLUST_METHOD_COMPLETE},
    {"Average", HCLUST_METHOD_AVERAGE},
    {"Median", HCLUST_METHOD_MEDIAN}
        };
        std::string clusteringTypecurrentText = "Single";  //"Single","Complete", "Average","Median"
        int opt_method = clusteringTypeMap.count(clusteringTypecurrentText) ? clusteringTypeMap.at(clusteringTypecurrentText) : HCLUST_METHOD_SINGLE;

        auto numOfLeaves = speciesSelectedIndicesCounter.size();
        double* distmat = new double[(numOfLeaves * (numOfLeaves - 1)) / 2];

        //iterate std::map<QString, int> speciesSelectedIndicesCounter and populate numbers
        for (auto& [key, value] : speciesSelectedIndicesCounter)
        {
            numbers.push_back(value);
            leafnames.push_back(key);
        }





        distmat = _settingsAction.condensedDistanceMatrix(numbers);

        int* merge = new int[2 * (numOfLeaves - 1)];
        double* height = new double[numOfLeaves - 1];
        hclust_fast(numOfLeaves, distmat, opt_method, merge, height);




        std::string newick = _settingsAction.mergeToNewick(merge, numOfLeaves);
        int totalChars = newick.length();
        //append a ";" to the end of the newick string
        //qDebug()<<"Newick format: " << QString::fromStdString(newick);
        newick += ';';
        //std::cout << "Newick format: " << newick << std::endl;
        QString formattedtree = _settingsAction.createJsonTreeFromNewick(QString::fromStdString(newick),leafnames);
        valueStringReference = QJsonDocument::fromJson(formattedtree.toUtf8()).object();
    delete[] distmat;
    delete[] merge;
    delete[] height;
}
    

    return valueStringReference;
}

void XSCClusterRankPlugin::publishClusterOrder(const QString& orderedClusters)
{
    _settingsAction.getClusterOrder().setString(orderedClusters);
    //qDebug() << "XSCClusterRankPlugin::publishClusterOrder: Send cluster order to core"<< orderedClusters;
}

void XSCClusterRankPlugin::publishRightClickCluster(const QString& orderedClusters)
{
    //aplit the const clusterNameAndLevel = `${clusterName} @%$,$%@ ${clusterLevel}`;
    _settingsAction.getRightClickedCluster().setString("");
    _settingsAction.getRightClickedCluster().setString(orderedClusters);
    /*
    qDebug() << "Cluster Name and Level: " << orderedClusters;
    QStringList clusterNameAndLevel = orderedClusters.split(" @%$,$%@ ");
    if (clusterNameAndLevel.size() == 2)
    {
        QString clusterName = clusterNameAndLevel.at(0);
        QString clusterLevelTemp = clusterNameAndLevel.at(1);
        if (clusterName == "" || clusterLevelTemp == "")
        {
            return;
        }
        QString clusterLevel;
        if (clusterLevelTemp == "1")
        {
            clusterLevel = "Top";
        }
        else if (clusterLevelTemp == "2")
        {
            clusterLevel = "Middle";
        }
        else if (clusterLevelTemp == "3")
        {
            clusterLevel = "Bottom";
        }
        else
        {
            clusterLevel = "Unknown";
        }
        qDebug() << "Cluster Name: " << clusterName << " Cluster Level: " << clusterLevel;
    }
    */
}

void XSCClusterRankPlugin::publishSelection(const std::vector<QString>& selectedIDs,const QString& topHselectedIDs)
{
    auto clusterDataset= _settingsAction.getHierarchyBottomClusterDataset().getCurrentDataset();
    auto pointsDataset= _settingsAction.getMainPointsDataset().getCurrentDataset();
    //auto speciesDataset= _settingsAction.getSpeciesNamesDataset().getCurrentDataset();
    std::vector<std::uint32_t> selectedIndices;

    if (clusterDataset.isValid() && pointsDataset.isValid())
    {
        bool isValid = false;
        isValid = clusterDataset->getParent().getDatasetId() == pointsDataset->getId();//&& speciesDataset->getParent().getDatasetId() == pointsDataset->getId();


        QList<QString> list;
        for (const auto& id : selectedIDs) {
            list.append(id);
        }

        QVariant variant = QVariant::fromValue(list);
        _settingsAction.getSelectedClusterNames().setVariant(variant);

        if (isValid)
        {
           
            if (!selectedIDs.empty())
            {

                auto rawData = mv::data().getDataset < Clusters>(clusterDataset.getDatasetId());
                auto clusters = rawData->getClusters();
                if (clusters.size() > 0)
                {
                    for (auto cluster : clusters)
                    {
                        auto clusterName = cluster.getName();
                        //if selectedIDs contains cluster.getName()
                        if (std::find(selectedIDs.begin(), selectedIDs.end(), clusterName) != selectedIDs.end())
                        {
                            auto clusterIndices = cluster.getIndices();
                            selectedIndices.insert(selectedIndices.end(), clusterIndices.begin(), clusterIndices.end());
                        }




                    }
                }

            }
            _pauseSelectionEvent = true;
            //qDebug() << topHselectedIDs;
            _settingsAction.getTopSelectedHierarchyStatus().setString(topHselectedIDs);
            //need to trigger remove table selection event first
            _settingsAction.getRemoveTableSelection().trigger();
            pointsDataset->setSelectionIndices(selectedIndices);
            mv::events().notifyDatasetDataSelectionChanged(pointsDataset);
            _settingsAction.getStatusChangedAction().setString("M");
            _pauseSelectionEvent = false;
        }
        else
        {
            qDebug() << "Datasets not valid";
        }

    }

    else
    {
        qDebug() << "Datasets not valid";
    }
    

    
    if (_settingsAction.getFilterEditTreeDataset().getCurrentDataset().isValid() && _settingsAction.getSpeciesNamesDataset().getCurrentDataset().isValid() && selectedIndices.size()>0 )
    {
        
        
        auto speciesDataset = mv::data().getDataset<Clusters>(_settingsAction.getSpeciesNamesDataset().getCurrentDataset().getDatasetId());
        std::map<QString, int> speciesSelectedIndicesCounter;
        auto speciesData = speciesDataset->getClusters();
        for (auto species : speciesData)
        {
            auto speciesNameKey= species.getName();
            int speciescellCountValue = 0;
            auto indices= species.getIndices();
            speciescellCountValue = std::count_if(selectedIndices.begin(), selectedIndices.end(), [&](const auto& element) {
                return std::find(indices.begin(), indices.end(), element) != indices.end();
                });
            
            speciesSelectedIndicesCounter.insert({ speciesNameKey, speciescellCountValue });
        }

        if ( speciesSelectedIndicesCounter.size()>0)
        {
           // qDebug() << "XSCClusterRankPlugin::publishSelection: Send selection to core";
            QJsonObject valueStringReference = createJsonTree(speciesSelectedIndicesCounter);
            //print speciesSelectedIndicesCounter
            /*
            qDebug() << "*******************";
            int i = 1;
            for (auto& [key, value] : speciesSelectedIndicesCounter)
            {
                qDebug() << i<< key << " : " << value;
                i++;
            }
            qDebug() << "*******************";
            */

            auto mainTreeDataset = mv::data().getDataset<XSCTree>(_settingsAction.getFilterEditTreeDataset().getCurrentDataset().getDatasetId());
            mainTreeDataset->setTreeData(valueStringReference);

            events().notifyDatasetDataChanged(mainTreeDataset);
            _settingsAction.getGeneNamesConnection().setString("");
        }

    }



    //// ask core for the selection set for the current data set
    //auto selectionSet = _currentDataSet->getSelection<Points>();
    //auto& selectionIndices = selectionSet->indices;

    //// clear the selection and add the new points
    //selectionIndices.clear();
    //selectionIndices.reserve(_currentDataSet->getNumPoints());
    //for (const auto id : selectedIDs) {
    //    selectionIndices.push_back(id);
    //}

    //// notify core about the selection change
    //if (_currentDataSet->isDerivedData())
    //    events().notifyDatasetDataSelectionChanged(_currentDataSet->getSourceDataset<DatasetImpl>());
    //else
    //    events().notifyDatasetDataSelectionChanged(_currentDataSet);
}
/*
namespace  local
{
    void get_recursive_cluster_tree(QStandardItem* item, Dataset<DatasetImpl> currentDataset, const QVector<QString>& hierarchy, qsizetype h, bool firstTime, bool intersection = true, const std::vector<uint32_t>& indices = {})
    {
        if (h >= hierarchy.size())
            return;

        auto childDatasets = currentDataset->getChildren({ ClusterType });
        for (qsizetype c = 0; c < childDatasets.size(); ++c)
        {
            if (childDatasets[c]->getGuiName() == hierarchy[h])
            {
                mv::Dataset<Clusters> clusterData = childDatasets[c];
                auto clusters = clusterData->getClusters();


                if (intersection && !firstTime)
                {
                    QSet<QString> clusterNames;
                    for (auto cluster : clusters)
                    {
                        QString name = cluster.getName();
                        clusterNames.insert(name);
                    }
                    for (qsizetype row = item->rowCount() - 1; row >= 0; --row)
                    {
                        QStandardItem* child = item->child(row, 0);
                        if (!clusterNames.contains(child->text()))
                        {
                            item->removeRow(row);
                        }
                    }
                }

                for (auto cluster : clusters)
                {
                    QString name = cluster.getName();
                    QStandardItem* correspondingItem = nullptr;

                    if (!firstTime)
                    {
                        for (qsizetype row = 0; row < item->rowCount(); ++row)
                        {
                            QStandardItem* child = item->child(row, 0);
                            if (child->text() == name)
                            {
                                correspondingItem = child;
                                break;
                            }
                        }
                        if (intersection && (correspondingItem == nullptr))
                            continue;;
                    }



                    {
                        std::vector<uint32_t> clusterIndices = cluster.getIndices();
                        std::sort(clusterIndices.begin(), clusterIndices.end());
                        std::vector<uint32_t> intersectingIndices;
                        if (h == 0)
                            intersectingIndices = clusterIndices;
                        else
                        {
                            std::set_intersection(indices.cbegin(), indices.cend(), clusterIndices.cbegin(), clusterIndices.cend(), std::back_inserter(intersectingIndices));
                        }
                        if (!intersectingIndices.empty())
                        {
                            if (correspondingItem == nullptr)
                            {
                                QPixmap pixmap(16, 16);
                                pixmap.fill(cluster.getColor());
                                correspondingItem = new QStandardItem(pixmap, name);
                                correspondingItem->setData(h, Qt::UserRole);

                                item->appendRow(correspondingItem);
                            }
                            get_recursive_cluster_tree(correspondingItem, currentDataset, hierarchy, h + 1, firstTime, intersection, intersectingIndices);
                        }

                    }
                }
                break;
            }
        }
    }
}


void XSCClusterRankPlugin::createHierarchy(qsizetype index, const Dataset<DatasetImpl>& dataset)
{
    
    const qsizetype NrOfDatasets = _selectedDatasetsAction.size();


    QVector<QString> hierarchy = { "class", "subclass","cross_species_cluster" };

    for (qsizetype i = 0; i < _selectedDatasetsAction.size(); ++i)
    {
        if (!_selectedDatasetsAction.getDataset(i).isValid())
            return;
    }
    _model.clear();
    //  _model.invisibleRootItem()->setData("Hierarchy", Qt::DisplayRole);

    QStringList finalLevelItems;
    for (qsizetype i = 0; i < _selectedDatasetsAction.size(); ++i)
    {

        Dataset<DatasetImpl> currentDataset = _selectedDatasetsAction.getDataset(i).get();


        auto childDatasets = currentDataset->getChildren({ ClusterType });
        for (qsizetype c = 0; c < childDatasets.size(); ++c)
        {
            if (childDatasets[c]->getGuiName() == hierarchy.last())
            {
                Dataset<Clusters> clusterData = childDatasets[c];
                auto clusters = clusterData->getClusters();

                for (auto cluster : clusters)
                {
                    QString name = cluster.getName();
                    finalLevelItems.append(name);
                }
            }
        }


        local::get_recursive_cluster_tree(_model.invisibleRootItem(), currentDataset, hierarchy, 0, (i == 0), true);

    }


    _selectedOptionsAction.setOptions(finalLevelItems);
    if (finalLevelItems.count())
        _selectedOptionsAction.selectOption(finalLevelItems.first());

   
}
 */



QString XSCClusterRankPlugin::getCurrentDataSetID() const
{
    if (_currentDataSet.isValid())
        return _currentDataSet->getId();
    else
        return QString{};
}
void XSCClusterRankPlugin::fromVariantMap(const QVariantMap& variantMap)
{
    ViewPlugin::fromVariantMap(variantMap);

    mv::util::variantMapMustContain(variantMap, "CSCCR:Cross-Species Comparison Cluster Rank Settings");
    _settingsAction.fromVariantMap(variantMap["CSCCR:Cross-Species Comparison Cluster Rank Settings"].toMap());


}

QVariantMap XSCClusterRankPlugin::toVariantMap() const
{
    QVariantMap variantMap = ViewPlugin::toVariantMap();

    _settingsAction.insertIntoVariantMap(variantMap);

    return variantMap;
}

// =============================================================================
// Plugin Factory 
// =============================================================================

XSCClusterRankPluginFactory::XSCClusterRankPluginFactory()
{
    setIconByName("sitemap");
}

ViewPlugin* XSCClusterRankPluginFactory::produce()
{
    return new XSCClusterRankPlugin(this);
}

mv::DataTypes XSCClusterRankPluginFactory::supportedDataTypes() const
{
    // This XSCClusterRank analysis plugin is compatible with points datasets
    DataTypes supportedTypes;
    supportedTypes.append(PointType);
    return supportedTypes;
}

mv::gui::PluginTriggerActions XSCClusterRankPluginFactory::getPluginTriggerActions(const mv::Datasets& datasets) const
{
    PluginTriggerActions pluginTriggerActions;
    bool isValidDataset = false;
    const auto getPluginInstance = [this]() -> XSCClusterRankPlugin* {
        return dynamic_cast<XSCClusterRankPlugin*>(plugins().requestViewPlugin(getKind()));
    };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets == 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType)) {

        auto dataset= datasets.first();
        auto children = dataset->getChildren({ClusterType},false);
        if (children.size()>1)
        {
            isValidDataset = true;
        }

        if (isValidDataset)
        {
            auto pluginTriggerAction = new PluginTriggerAction(const_cast<XSCClusterRankPluginFactory*>(this), this, "Cross-Species Comparison Cluster Rank View", "Cross-Species Comparison Cluster Rank visualization", StyledIcon("sitemap"), [this, getPluginInstance, datasets](PluginTriggerAction& pluginTriggerAction) -> void {
                for (auto dataset : datasets)
                    getPluginInstance()->loadData(Datasets({ dataset }));

                });

            pluginTriggerActions << pluginTriggerAction;
        }



    }

    return pluginTriggerActions;
}
