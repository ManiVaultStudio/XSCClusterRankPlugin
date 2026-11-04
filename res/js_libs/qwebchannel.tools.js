var isQtAvailable = false;

// Here, we establish the connection to Qt
// Any signals that we want to send from ManiVault to
// the JS side have to be connected here
try {
    new QWebChannel(qt.webChannelTransport, function (channel) {
        // Establish connection
        QtBridge = channel.objects.QtBridge;

        // register signals
        QtBridge.qt_js_setDataAndPlotInJS.connect(function () { drawChart(arguments[0]); });   // drawChart is defined in icicle_chart.tools.js
        QtBridge.qt_js_removeClusterSelectionHighlight.connect(function () { removeRectHighlight(arguments[0]); });   // drawChart is defined in icicle_chart.tools.js

        QtBridge.qt_js_setClusterMarks.connect(function () { clusterMarks(arguments[0]); }); 

        QtBridge.qt_js_removeRightClickIcon.connect(function () { removeRightClickIcon(arguments[0]); });  

        // confirm successful connection
        isQtAvailable = true;
        notifyBridgeAvailable();
    });
} catch (error) {
    log("XSC Cluster Rank: qwebchannel: could not connect qt");
}

// The slot js_available is defined by ManiVault's WebWidget and will
// invoke the initWebPage function of our web widget (here, ChartWidget)
function notifyBridgeAvailable() {

    if (isQtAvailable) {
        QtBridge.js_available();
    }
    else {
        log("XSC Cluster Rank: qwebchannel: QtBridge is not available - something went wrong");
    }

}

// The QtBridge is connected to our WebCommunicationObject (ChartCommObject)
// and we can call all slots defined therein
function passSelectionToQt(dat) {

    //convert dat to string from array by adding a separator and then pass it to Qt
    /*
    if (dat != "") {
        dat = dat.join(" @%$,$%@ ");
    }
    else {
        dat = "";
    }
    */

    if (isQtAvailable) {
        QtBridge.js_qt_passSelectionToQt(dat);
    }
}




function passClusterOrderToQT(dat) {
    //convert dat to string from array by adding a separator and then pass it to Qt
    if (dat != "") {
        dat = dat.join(" @%$,$%@ ");
    } else {
        dat = "";
    }

    if (isQtAvailable) {
        QtBridge.js_qt_passClusterOrderToQT(dat);
    }
}


function passRightClickToQt(dat) {

    if (isQtAvailable) {
        QtBridge.js_qt_passRightClickToQt(dat);
    }
}



// utility function: pipe errors to log
window.onerror = function (msg, url, num) {
    log("XSC Cluster Rank: qwebchannel: Error: " + msg + "\nURL: " + url + "\nLine: " + num);
};

// utility function: auto log for Qt and console
function log(logtext) {

    if (isQtAvailable) {
        QtBridge.js_debug(logtext.toString());
    }
    else {
        console.log(logtext);
    }
}
