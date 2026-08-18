#pragma once

#include <ViewPlugin.h>

#include <Dataset.h>
#include <PointData/PointData.h>
#include <ClusterData/ClusterData.h>
#include <XSCTreeData/XSCTreeData.h>
#include <widgets/DropWidget.h>
#include "SettingsAction.h"
#include <QWidget>
#include <actions/HorizontalToolbarAction.h>

/** All plugin related classes are in the ManiVault plugin namespace */
using namespace mv::plugin;

/** Drop widget used in this plugin is located in the ManiVault gui namespace */
using namespace mv::gui;

/** Dataset reference used in this plugin is located in the ManiVault util namespace */
using namespace mv::util;

class ChartWidget;


class XSCClusterRankPlugin : public ViewPlugin
{
    Q_OBJECT

public:
    /**
     * Constructor
     * @param factory Pointer to the plugin factory
     */
    XSCClusterRankPlugin(const PluginFactory* factory);

    /** Destructor */
    ~XSCClusterRankPlugin() override = default;
    
    /** This function is called by the core after the view plugin has been created */
    void init() override;

    /** Store a private reference to the data set that should be displayed */
    void loadData(const mv::Datasets& datasets) override;
    //void createHierarchy(qsizetype index, const Dataset<DatasetImpl>& dataset);
public slots:
    /** Converts ManiVault's point data to a json-like data structure that Qt can pass to the JS code */
    void convertDataAndUpdateChart();
    void computeHierarchy();
    void hierarchyStrokeforSelected();

private:
    /** Published selections received from the JS side to ManiVault's core */
    void publishSelection(const std::vector<QString>& selectedIDs, const QString& topHselectedIDs);
    void publishClusterOrder(const QString& selectedIDs);
    void publishRightClickCluster(const QString& selectedIDs);
    QJsonObject createJsonTree(std::map<QString, int> speciesSelectedIndicesCounter);
    QString getCurrentDataSetID() const;
    SettingsAction& getSettingsAction() { return _settingsAction; }

public: // Serialization

    /**
     * Load plugin from variant map
     * @param Variant map representation of the plugin
     */
    Q_INVOKABLE void fromVariantMap(const QVariantMap& variantMap) override;

    /**
     * Save plugin to variant map
     * @return Variant map representation of the plugin
     */
    Q_INVOKABLE QVariantMap toVariantMap() const override;
protected:
    ChartWidget*            _chartWidget;       // WebWidget that sets up the HTML page
    SettingsAction      _settingsAction;    // Settings action for the plugin
    DropWidget*             _dropWidget;        // Widget for drag and drop behavior
    mv::Dataset<Points>   _currentDataSet;    // Reference to currently shown data set
    //mv::Dataset<Clusters> _clusterDataset;    // Reference to the cluster dataset
    mv::Dataset<Points>   _embeddingDataset; // Reference to the low-dimensional t-SNE dataset
    //Dataset<XSCTree>    _mainTreeDataset; // Reference to the main tree dataset
    bool _pauseSelectionEvent = false;
    QVariantList _dataForChart;
};

/**
 * XSCClusterRank view plugin factory class
 *
 * Note: Factory does not need to be altered (merely responsible for generating new plugins when requested)
 */
class XSCClusterRankPluginFactory : public ViewPluginFactory
{
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID   "studio.manivault.XSCClusterRankPlugin"
                      FILE  "XSCClusterRankPlugin.json")

public:

    /** Default constructor */
    XSCClusterRankPluginFactory();

    /** Destructor */
    ~XSCClusterRankPluginFactory() override {}
    
    /** Creates an instance of the XSCClusterRank view plugin */
    ViewPlugin* produce() override;

    /** Returns the data types that are supported by the XSCClusterRank view plugin */
    mv::DataTypes supportedDataTypes() const override;

    /**
     * Get plugin trigger actions given \p datasets
     * @param datasets Vector of input datasets
     * @return Vector of plugin trigger actions
     */
    PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;
};
