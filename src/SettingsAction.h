#pragma once
#include <actions/WidgetAction.h>
#include <actions/IntegralAction.h>
#include <actions/DecimalAction.h>
#include <actions/OptionAction.h>
#include <actions/OptionsAction.h>
#include <actions/ToggleAction.h>
#include "actions/DatasetPickerAction.h"
#include "PointData/PointData.h"
#include "ClusterData/ClusterData.h"
#include "event/EventListener.h"
#include "actions/Actions.h"
#include "Plugin.h"
#include "DataHierarchyItem.h"
#include "Set.h"
#include <AnalysisPlugin.h>
#include <memory>
#include <algorithm>    
#include <QDebug>
#include <QLabel>
#include <QComboBox>
#include <QMessageBox>
#include <QGroupBox>
#include <QPushButton>
#include <QGridLayout>
#include <QFormLayout>
#include <QString>
#include <string>
#include <event/Event.h>
#include <QDebug>
#include <QLabel>
#include <string>
#include <QMessageBox>
#include "actions/VariantAction.h"
#include "actions/GroupAction.h"
using namespace mv::gui;
class QMenu;
class XSCClusterRankPlugin;

// Define the struct for PointData
struct PointDataStructChild {

    std::vector<float> pointVector;
    int numPoints;
    int numDimensions;
    std::vector<QString> dimensionNames;
};
struct PointDataStructParent {

    std::vector<float> pointVector; ////change to float for ci build __bfloat16 to float
    int numPoints;
    int numDimensions;
    std::vector<QString> dimensionNames;
};

class FetchMetaData;
namespace mv
{
    class CoreInterface;
}

    class SettingsAction : public WidgetAction
    {
    public:
        class OptionSelectionAction : public GroupAction
        {
        protected:
            class Widget : public mv::gui::WidgetActionWidget {
            public:
                Widget(QWidget* parent, OptionSelectionAction* optionSelectionAction);
            };

            QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override {
                return new OptionSelectionAction::Widget(parent, this);
            };

        public:
            OptionSelectionAction(SettingsAction& SettingsAction);

        protected:
            SettingsAction& _settingsAction;
        };



    protected:

        class Widget : public mv::gui::WidgetActionWidget {
        public:
            Widget(QWidget* parent, SettingsAction* SettingsAction);
        };

        QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override {
            return new SettingsAction::Widget(parent, this);
        };

    public:
        SettingsAction(XSCClusterRankPlugin& XSCClusterRankPlugins);

public: // Action getters

    DatasetPickerAction& getMainPointsDataset() { return _mainPointsDataset; }
    DatasetPickerAction& getHierarchyTopClusterDataset() { return _hierarchyTopClusterDataset; }
    DatasetPickerAction& getHierarchyMiddleClusterDataset() { return _hierarchyMiddleClusterDataset; }
    DatasetPickerAction& getHierarchyBottomClusterDataset() { return _hierarchyBottomClusterDataset; }
    VariantAction& getSelectedClusterNames() { return _selectedClusterNamesVariant; }
    //VariantAction& getFilteredGeneNames() { return _filteredGeneNamesVariant; }
    //TriggerAction& getUpdateButtonForGeneFiltering() { return _updateButtonForGeneFiltering; }
    DatasetPickerAction& getSpeciesNamesDataset() { return _speciesNamesDataset; }
    DatasetPickerAction& getFilterEditTreeDataset() { return _filterEditTreeDataset; }
    //IntegralAction& getTopNGenesFilter() { return _topNGenesFilter; }
    OptionSelectionAction& getOptionSelectionAction() { return _optionSelectionAction; }
    DatasetPickerAction& getReferenceTreeDataset() { return _referenceTreeDataset; }
    StringAction& getGeneNamesConnection() { return _geneNamesConnection; }
    TriggerAction& getCreatePointSelectTree() { return _createPointSelectTree; }
    DatasetPickerAction& getEmbeddingDataset() { return _embeddingDataset; }
    OptionsAction& getTopHierarchyRelativeClusterCountInclusion() { return _topHierarchyRelativeClusterCountInclusion; }
    StringAction& getStatusChangedAction() { return _statusChangedAction; }
    TriggerAction& getRemoveTableSelection() { return _removeTableSelection; }
    TriggerAction& getSubsampleDataStart() { return _subsampleDataStart; }
    OptionAction& getSubsampleByLevel() { return _subsampleByLevel; }
    DecimalAction& getSubsamplePercentValue() { return _subsamplePercentValue; }
    ToggleAction& getSubsampleInplace() { return _subsampleInplace; }
    QMessageBox* getSubsamplingPopupMessage() const { return _subsamplingPopupMessage; }
    TriggerAction& getGenerateTreeDataFilesPerClusterStart() { return _generateTreeDataFilesPerClusterStart; }
    StringAction& getClusterOrder() { return _clusterOrder; }
    StringAction& getRightClickedCluster() { return _rightClickedCluster; }
    TriggerAction& getClearRightClickedCluster() { return _clearRightClickedCluster; }
    StringAction& getTopSelectedHierarchyStatus() { return _topSelectedHierarchyStatus; }

     

    //DecimalAction& getTreeSimilarity() { return _treeSimilarity; }
    double* condensedDistanceMatrix(std::vector<float>& items);
    std::string mergeToNewick(int* merge, int numOfLeaves);
    QString createJsonTreeFromNewick(QString tree, std::vector<QString> leafNames);
    void populatePointDataParent(QString& datasetId, PointDataStructParent& pointDataValues);
    void populatePointDataChild(QString& datasetId, PointDataStructChild& pointDataValues);
    void populateClusterData(QString& datasetId, std::map<QString, std::pair<QColor, std::vector<int>>>& clusterMap);
    void updateButtonTriggeredForTreeCreation();
    private:
        //QVariant createModelFromData(const QStringList& returnGeneList, const std::map<QString, std::map<QString, float>>& map, const QString& treeDatasetId, const float& treeSimilarityScore, const std::map<QString, std::vector<QString>>& geneCounter, const int& n);
        //QVariant findTopNGenesPerCluster(const std::map<QString, std::map<QString, float>>& map, int n, QString datasetId, float treeSimilarityScore);

public: // Serialization

    /**
     * Load widget action from variant map
     * @param Variant map representation of the widget action
     */
    void fromVariantMap(const QVariantMap& variantMap) override;

    /**
     * Save widget action to variant map
     * @return Variant map representation of the widget action
     */
    QVariantMap toVariantMap() const override;

protected:
    XSCClusterRankPlugin& _XSCClusterRankPlugin;
    DatasetPickerAction    _mainPointsDataset;
    DatasetPickerAction    _hierarchyTopClusterDataset;
    DatasetPickerAction    _hierarchyMiddleClusterDataset;
    DatasetPickerAction    _hierarchyBottomClusterDataset;

    VariantAction           _selectedClusterNamesVariant;
    //VariantAction           _filteredGeneNamesVariant;
    //TriggerAction          _updateButtonForGeneFiltering;
    DatasetPickerAction    _speciesNamesDataset;
    DatasetPickerAction    _filterEditTreeDataset;
    //std::map<QString, std::map<QString, float>> _clusterNameToGeneNameToExpressionValue;
    //IntegralAction          _topNGenesFilter;
    OptionSelectionAction         _optionSelectionAction;
    DatasetPickerAction    _referenceTreeDataset;
    StringAction           _geneNamesConnection;
    //DecimalAction          _treeSimilarity;
    TriggerAction         _createPointSelectTree;
    DatasetPickerAction    _embeddingDataset;
    OptionsAction           _topHierarchyRelativeClusterCountInclusion;
    StringAction           _statusChangedAction;
    TriggerAction            _removeTableSelection;
    TriggerAction            _subsampleDataStart;
    OptionAction             _subsampleByLevel;
    DecimalAction           _subsamplePercentValue;
    ToggleAction             _subsampleInplace;
    TriggerAction            _generateTreeDataFilesPerClusterStart;
    StringAction             _clusterOrder;
    StringAction              _rightClickedCluster;
    TriggerAction              _clearRightClickedCluster;
    StringAction               _topSelectedHierarchyStatus;
    QMessageBox*               _subsamplingPopupMessage;
};
