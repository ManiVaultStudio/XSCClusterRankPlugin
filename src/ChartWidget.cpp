#include "ChartWidget.h"
#include "XSCClusterRankPlugin.h"

#include <QDebug>
#include <QString>

using namespace mv;
using namespace mv::gui;

// =============================================================================
// ChartCommObject
// =============================================================================

ChartCommObject::ChartCommObject() :
    _selectedIDsFromJS()
{
}

void ChartCommObject::js_qt_passSelectionToQt(const QString& data){
    _selectedIDsFromJS.clear();
   

    if (data!="")
    {
        QStringList twoList = data.split(" $topsplit$ ");

        if (twoList.size()==2)
        {
            QStringList list = twoList[0].split(" @%$,$%@ ");
            for (const auto& item : list) {
                _selectedIDsFromJS.push_back(item);
            }

                _selectedTopIDSFromJS= twoList[1];

        }
        /*
        // data.split(" @%$,$%@ ") and store in _selectedIDsFromJS.push_back
        QStringList list = data.split(" @%$,$%@ ");
        for (const auto& item : list) {
            _selectedIDsFromJS.push_back(item);
        }
        */
        //qDebug() << "ChartCommObject::js_qt_passSelectionToQt: Not empty";// Selected items : " << _selectedIDsFromJS[0]; // in this case we know that it is only one
    }    
    else
    {
        qDebug() << "ChartCommObject::js_qt_passSelectionToQt: Empty";
    }
    // Notify ManiVault core and thereby other plugins about new selection
    emit passSelectionToCore(_selectedIDsFromJS, _selectedTopIDSFromJS);
}


void ChartCommObject::js_qt_passClusterOrderToQT(const QString& data) {

    // Notify ManiVault core and thereby other plugins about new selection
    emit passClusterOrderToCore(data);
}

void ChartCommObject::js_qt_passRightClickToQt(const QString& data) {

    // Notify ManiVault core and thereby other plugins about new selection
    emit passRightClickToCore(data);
}

// =============================================================================
// ChartWidget
// =============================================================================

ChartWidget::ChartWidget(XSCClusterRankPlugin* viewJSPlugin):
    _viewJSPlugin(viewJSPlugin),
    _comObject()
{
    // For more info on drag&drop behavior, see the Cross-Species Comparison Cluster Rank View project
    setAcceptDrops(true);

    // Ensure linking to the resources defined in res/XSCClusterRank_chart.qrc
    Q_INIT_RESOURCE(XSCClusterRank_chart);

    // ManiVault and Qt create a "QtBridge" object on the js side which represents _comObject
    // there, we can connect the signals qt_js_* and call the slots js_qt_* from our communication object
    init(&_comObject);

    layout()->setContentsMargins(0, 0, 0, 0);
}

void ChartWidget::initWebPage()
{
    qDebug() << "ChartWidget::initWebPage: WebChannel bridge is available.";
    // This call ensures data chart setup when this view plugin is opened via the context menu of a data set
    _viewJSPlugin->convertDataAndUpdateChart();
}

