// ManiVault invokes this function to set the plot data,
// when emitting qt_js_setDataInJS from the communication object
// The connection is established in qwebchannel.tools.js


window.onresize = doALoadOfStuff;
var data;
var svg;
var x;
var y;
var partition;
var rect;
var margin = { top: 10, right: 10, bottom: 10, left: 10 };
var width;
var height;
var selectedClusterContainer = [];
var setMarkingsFlag = false;
var fullMark = [];
var partialMark = [];
var previousClicked;
function doALoadOfStuff() {
    if (data != "") {
        VisCreate();
    }


}

function removeRectHighlight(d) {
    if (data != "") {
        d3.selectAll('rect')
            .style('stroke', '#000')
            .style('stroke-width', 0.2);
        selectedClusterContainer = [];

    }
}

function removeRightClickIcon(d) {
    if (data != "") {
        d3.selectAll(".magnifying-icon").remove();
    }
}

function drawChart(d) {

    if (d != "") {
        //log("\nData received: \n" + d)
        var parsedData = JSON.parse(d);
        // If parsedData is an array, take the first element
        data = Array.isArray(parsedData) ? parsedData[0] : parsedData;
        VisCreate();

    }
}

function clusterMarks(d) {

    if (d != "") {
        var dataarrayfullpartial = d.split("##%%$joinHere$%%##");
        fullMark = [];
        partialMark = [];
        fullMark = dataarrayfullpartial[0].split("@$%%$@");
        partialMark = dataarrayfullpartial[1].split("@$%%$@");

        //if either of the mark is non empty
        if (fullMark.length > 0 || partialMark.length > 0) {
            setMarkingsFlag = true;
            if (data != "") {
                VisCreate();
            }
            setMarkingsFlag = false;
        }

    }

}

if(isDebug) {
    console.log("It's Debug Mode");
    data = JSON.stringify(
                [
                    {
                        "children": [
                            {
                                "children": [
                                    {
                                        "children": [
                                            { "color": "#e5a7b0", "name": "Lamp5_1", "value": 664 },
                                            { "color": "#e097a1", "name": "Lamp5_2", "value": 5846 },
                                            { "color": "#dc8793", "name": "Lamp5_3", "value": 3314 },
                                            { "color": "#b36972", "name": "Lamp5_5", "value": 2199 },
                                            { "color": "#e386ff", "name": "Sncg_3", "value": 637 }
                                        ],
                                        "color": "#da808c",
                                        "name": "Lamp5"
                                    },
                                    {
                                        "children": [
                                            { "color": "#f641a8", "name": "Chandelier", "value": 2356 },
                                            { "color": "#e47175", "name": "Pvalb_1", "value": 20898 },
                                            { "color": "#e2686c", "name": "Pvalb_2", "value": 1556 }
                                        ],
                                        "color": "#d93137",
                                        "name": "Pvalb"
                                    },
                                    {
                                        "children": [
                                            { "color": "#cd7883", "name": "Lamp5_4", "value": 890 },
                                            { "color": "#e691ff", "name": "Sncg_2", "value": 622 },
                                            { "color": "#e17bff", "name": "Sncg_4", "value": 1942 }
                                        ],
                                        "color": "#df70ff",
                                        "name": "Sncg"
                                    },
                                    {
                                        "children": [
                                            { "color": "#ffb84f", "name": "Sst_1", "value": 1659 },
                                            { "color": "#ffb342", "name": "Sst_2", "value": 1809 },
                                            { "color": "#ffb03c", "name": "Sst_3", "value": 10966 },
                                            { "color": "#ffae36", "name": "Sst_4", "value": 2277 },
                                            { "color": "#ffac30", "name": "Sst_5", "value": 957 },
                                            { "color": "#ffa92a", "name": "Sst_6", "value": 1343 },
                                            { "color": "#ffa724", "name": "Sst_7", "value": 230 }
                                        ],
                                        "color": "#ff9900",
                                        "name": "Sst"
                                    },
                                    {
                                        "children": [
                                            { "color": "#b1b10c", "name": "Sst Chodl", "value": 714 }
                                        ],
                                        "color": "#b1b10c",
                                        "name": "Sst Chodl"
                                    },
                                    {
                                        "children": [
                                            { "color": "#e99cff", "name": "Sncg_1", "value": 1046 },
                                            { "color": "#c091d3", "name": "Vip_1", "value": 2304 },
                                            { "color": "#bd8cd1", "name": "Vip_2", "value": 6140 },
                                            { "color": "#ba87cf", "name": "Vip_3", "value": 3815 },
                                            { "color": "#b883cd", "name": "Vip_4", "value": 1540 }
                                        ],
                                        "color": "#a45fbf",
                                        "name": "Vip"
                                    }
                                ],
                                "color": "#d93137",
                                "name": "GABAergic"
                            },
                            {
                                "children": [
                                    {
                                        "children": [
                                            { "color": "#b1ec30", "name": "L2/3 IT", "value": 71164 }
                                        ],
                                        "color": "#b1ec30",
                                        "name": "L2/3 IT"
                                    },
                                    {
                                        "children": [
                                            { "color": "#588ea2", "name": "L5 ET_1", "value": 7667 },
                                            { "color": "#0d5b78", "name": "L5 ET_2", "value": 45 }
                                        ],
                                        "color": "#0d5b78",
                                        "name": "L5 ET"
                                    },
                                    {
                                        "children": [
                                            { "color": "#86cac6", "name": "L5 IT_1", "value": 46452 },
                                            { "color": "#67bcb7", "name": "L5 IT_2", "value": 28168 },
                                            { "color": "#beb867", "name": "L5 IT_3", "value": 2596 }
                                        ],
                                        "color": "#50b2ad",
                                        "name": "L5 IT"
                                    },
                                    {
                                        "children": [
                                            { "color": "#3e9e64", "name": "L5/6 NP", "value": 7055 }
                                        ],
                                        "color": "#3e9e64",
                                        "name": "L5/6 NP"
                                    },
                                    {
                                        "children": [
                                            { "color": "#6eb0ce", "name": "L6 CT_1", "value": 11148 },
                                            { "color": "#4d9ec3", "name": "L6 CT_2", "value": 17244 },
                                            { "color": "#c0c0c0", "name": "No Agreement", "value": 18497 }
                                        ],
                                        "color": "#2d8cb8",
                                        "name": "L6 CT"
                                    },
                                    {
                                        "children": [
                                            { "color": "#beb867", "name": "L6 IT_1", "value": 3537 },
                                            { "color": "#716c18", "name": "L6 IT_2", "value": 20049 }
                                        ],
                                        "color": "#a19922",
                                        "name": "L6 IT"
                                    },
                                    {
                                        "children": [
                                            { "color": "#ffd700", "name": "L6 IT_3", "value": 2428 }
                                        ],
                                        "color": "#5100ff",
                                        "name": "L6 IT Car3"
                                    },
                                    {
                                        "children": [
                                            { "color": "#7044aa", "name": "L6b", "value": 8986 }
                                        ],
                                        "color": "#7044aa",
                                        "name": "L6b"
                                    }
                                ],
                                "color": "#b1ec30",
                                "name": "Glutamatergic"
                            },
                            {
                                "children": [
                                    {
                                        "children": [
                                            { "color": "#958f80", "name": "Astro_1", "value": 446 },
                                            { "color": "#484132", "name": "Astro_2", "value": 5000 },
                                            { "color": "#374a45", "name": "OPC", "value": 3860 },
                                            { "color": "#c0c0c0", "name": "Unknown", "value": 4 }
                                        ],
                                        "color": "#665c47",
                                        "name": "Astro"
                                    },
                                    {
                                        "children": [
                                            { "color": "#8d6c62", "name": "Endo", "value": 1117 }
                                        ],
                                        "color": "#8d6c62",
                                        "name": "Endo"
                                    },
                                    {
                                        "children": [
                                            { "color": "#94af97", "name": "Microglia/PVM", "value": 1419 }
                                        ],
                                        "color": "#94af97",
                                        "name": "Micro-PVM"
                                    },
                                    {
                                        "children": [
                                            { "color": "#88a19a", "name": "Oligo_1", "value": 7099 },
                                            { "color": "#3a544c", "name": "Oligo_2", "value": 8308 }
                                        ],
                                        "color": "#53776c",
                                        "name": "Oligo"
                                    },
                                    {
                                        "children": [
                                            { "color": "#697255", "name": "VLMC", "value": 952 }
                                        ],
                                        "color": "#697255",
                                        "name": "VLMC"
                                    }
                                ],
                                "color": "#53776c",
                                "name": "Non-Neuronal"
                            }
                        ],
                        "color": "white",
                        "name": "All"
                    }
                ],
                null,
                4
            );
    drawChart(data);
}