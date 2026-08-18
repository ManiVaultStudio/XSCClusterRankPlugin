#include "SettingsAction.h"
#include "XSCClusterRankPlugin.h"
#include <vector>
#include <numeric>
#include <QtConcurrent>
#include <iostream>
#include <algorithm>
#include <XSCTreeData/XSCTreeData.h>
#include "lib/Distance/annoylib.h"
#include "lib/Distance/kissrandom.h"
#include "lib/JSONnlohmann/json.hpp"
#include "lib/Clustering/fastcluster.h"
//#include "lib/NewickComparator/newick_comparator.h" //https://github.com/MaciejSurowiec/Maximum_agreement_subtree_problem
#include <stack>
#include <sstream>
#include <QString>
#include <cstdio>
#ifdef _WIN32
#include <execution>
#endif

#include <random>

bool areSameIgnoreOrder(const QStringList& list1, const QStringList& list2) {
    if (list1.size() != list2.size()) {
        return false;
    }

    QStringList sortedList1 = list1;
    QStringList sortedList2 = list2;

    std::sort(sortedList1.begin(), sortedList1.end());
    std::sort(sortedList2.begin(), sortedList2.end());

    return sortedList1 == sortedList2;
}




float calculateVariance(const std::vector<float>& numbers) {
    float sum = std::accumulate(numbers.begin(), numbers.end(), 0.0);
    float mean = sum / numbers.size();
    float variance = 0.0;

    for (const auto& num : numbers) {
        variance += std::pow(num - mean, 2);
    }

    return variance / numbers.size();
}
std::vector<int> convertToIntVector(const std::vector<uint32_t>& input) {
    std::vector<int> output(input.size());
    std::transform(input.begin(), input.end(), output.begin(), [](uint32_t val) {
        return static_cast<int>(val);
        });
    return output;
}
void updateIndicesMap(const Cluster& cluster, std::map<QString, std::vector<int>>& indicesMap) {
    auto clusterIndices = cluster.getIndices();
    indicesMap[cluster.getName()] = convertToIntVector(clusterIndices);
}
std::string jsonToNewick(const nlohmann::json& node, const std::vector<QString>& species) {
    std::string newick;
    if (node.contains("children")) {
        newick += "(";
        for (const auto& child : node["children"]) {
            newick += jsonToNewick(child, species) + ",";
        }
        newick = newick.substr(0, newick.size() - 1);  // Remove trailing comma
        newick += ")";
    }
    if (node.contains("name")) {
        std::string nodeName = node["name"].get<std::string>();
        auto it = std::find_if(species.begin(), species.end(), [&nodeName](const QString& str) {
            return str.compare(QString::fromStdString(nodeName)) == 0;
            });
        if (it != species.end()) {
            newick += std::to_string(std::distance(species.begin(), it) + 1);  // Indices start from 1
        }
    }
    return newick;
}

void printMap(const std::map<QString, std::map<QString, float>>& map) {
    for (const auto& outerPair : map) {
        //std::cout << "Cluster Name: " << outerPair.first.toStdString() << std::endl;
        for (const auto& innerPair : outerPair.second) {
            //std::cout << "    Gene Name: " << innerPair.first.toStdString() << ", Expression Value: " << innerPair.second << std::endl;
        }
    }
}
float calculateMean(const std::vector<float>& v) {
    if (v.empty())
        return 0.0f;

    
    float sum = 0.0f;
    #ifdef _WIN32
        sum = std::reduce(std::execution::par, v.begin(), v.end());
    #else
        sum = std::reduce( v.begin(), v.end());
    #endif

    float mean = sum / v.size();

    return mean;
}
struct Statistics {
    float mean; 
    float variance;
    float stdDeviation;
};

Statistics calculateStatistics(const std::vector<float>& numbers) {
    if (numbers.empty()) {
        return { 0.0, 0.0, 0.0 };
    }

    float sum = std::accumulate(numbers.begin(), numbers.end(), 0.0);
    float mean = sum / numbers.size();
    float sq_sum = std::inner_product(numbers.begin(), numbers.end(), numbers.begin(), 0.0);
    float variance = sq_sum / numbers.size() - mean * mean;
    float stdDeviation = std::sqrt(variance);

    return { mean, variance, stdDeviation };
}


std::string SettingsAction::mergeToNewick(int* merge, int numOfLeaves) {
    std::vector<std::string> labels(numOfLeaves);
    for (int i = 0; i < numOfLeaves; ++i) {
        labels[i] = std::to_string(i + 1);
    }

    std::stack<std::string> stack;

    for (int i = 0; i < 2 * (numOfLeaves - 1); i += 2) {
        int left = merge[i];
        int right = merge[i + 1];

        std::string leftStr;
        if (left < 0) {
            leftStr = labels[-left - 1];
        }
        else {
            leftStr = stack.top();
            stack.pop();
        }

        std::string rightStr;
        if (right < 0) {
            rightStr = labels[-right - 1];
        }
        else {
            rightStr = stack.top();
            stack.pop();
        }

        std::string merged = "(" + leftStr + "," + rightStr + ")";
        stack.push(merged);
    }

    return stack.top() + ";";
}
QString SettingsAction::createJsonTreeFromNewick(QString tree, std::vector<QString> leafnames)
{
    int i = 0;
    std::string jsonString = "";
    std::stringstream jsonStream;
    std::string newick = tree.toStdString();
    while (i < newick.size()) {
        if (newick[i] == '(') {
            jsonStream << "{\n\"children\": [";
            i++;
        }
        else if (newick[i] == ',') {
            jsonStream << ",";
            i++;
        }
        else if (newick[i] == ')') {
            jsonStream << "],\n\"id\": 1,\n\"score\": 1,\n\"branchLength\": 1.0,\n\"width\": 1\n}";
            i++;
        }
        else if (newick[i] == ';') {
            break;
        }
        else {
            if (isdigit(newick[i])) {
                int skip = 1;
                std::string num = "";
                for (int j = i; j < newick.size(); j++) {
                    if (isdigit(newick[j])) {
                        continue;
                    }
                    else {
                        num = newick.substr(i, j - i);

                        skip = j - i;
                        break;
                    }
                }
                std::string species = leafnames[(std::stoi(num) - 1)].toStdString();
                jsonStream << "{\n\"color\": \"#000000\",\n\"hastrait\": true,\n\"iscollapsed\": false,\n\"branchLength\": 1.0,\n\"mean\": 0.0,\n\"name\": \"" << species << "\"\n}";
                i += skip;
            }
        }
    }

    jsonString = jsonStream.str();

    nlohmann::json json = nlohmann::json::parse(jsonString);
    std::string jsonStr = json.dump(4);
    //qDebug()<< "XSCClusterRankPlugin::createJsonTree: jsonStr: " << QString::fromStdString(jsonStr);
    QString formattedTree = QString::fromStdString(jsonStr);

    
    return  formattedTree;
}
double* SettingsAction::condensedDistanceMatrix(std::vector<float>& items) {
    int n = items.size();
    double* distmat = new double[(n * (n - 1)) / 2];
    int k = 0;

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            distmat[k] = std::abs(items[i] - items[j]);
            ++k;
        }
    }

    /*std::cout << "Distance matrix: " << std::endl;
    int index = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            std::cout << "Distance " << i << " value: " << items[i] << " and " << j << " value: " << items[j] << ": " << distmat[index++] << std::endl;
        }
    }*/
    return distmat;
}
/*
QVariant SettingsAction::createModelFromData(const QStringList& returnGeneList, const std::map<QString, std::map<QString, float>>& map,  const QString& treeDatasetId ,const float& treeSimilarityScore, const std::map<QString, std::vector<QString>>& geneCounter, const int& n) {

    if (returnGeneList.isEmpty() || map.empty()) {
        return QVariant();
    }

    QStandardItemModel* model = new QStandardItemModel();
    QStringList initColumnNames = { "ID",  "Variance", "Newick tree","Top "+QString::number(n)+" Appearances","Similarity with Reference Tree", "Standard Deviation","Grand Mean","Gene Apearance Species"};
    model->setHorizontalHeaderLabels(initColumnNames);
    int numOfSpecies = map.size();
    std::map<QString, std::map<QString, float>>::const_iterator it = map.begin();
    for (int i = 0 + initColumnNames.size(); i < numOfSpecies + initColumnNames.size(); i++, it++) {
        QString headerTitle = it->first;
        headerTitle.replace("_", " ");
        headerTitle = QString("Mean ") + headerTitle;
        model->setHorizontalHeaderItem(i, new QStandardItem(headerTitle));
    }
    

    std::map<QString, QString> newickTrees;
    for (auto gene : returnGeneList)
    {
        QList<QStandardItem*> row;
        std::vector<float> numbers;


        for (const auto& outerPair : map) {
            QString outerKey = outerPair.first;
            const std::map<QString, float>& innerMap = outerPair.second;
            try {
                numbers.push_back(innerMap.at(gene));
            }
            catch (const std::out_of_range& e) {
                numbers.push_back(0);
            }



        }

        const std::unordered_map<std::string, int> clusteringTypeMap = {
    {"Complete", HCLUST_METHOD_COMPLETE},
    {"Average", HCLUST_METHOD_AVERAGE},
    {"Median", HCLUST_METHOD_MEDIAN}
        };
        std::string clusteringTypecurrentText = "Single";  //"Single","Complete", "Average","Median"
        int opt_method = clusteringTypeMap.count(clusteringTypecurrentText) ? clusteringTypeMap.at(clusteringTypecurrentText) : HCLUST_METHOD_SINGLE;

        auto numOfLeaves = numOfSpecies;
        double* distmat = new double[(numOfLeaves * (numOfLeaves - 1)) / 2];


        distmat = condensedDistanceMatrix(numbers);

        int* merge = new int[2 * (numOfLeaves - 1)];
        double* height = new double[numOfLeaves - 1];
        hclust_fast(numOfLeaves, distmat, opt_method, merge, height);
        std::string newick = mergeToNewick(merge, numOfLeaves);
        int totalChars = newick.length();
        //std::cout << "\nOriginal Newick format: " << newick << std::endl;
        //add gene and newick to newickTrees
        newickTrees.insert(std::make_pair(gene, QString::fromStdString(newick)));

        delete[] distmat;
        delete[] merge;
        delete[] height;

        Statistics stats = calculateStatistics(numbers);

        row.push_back(new QStandardItem(gene));
        row.push_back(new QStandardItem(QString::number(stats.variance)));
        row.push_back(new QStandardItem(""));
        QString key = gene;
        //qDebug() << "\n**Trying to find key:" << gene << "\n";
        auto it = geneCounter.find(key);
        if (it != geneCounter.end()) {
            QString value = QString::number((it->second).size()) + "/" + QString::number(numOfSpecies) + " Species";
            //qDebug() << "Key found. Value:" << value << "\n";
            row.push_back(new QStandardItem(value));
        }
        else {
            qDebug() << "Key "<< gene<<"not found.\n";
            row.push_back(new QStandardItem("N/A"));
        }


        row.push_back(new QStandardItem(QString::number(-1)));
        
        row.push_back(new QStandardItem(QString::number(stats.stdDeviation)));
        row.push_back(new QStandardItem(QString::number(stats.mean)));
        QString speciesGeneAppearancesComb;
        for (const auto& str : it->second) {
            if (!speciesGeneAppearancesComb.isEmpty()) {
                speciesGeneAppearancesComb += ";";
            }
            speciesGeneAppearancesComb += str;
        }

        row.push_back(new QStandardItem(speciesGeneAppearancesComb));
        for (auto numb : numbers)
        {
            row.push_back(new QStandardItem(QString::number(numb)));
        }

        // Create a new item for the vertical header
        //QStandardItem* verticalHeaderItem = new QStandardItem(QString::number(geneCounter[gene]));

        // Add the new item to the model's vertical header
       // model->setVerticalHeaderItem(model->rowCount(), verticalHeaderItem);

        // Add the row to the model
        model->appendRow(row);
    }

    //qDebug() << "***********Access location\n";
    //for (auto& pair : geneCounter) {
    //    std::cout << "Gene: " << pair.first.toStdString() << ", Count: " << pair.second << std::endl;
    //}

    //print newickTrees
    //for (auto& pair : newickTrees) {
    //    std::cout << "Gene: " << pair.first.toStdString() << ", Newick: " << pair.second.toStdString() << std::endl;
    //}



    //check which newick trees are exactly similar to "(((((((20,(((24,((25,9),(12,11))),(19,15)),(22,21))),((1,17),23)),((2,18),(8,6))),(14,10)),(3,16)),(7,5)),(13,4));" and add color "#00a2ed" to the genes
    // Define the target newick tree and color
    //std::string targetNewick = "((((((20,(((((18,24),(19,17)),16),(11,25)),(9,6))),(23,4)),((3,1),(15,(22,((13,2),7))))),(10,5)),(8,21)),(12,14));";
    QString targetColor = "#fdb900";
    std::string targetNewick="";
    QStringList fullTreeNames;

    //iterate std::map<QString, std::map<QString, float>> map and fill keys in std::vector<QString> leafNames 
    std::vector<QString> leafnames;
    for (const auto& outerPair : map) {
        leafnames.push_back(outerPair.first);
    }



    std::map<QString, std::pair<QString, float>> treeSimilarities;
    if (treeDatasetId != "")
    {
        auto fullTreeData = mv::data().getDataset<XSCTree>(treeDatasetId);
        if (fullTreeData.isValid())
        {
            fullTreeNames = fullTreeData->getTreeLeafNames();
            auto treeData = fullTreeData->getTreeData();

            QJsonDocument jsonDoc(treeData);
            auto temp = jsonDoc.toJson(QJsonDocument::Compact).toStdString();
            auto jsonTree = nlohmann::json::parse(temp);
            targetNewick = jsonToNewick(jsonTree, leafnames);
            targetNewick += ";";  // End of Newick string
        }

    }

    /*

    auto jsonTree = nlohmann::json::parse(jsonString);
    std::string targetNewick = jsonToNewick(jsonTree, leafnames);
    targetNewick += ";";  // End of Newick string

    */
    /*if (fullTreeNames.size()>0 && leafnames.size()>0 && targetNewick != "")
    {

   //convert  std::vector<QString> to QStringList leafnames
        QStringList copyleafNames;
        for (auto& leaf : leafnames) {
            copyleafNames.push_back(leaf);
        }

       

    if(areSameIgnoreOrder(fullTreeNames, copyleafNames)){
    // Iterate over the newickTrees map
    for (auto& pair : newickTrees) {
/*
        qDebug() << "\n*****************";
        qDebug() << "First tree: " << pair.second;
        qDebug() << "Second tree: " << QString::fromStdString(targetNewick);
        qDebug() << "*****************\n";
        */
        //add a ";" to the end of the string pair.second.toStdString()
        /*std::string modifiedNewick = pair.second.toStdString();
        

       const char* string1 = targetNewick.c_str();
       const char* string2 = modifiedNewick.c_str();
        
        //const char* string1 = "(((((((20,(((24,((25,9),(12,11))),(19,15)),(22,21))),(23,(1,17))),((2,18),(8,6))),(14,10)),(3,16)),(7,5)),(13,4));";
        //const char* string2 = "(((((((20,(((24,((25,9),(12,11))),(19,15)),(22,21))),((1,17),23)),((2,18),(8,6))),(14,10)),(16,3)),(7,5)),(13,4));";

    // Create two Tree objects
        Tree t1;
        Tree t2;

        // Change the standard input to read from the strings
        freopen("CON", "r", stdin);
        FILE* file1;
        FILE* file2;
        fopen_s(&file1, "file1.txt", "w");
        fputs(string1, file1);
        if (file1 != nullptr) {
            fclose(file1);
        }
        
        fopen_s(&file2, "file2.txt", "w");
        fputs(string2, file2);
        if (file1 != nullptr) {
            fclose(file2);
        }

        // Read tree structures from the strings
        freopen("file1.txt", "r", stdin);
        t1.CreateTree();
        freopen("file2.txt", "r", stdin);
        t2.CreateTree();

        // Calculate and print the similarity
        int sim = Calculate(&t1, &t2); // sim is the minimum number of leaves to remove to make the trees isomorphic
        
       /* qDebug() << "\n*****************\n"
            << "First tree: " << string1
            << "\nSecond tree: " << string2
            << "\nSimvalue: " << sim
            << "\n*****************\n"; */

        //qDebug()<<"\n****Simvalue: "<<sim<<"****\n";

        // If the current newick tree is the same as the target

        /*float similarity = 1.0 - static_cast<float>(sim) / static_cast<float>(numOfSpecies); //the similarity between two Newick trees,


        //insert pair.first modifiedNewick similarity to treeSimilarities
        std::pair<QString, float>  temp;
        temp.first = createJsonTreeFromNewick(QString::fromStdString(modifiedNewick), leafnames);
        temp.second = similarity;
        treeSimilarities.insert(std::make_pair(pair.first, temp));
        /*
         1.	Calculate(&t1, &t2) is a function that takes two trees in Newick format and returns the minimum number of leaves that need to be removed to make them isomorphic.
2.	sim is the result of this calculation.
3.	numOfSpeciesLeaves is presumably the total number of leaves in the tree (or in both trees if they have the same number of leaves).
4.	static_cast<float>(sim) / static_cast<float>(numOfSpeciesLeaves) calculates the proportion of leaves that need to be removed to make the trees isomorphic.
5.	1.0 - static_cast<float>(sim) / static_cast<float>(numOfSpeciesLeaves) then subtracts this proportion from 1 to give the proportion of leaves that do not need to be removed, which can be interpreted as a measure of similarity between the trees.
         if no leaves need to be removed (i.e., the trees are already isomorphic), sim will be 0, and the similarity will be 1.0. If all leaves need to be removed, sim will be equal to numOfSpeciesLeaves, and the similarity will be 0.
        */
        /*
        int x= (1-treeSimilarityScore)*numOfSpecies;
        
        if (sim <= x) {
            // Find the corresponding gene in the model
            QList<QStandardItem*> items = model->findItems(pair.first);
            for (auto& item : items) {
                // Get the row of the item
                int row = item->row();
                // Set the background color of each item in the row
                for (int i = 0; i < model->columnCount(); i++) {
                    QStandardItem* itemInRow = model->item(row, i);
                    if (itemInRow) {
                        itemInRow->setBackground(QBrush(QColor(targetColor)));
                    }
                }
            }
        }

        */
    /*}

}
    }

    //check which newick trees are the same and group them together
    /*
    std::map<QString, std::vector<QString>> clusteringMap;

    for (auto it = newickTrees.begin(); it != newickTrees.end(); ++it) {
        QString currentNewick = it->second;
        QString currentGene = it->first;
        if (clusteringMap.empty()) {
            clusteringMap[currentNewick] = { currentGene };
        }
        else {
            bool found = false;
            for (auto& cluster : clusteringMap) {
                QString clusterNewick = cluster.first;
                if (currentNewick == clusterNewick) {
                    cluster.second.push_back(currentGene);
                    found = true;
                    break;
                }
            }
            if (!found) {
                clusteringMap[currentNewick] = { currentGene };
            }
        }
    }
    */
    //QStringList colorCodes = {"#8dd3c7" ,"#ffffb3"}

    //colors
    // left right color codes https://encycolorpedia.com/00a2ed #ff5d12 and #00a2ed  or #0fb1e0 and #f04e1f
    //testing
    //clusteringMap[clusteringMap.begin()->first] = { "VSTM5", "REC8", "TPGS2" }; 
    //clusteringMap[(++clusteringMap.begin())->first] = { "FGD6", "ANGEL1","FANCC" };
    
    
    //print clusteringMap    '
    /*for (auto& cluster : clusteringMap) {
        QString newick = cluster.first;
        std::vector<QString> genes = cluster.second;

        if (genes.size() > 1) {
            std::cout << "Newick: " << newick.toStdString() << std::endl;
            std::cout << "Genes: ";
            for (auto& gene : genes) {
                std::cout << gene.toStdString() << ", ";
            }
            std::cout << std::endl;
        }

    }
    */
    /*
    QStringList colorCodes = { "#8dd3c7", "#ffffb3", "#bebada", "#fb8072", "#80b1d3", "#fdb462", "#b3de69", "#fccde5", "#d9d9d9", "#bc80bd", "#ccebc5", "#ffed6f", "#a6cee3", "#1f78b4", "#b2df8a", "#33a02c", "#fb9a99", "#e31a1c", "#fdbf6f", "#ff7f00", "#cab2d6", "#6a3d9a", "#ffff99", "#b15928" };
    int colorIndex = 0;
    for (auto& cluster : clusteringMap) {
        QString newick = cluster.first;
        std::vector<QString> genes = cluster.second;

        if (genes.size() > 1) {
            for (auto& gene : genes) {
                for (int i = 0; i < model->rowCount(); i++) {
                    if (model->item(i, 0)->text() == gene) {
                        for (int j = 0; j < model->columnCount(); j++) {
                            model->item(i, j)->setBackground(QBrush(QColor(colorCodes[colorIndex])));
                        }
                    }
                }
            }
            colorIndex = (colorIndex + 1) % colorCodes.size();
        }
    }

    */

    //based on first column string value from  model, update the 4th column vaLUE   from treeSimilarities
    /*for (int i = 0; i < model->rowCount(); i++) {
        QString gene = model->item(i, 0)->text();
        //qDebug() <<"Gene: " << gene;
        //qDebug() << "Tree Similarity: " << treeSimilarities[gene];
        auto it = treeSimilarities.find(gene);
        auto map = it->second;
        auto similarity = map.second;
        auto newick= map.first;

        if (it != treeSimilarities.end()) {

            model->item(i, 2)->setText(newick);
            model->item(i, 4)->setText(QString::number(similarity));
        }
    }



    return QVariant::fromValue(model);

}
*/
/*QVariant SettingsAction::findTopNGenesPerCluster(const std::map<QString, std::map<QString, float>>& map, int n, QString datasetId, float treeSimilarityScore) {
    
    if (map.empty() || n <= 0) {
        return QVariant();
    }

    QSet<QString> geneList;
    QStringList returnGeneList;
    std::map<QString, std::vector<QString>> geneAppearanceCounter;

    for (const auto& outerPair : map) {
        // Convert map to vector of pairs
        std::vector<std::pair<QString, float>> geneExpressionVec(outerPair.second.begin(), outerPair.second.end());

        // Sort the vector in descending order based on the expression value
        std::sort(geneExpressionVec.begin(), geneExpressionVec.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        // Add the top n genes to the geneList and initialize their count in geneAppearanceCounter
        for (int i = 0; i < std::min(n, static_cast<int>(geneExpressionVec.size())); ++i) {
            geneList.insert(geneExpressionVec[i].first);
        }
    }

    // Convert QSet<QString> geneList to QStringList returnGeneList
    returnGeneList = QStringList(geneList.begin(), geneList.end());

    // Increment count for each gene in geneList for the geneAppearanceCounter for top n genes
    for (const auto& outerPair : map) {
        // Convert map to vector of pairs
        std::vector<std::pair<QString, float>> geneExpressionVec(outerPair.second.begin(), outerPair.second.end());

        // Sort the vector in descending order based on the expression value
        std::sort(geneExpressionVec.begin(), geneExpressionVec.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        for (int i = 0; i < std::min(n, static_cast<int>(geneExpressionVec.size())); ++i) {
            // If geneExpressionVec[i].first is present in the key of geneAppearanceCounter, push the outerpair.first in the vector
            if (geneList.contains(geneExpressionVec[i].first)) {
                geneAppearanceCounter[geneExpressionVec[i].first].push_back(outerPair.first);
            }

        }
    }

    //qDebug() << "***********Insert location\n";
    //for (auto& pair : geneAppearanceCounter) {
    //    std::cout << "Gene: " << pair.first.toStdString() << ", Count: " << pair.second << std::endl;
    //}
    QVariant returnValue;
    //QVariant returnValue = createModelFromData(returnGeneList, map, datasetId, treeSimilarityScore, geneAppearanceCounter,n);

    return returnValue;
}
*/



SettingsAction::SettingsAction(XSCClusterRankPlugin& XSCClusterRankPlugin) :
    WidgetAction(&XSCClusterRankPlugin, "XSCClusterRankPlugin Settings"),
    _XSCClusterRankPlugin(XSCClusterRankPlugin),
    _mainPointsDataset(this, "Main Points"),
    _hierarchyTopClusterDataset(this, "Top Hierarchy"),
    _hierarchyMiddleClusterDataset(this, "Middle Hierarchy"),
    _hierarchyBottomClusterDataset(this, "Bottom Hierarchy"),
    _selectedClusterNamesVariant(this, "Selected Cluster Names Variant"),
    //_filteredGeneNamesVariant(this, "Filtered Gene Names Variant"),
    //_updateButtonForGeneFiltering(this, "Filter Genes Trigger"),
    _speciesNamesDataset(this, "Species Names"),
    //_topNGenesFilter(this, "Top N Genes Filter"),
    _optionSelectionAction(*this),
    _referenceTreeDataset(this, "Reference Tree"),
    _filterEditTreeDataset(this, "Filter Tree"),
    _geneNamesConnection(this, "Gene Names Connection"),
    _createPointSelectTree(this, "Create Point Select Tree"),
    _embeddingDataset(this, "Low Dim Embedding"),
    _topHierarchyRelativeClusterCountInclusion(this, "Top Hierarchy Relative Cluster Count Inclusion"),
    _statusChangedAction(this, "Status Changed"),
    _removeTableSelection(this, "Remove Table Selection"),
    _clusterOrder(this, "Cluster Order"),
    _subsampleDataStart(this, "Subsample Data"),
    _subsampleByLevel(this, "Subsample By Level"),
    _subsamplePercentValue(this, "Subsample Percent Value"),
    _subsampleInplace(this, "Subsample Inplace"),
    _generateTreeDataFilesPerClusterStart(this, "Generate Tree Data Files Per Cluster"),
    _rightClickedCluster(this, "Right Clicked Cluster"),
    _clearRightClickedCluster(this, "Clear Right Clicked Cluster"),
    _topSelectedHierarchyStatus(this, "Top Selected Hierarchy Status")
    //_treeSimilarity(this, "Tree Similarity")
{
    setSerializationName("CSCCR:Cross-Species Comparison Cluster Rank Settings");
    _mainPointsDataset.setSerializationName("CSCCR:MainPointsDataset");
    _embeddingDataset.setSerializationName("CSCCR:EmbeddingDataset");
    _topHierarchyRelativeClusterCountInclusion.setSerializationName("CSCCR:TopHierarchyRelativeClusterCountInclusion");
    _statusChangedAction.setSerializationName("CSCCR:StatusChangedAction");
    _hierarchyTopClusterDataset.setSerializationName("CSCCR:HierarchyTopClusterDataset");
    _hierarchyMiddleClusterDataset.setSerializationName("CSCCR:HierarchyMiddleClusterDataset");
    _hierarchyBottomClusterDataset.setSerializationName("CSCCR:HierarchyBottomClusterDataset");
    _selectedClusterNamesVariant.setSerializationName("CSCCR:SelectedClusterNamesVariant");
    //_filteredGeneNamesVariant.setSerializationName("CSCCR:FilteredGeneNamesVariant");
    //_updateButtonForGeneFiltering.setSerializationName("CSCCR:UpdateButtonForGeneFiltering");
    _speciesNamesDataset.setSerializationName("CSCCR:SpeciesNamesDataset");
    //_topNGenesFilter.setSerializationName("CSCCR:TopNGenesFilter");
    _referenceTreeDataset.setSerializationName("CSCCR:ReferenceTreeDataset");
    _filterEditTreeDataset.setSerializationName("CSCCR:FilterTreeDataset");
    _createPointSelectTree.setSerializationName("CSCCR:CreatePointSelectTreeTrigger");
    _geneNamesConnection.setSerializationName("CSCCR:GeneNamesConnection");
    _removeTableSelection.setSerializationName("CSCCR:RemoveTableSelection");
    _clusterOrder.setSerializationName("CSCCR:ClusterOrder");
    _rightClickedCluster.setSerializationName("CSCCR:RightClickedCluster");
    _clearRightClickedCluster.setSerializationName("CSCCR:ClearRightClickedCluster");
    _topSelectedHierarchyStatus.setSerializationName("CSCCR:TopSelectedHierarchyStatus");
    //_treeSimilarity.setSerializationName("CSCCR:TreeSimilarity");

    setText("Cross-Species Comparison Cluster Rank Settings");
    _mainPointsDataset.setToolTip("Main Points Dataset");
    _embeddingDataset.setToolTip("Embedding Dataset");
    _topHierarchyRelativeClusterCountInclusion.setToolTip("Top Hierarchy Relative Cluster Count Inclusion");
    _statusChangedAction.setToolTip("Status Changed");
    _hierarchyTopClusterDataset.setToolTip("Hierarchy Top Cluster Dataset");
    _hierarchyMiddleClusterDataset.setToolTip("Hierarchy Middle Cluster Dataset");
    _hierarchyBottomClusterDataset.setToolTip("Hierarchy Bottom Cluster Dataset");
    _selectedClusterNamesVariant.setToolTip("Selected Cluster Names Variant");
    //_filteredGeneNamesVariant.setToolTip("Filtered Gene Names Variant");
    //_updateButtonForGeneFiltering.setToolTip("Filter Genes Trigger");
    _speciesNamesDataset.setToolTip("Species Names Dataset");
    _filterEditTreeDataset.setToolTip("Filter Tree Dataset");
    _createPointSelectTree.setToolTip("Create Point Select Tree Trigger");
    //_topNGenesFilter.setToolTip("Top N Genes Filter");
    //_topNGenesFilter.initialize(1, 100, 10);
    _referenceTreeDataset.setToolTip("Reference Tree Dataset");
    _geneNamesConnection.setToolTip("Gene Names Connection");
    _clusterOrder.setToolTip("Cluster Order");
    _rightClickedCluster.setToolTip("Right Clicked Cluster");
    _clearRightClickedCluster.setToolTip("Clear Right Clicked Cluster");
    _topSelectedHierarchyStatus.setToolTip("Top Selected Hierarchy Status");
    _topSelectedHierarchyStatus.setString("");
    _removeTableSelection.setToolTip("Remove Table Selection");
    _subsampleDataStart.setToolTip("Subsample Data");
    _subsampleByLevel.setToolTip("Subsample By Level");
    _subsamplePercentValue.setToolTip("Subsample Percent Value");
    _subsampleInplace.setToolTip("Subsample Inplace");
    _generateTreeDataFilesPerClusterStart.setToolTip("Generate Tree Data Files Per Cluster");
    //_treeSimilarity.setToolTip("Tree Similarity");
   // _treeSimilarity.initialize(0.0, 1.0, 1.0, 2);
    _subsamplePercentValue.initialize(1.00, 100.00, 15.00, 2);
    _rightClickedCluster.setString("");
    _subsampleByLevel.initialize(QStringList{ "Top","Middle","Bottom" }, "Bottom");
    _subsampleInplace.setChecked(true);
    _mainPointsDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == PointType;
        });
    _embeddingDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == PointType;
        });
    _hierarchyTopClusterDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == ClusterType;
        });
    _hierarchyMiddleClusterDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == ClusterType;
        });

    _hierarchyBottomClusterDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == ClusterType;
        });

    _speciesNamesDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == ClusterType;
        });
    _filterEditTreeDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == XSCTreeType;
        });
    _referenceTreeDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == XSCTreeType;
        });
    const auto filterTreedatasetUpdate = [this]() -> void {};
    connect(&_filterEditTreeDataset, &DatasetPickerAction::currentIndexChanged, this, filterTreedatasetUpdate);
    const auto referenceTreedatasetUpdate = [this]() -> void {};
    connect(&_referenceTreeDataset, &DatasetPickerAction::currentIndexChanged, this, referenceTreedatasetUpdate);
    const auto hierarchyTopClusterDatasetUpdate = [this]() -> void
        {

        };
    connect(&_hierarchyTopClusterDataset, &DatasetPickerAction::currentIndexChanged, this, hierarchyTopClusterDatasetUpdate);

    const auto hierarchyMiddleClusterDatasetUpdate = [this]() -> void
        {

        };
    connect(&_hierarchyMiddleClusterDataset, &DatasetPickerAction::currentIndexChanged, this, hierarchyMiddleClusterDatasetUpdate);



    const auto hierarchyBottomClusterDatasetUpdate = [this]() -> void
        {

        };
    connect(&_hierarchyBottomClusterDataset, &DatasetPickerAction::currentIndexChanged, this, hierarchyBottomClusterDatasetUpdate);



    _subsamplingPopupMessage = new QMessageBox();
    _subsamplingPopupMessage->setIcon(QMessageBox::Information);
    _subsamplingPopupMessage->setWindowTitle("Computation process in progress");
    _subsamplingPopupMessage->setText("Subsample process in progress");
    _subsamplingPopupMessage->setInformativeText(
        "This process may take some time based on the input data size, your memory and processor. <br> This popup will close automatically once initialization is complete. "
    );
    _subsamplingPopupMessage->setTextFormat(Qt::RichText); // Enable rich text formatting
    _subsamplingPopupMessage->setTextInteractionFlags(Qt::TextBrowserInteraction); // Enable link interaction
    _subsamplingPopupMessage->setStandardButtons(QMessageBox::NoButton);
    _subsamplingPopupMessage->setModal(true);

    //const auto filteredGeneNamesVariantUpdate = [this]() -> void
    //    {

    //    };
    //connect(&_filteredGeneNamesVariant, &VariantAction::changed, this, filteredGeneNamesVariantUpdate);

    const auto selectedClusterNamesVariantUpdate = [this]() -> void
        {

        };
    connect(&_selectedClusterNamesVariant, &VariantAction::changed, this, selectedClusterNamesVariantUpdate);

    // const auto updateButtonForGeneFilteringUpdate = [this]() -> void
    //     {
    //         auto clusterDataset = _hierarchyBottomClusterDataset.getCurrentDataset();
    //         auto pointsDataset = _mainPointsDataset.getCurrentDataset();
    //         auto speciesDataset = _speciesNamesDataset.getCurrentDataset();
    //         auto treeDataset = _filterTreeDataset.getCurrentDataset();
    //         bool isValid = false;
    //         QString datasetId = "";
    //         if (treeDataset.isValid())
    //         {
    //             datasetId = treeDataset->getId();
    //         }
    //         _clusterNameToGeneNameToExpressionValue.clear();
    //         //std::vector<QString> leafnames;
    //         if (clusterDataset.isValid() && pointsDataset.isValid() && speciesDataset.isValid())
    //         {
    //             isValid = clusterDataset->getParent().getDatasetId() == pointsDataset->getId() && speciesDataset->getParent().getDatasetId() == pointsDataset->getId();

    //             if (isValid)
    //             {
    //                 QVariant variant = _selectedClusterNamesVariant.getVariant();
    //                 QStringList clusterList = variant.toStringList();

    //                 QStringList speciesList;
    //                 auto speciesData = mv::data().getDataset<Clusters>(speciesDataset->getId());
    //                 auto species = speciesData->getClusters();
    //                 if (!species.empty())
    //                 {
    //                     for (auto& specie : species)
    //                     {
    //                         speciesList.push_back(specie.getName());
    //                     }
    //                 }
    //                 QStringList fullTreeNames;
    //                 if (datasetId!="")
    //                 {
    //                     auto fullTreeData = mv::data().getDataset<XSCTree>(datasetId);
    //                     if (fullTreeData.isValid())
    //                     {
    //                         fullTreeNames = fullTreeData->getTreeLeafNames();
    //                     }
    //                     
    //                 }
    //                 
    //                 mv::Datasets filteredDatasets;

    //                 if (!speciesList.empty() )
    //                 {
    //                     
    //                     //if(areSameIgnoreOrder(fullTreeNames, speciesList))
    //                     {
    //                     auto allDatasets = mv::data().getAllDatasets({ PointType });
    //                     for (const auto& dataset : allDatasets)
    //                     {
    //                         if (speciesList.contains(dataset->getGuiName()) && dataset->getDataType() == PointType)
    //                         {
    //                             filteredDatasets.push_back(dataset);
    //                         }
    //                     }
    //                 }
    //                 }
    //                 if (!filteredDatasets.empty())
    //                 {
    //                     
    //                     for (const auto& dataset : filteredDatasets)
    //                     {
    //                         //qDebug() << dataset->getGuiName();
    //                         //leafnames.push_back(dataset->getGuiName());
    //                         auto rawData = mv::data().getDataset < Points>(dataset.getDatasetId());
    //                         auto dimensionNames = rawData->getDimensionNames();
    //                         auto children = rawData->getChildren();
    //                         for (auto& child : children)
    //                         {
    //                             if (child->getGuiName() + "_mainData" == _hierarchyBottomClusterDataset.getCurrentDataset()->getGuiName())
    //                             {
    //                                 auto clustersData = mv::data().getDataset<Clusters>(child->getId());
    //                                 auto clusters = clustersData->getClusters();
    //                                 if (!clusters.empty())
    //                                 {
    //                                     std::vector<int> clusterIndicesSelected;
    //                                     std::vector<int> clusterIndicesFull(rawData->getNumPoints()); //fill it with rawData->getNumPoints()
    //                                     std::iota(clusterIndicesFull.begin(), clusterIndicesFull.end(), 0); // Fill the vector with increasing values
    //                                     for (auto& cluster : clusters)
    //                                     {
    //                                         if (clusterList.contains(cluster.getName()))
    //                                         {
    //                                             auto clusterIndices = cluster.getIndices();
    //                                             std::copy(clusterIndices.begin(), clusterIndices.end(), std::back_inserter(clusterIndicesSelected));
    //                                         }
    //                                     }
    //                                     if (!clusterIndicesSelected.empty())
    //                                     {
    //                                         for (int i = 0; i < dimensionNames.size(); i++)
    //                                         {
    //                                             std::vector<float> resultContainerShort(clusterIndicesSelected.size());
    //                                             std::vector<float> resultContainerFull(clusterIndicesFull.size());
    //                                             std::vector<int> dimensionIndex = { i };
    //                                             rawData->populateDataForDimensions(resultContainerShort, dimensionIndex, clusterIndicesSelected);
    //                                             rawData->populateDataForDimensions(resultContainerFull, dimensionIndex, clusterIndicesFull);
    //                                             float shortMean = calculateMean(resultContainerShort);
    //                                             float fullMean = calculateMean(resultContainerFull);
    //                                             float meanValue = 0.0;
    //                                             if (fullMean != 0.0)
    //                                             {
    //                                                 meanValue = shortMean / fullMean;
    //                                             }
    //                                             _clusterNameToGeneNameToExpressionValue[dataset->getGuiName()][dimensionNames[i]] = meanValue;
    //                                         }
    //                                     }
    //                                 }
    //                             }
    //                         }
    //                     }
    //                 }
    //             }
    //         }
    //         
    //         QVariant geneListTable = findTopNGenesPerCluster(_clusterNameToGeneNameToExpressionValue, _topNGenesFilter.getValue(), datasetId, 1.0/*_treeSimilarity.getValue()*/ );

    //if (!geneListTable.isNull()) 
    //{
    //             _filteredGeneNamesVariant.setVariant(geneListTable);
    //         }
    //         else
    //         {
    //             qDebug()<<"No data found";
    //         }




    //     };
    // connect(&_updateButtonForGeneFiltering, &TriggerAction::triggered, this, updateButtonForGeneFilteringUpdate);

    const auto speciesNamesDatasetUpdate = [this]() -> void
        {

        };
    connect(&_speciesNamesDataset, &DatasetPickerAction::currentIndexChanged, this, speciesNamesDatasetUpdate);

    //const auto topNGenesFilterUpdate = [this]() -> void
    //    {

    //    };
    //connect(&_topNGenesFilter, &IntegralAction::valueChanged, this, topNGenesFilterUpdate);




    const auto updateGenerateTreeDataFilesPerClusterStart = [this]() -> void
        {
            updateButtonTriggeredForTreeCreation();
        };
    connect(&_generateTreeDataFilesPerClusterStart, &TriggerAction::triggered, this, updateGenerateTreeDataFilesPerClusterStart);


    const auto updateSubsampleDataStart = [this]() -> void {
        _subsampleDataStart.setDisabled(true);
        auto mainPointsDataset = _mainPointsDataset.getCurrentDataset();
        auto mainPointsDatasetId = mainPointsDataset->getId();
        auto speciesNamesDataset = _speciesNamesDataset.getCurrentDataset();
        auto topHierarchyDataset = _hierarchyTopClusterDataset.getCurrentDataset();
        auto middleHierarchyDataset = _hierarchyMiddleClusterDataset.getCurrentDataset();
        auto bottomHierarchyDataset = _hierarchyBottomClusterDataset.getCurrentDataset();

        if (!speciesNamesDataset.isValid() || !mainPointsDataset.isValid() || !topHierarchyDataset.isValid() || !middleHierarchyDataset.isValid() || !bottomHierarchyDataset.isValid()) {
            qDebug() << "Datasets not valid! Cannot perform subsample operation. Select all datasets.";
            _subsampleDataStart.setDisabled(false);

            return;
        }

        auto start = std::chrono::high_resolution_clock::now();
        auto subsampleLevel = _subsampleByLevel.getCurrentText();
        auto subsamplePercent = _subsamplePercentValue.getValue();
        auto subsampleInplace = _subsampleInplace.isChecked();
        auto speciesData = mv::data().getDataset<Clusters>(speciesNamesDataset->getId());
        auto mainPointsData = mv::data().getDataset<Points>(mainPointsDataset->getId());
        auto topLevelData = mv::data().getDataset<Clusters>(topHierarchyDataset->getId());
        auto middleLevelData = mv::data().getDataset<Clusters>(middleHierarchyDataset->getId());
        auto bottomLevelData = mv::data().getDataset<Clusters>(bottomHierarchyDataset->getId());

        auto tophierarchyClusters = topLevelData->getClusters();
        auto middlehierarchyClusters = middleLevelData->getClusters();
        auto bottomhierarchyClusters = bottomLevelData->getClusters();
        auto speciesClusters = speciesData->getClusters();
        auto mainPointsDimensions = mainPointsData->getDimensionNames();
        auto mainPointsNumofDims = mainPointsData->getNumDimensions();
        auto mainPointsNumofPoints = mainPointsData->getNumPoints();

        int maxNumOfPointsRequiredbySampling = static_cast<int>(subsamplePercent / 100.0 * mainPointsNumofPoints);

        if (maxNumOfPointsRequiredbySampling < 1) {
            qDebug() << "Cannot perform subsampling. Number of points required by sampling is less than 1.";
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = end - start;
            qDebug() << "Elapsed time: " << elapsed.count() << " seconds";
            _subsampleDataStart.setDisabled(false);
            return;
        }

        qDebug() << "Total indices in the dataset:" << mainPointsNumofPoints;
        qDebug() << "Number of points that will be generated by sampling should be less than or equal to:" << maxNumOfPointsRequiredbySampling;
        _subsamplingPopupMessage->show();
        QApplication::processEvents();
        std::map<QString, std::vector<int>> selectedhierarchyIndicesMap;

        if (subsampleLevel == "Top") {
            for (auto& cluster : tophierarchyClusters) {
                auto clusterIndices = cluster.getIndices();
                selectedhierarchyIndicesMap[cluster.getName()] = convertToIntVector(clusterIndices);
            }
        }
        else if (subsampleLevel == "Middle") {

            for (auto& cluster : middlehierarchyClusters) {
                auto clusterIndices = cluster.getIndices();
                selectedhierarchyIndicesMap[cluster.getName()] = convertToIntVector(clusterIndices);
            }
        }
        else if (subsampleLevel == "Bottom") {

            for (auto& cluster : bottomhierarchyClusters) {
                auto clusterIndices = cluster.getIndices();
                selectedhierarchyIndicesMap[cluster.getName()] = convertToIntVector(clusterIndices);
            }
        }
        else {
            qDebug() << "Invalid subsample level. Cannot perform subsampling.";
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = end - start;
            qDebug() << "Elapsed time: " << elapsed.count() << " seconds";
            _subsampleDataStart.setDisabled(false);
            return;
        }
        qDebug() << "Number of clusters in the hierarchy:" << selectedhierarchyIndicesMap.size();

        std::vector<int> finalIndices;
        std::random_device rd;
        std::mt19937 g(rd());
        qDebug() << "Shuffling and selecting points for subsampling...";
        if (0)
        {
            // Step 1: Calculate the total number of points across all keys
            size_t totalPoints = 0;
            for (const auto& [key, val] : selectedhierarchyIndicesMap) {
                totalPoints += val.size();
            }

            // Step 2: Determine the total number of points to be selected based on subsamplePercent
            size_t totalSubsampleSize = static_cast<size_t>(totalPoints * subsamplePercent / 100.0);

            // Step 3: For each key, calculate the number of points to be selected proportionally
            std::vector<int> finalIndices;
            std::random_device rd;
            std::mt19937 g(rd());

            std::map<QString, size_t> keySubsampleSizes;
            size_t accumulatedSize = 0;

            for (const auto& [key, val] : selectedhierarchyIndicesMap) {
                size_t keySubsampleSize = static_cast<size_t>(val.size() * subsamplePercent / 100.0);
                keySubsampleSizes[key] = keySubsampleSize;
                accumulatedSize += keySubsampleSize;
            }

            // Adjust for rounding errors
            while (accumulatedSize < totalSubsampleSize) {
                for (auto& [key, size] : keySubsampleSizes) {
                    if (accumulatedSize >= totalSubsampleSize) break;
                    size++;
                    accumulatedSize++;
                }
            }

            while (accumulatedSize > totalSubsampleSize) {
                for (auto& [key, size] : keySubsampleSizes) {
                    if (accumulatedSize <= totalSubsampleSize) break;
                    if (size > 0) {
                        size--;
                        accumulatedSize--;
                    }
                }
            }

            // Step 4: Shuffle and select the points for each key based on the calculated number
            for (const auto& [key, val] : selectedhierarchyIndicesMap) {
                auto indices = val;
                std::shuffle(indices.begin(), indices.end(), g);
                size_t keySubsampleSize = keySubsampleSizes[key];
                finalIndices.insert(finalIndices.end(), indices.begin(), indices.begin() + keySubsampleSize);
            }

        }
        else
        {

            for (const auto& [key, val] : selectedhierarchyIndicesMap) {
                auto indices = val;
                // Ensure good distribution by shuffling the indices and take subsamplePercent of the indices from each key and put into final indices
                if (!indices.empty()) {
                    std::shuffle(indices.begin(), indices.end(), g);
                    size_t subsampleSize = static_cast<size_t>(indices.size() * subsamplePercent / 100.0);
                    finalIndices.insert(finalIndices.end(), indices.begin(), indices.begin() + subsampleSize);
                }
            }

        }



        auto finalIndicesSize = finalIndices.size();
        qDebug() << "Subsampling process started please wait...";
        qDebug() << finalIndicesSize << " indices will be generated by subsampling.";
        std::map<QString, std::map<QString, std::pair<QColor, std::vector<int>>>> childrenClusterDatasets;
        std::map<QString, PointDataStructChild> childrenPointDatasets;
        std::map<QString, PointDataStructParent> parentPointDataset;
        PointDataStructParent parentPointDataValues;
        parentPointDataValues.numPoints = finalIndicesSize;
        parentPointDataValues.numDimensions = mainPointsNumofDims;
        parentPointDataValues.dimensionNames = mainPointsDimensions;
        std::vector<float> parentPointVector(finalIndicesSize * mainPointsNumofDims); ////change to float for ci build __bfloat16 to float
        qDebug() << "Parent point dataset created.";
        for (int i = 0; i < finalIndicesSize; i++) {
            for (int j = 0; j < mainPointsNumofDims; j++) {
                float value = mainPointsData->getValueAt(finalIndices[i] * mainPointsNumofDims + j);
                float bfloatValue = static_cast<float>(value); ////change to float for ci build __bfloat16 to float
                parentPointVector[i * mainPointsNumofDims + j] = bfloatValue;
            }
        }
        qDebug() << "Parent point dataset populated.";
        parentPointDataValues.pointVector = parentPointVector;
        parentPointDataset.insert(std::make_pair(mainPointsDatasetId, parentPointDataValues));
        auto children = mainPointsData->getChildren();
        qDebug() << "Number of children datasets: " << children.size();
        if (children.size() > 0)
        {
            for (auto child : children)
            {
                qDebug() << "Processing child dataset: " << child->getGuiName();
                QString idVal = child->getId();
                if (child->getDataType() == ClusterType)
                {
                    std::map<QString, std::pair<QColor, std::vector<int>>> clusterMap;
                    auto clusterData = mv::data().getDataset<Clusters>(idVal);
                    auto clusters = clusterData->getClusters();
                    for (auto& cluster : clusters)
                    {
                        auto clusterIndices = cluster.getIndices();
                        auto clusterColor = cluster.getColor();
                        auto clusterName = cluster.getName();
                        //map indices position that are present from finalIndices to the cluster indices
                        std::vector<int> clusterIndicesSelected;
                        for (int i = 0; i < finalIndicesSize; i++)
                        {
                            if (std::find(clusterIndices.begin(), clusterIndices.end(), finalIndices[i]) != clusterIndices.end())
                            {
                                clusterIndicesSelected.push_back(i);
                            }
                        }
                        clusterMap.insert(std::make_pair(clusterName, std::make_pair(clusterColor, clusterIndicesSelected)));


                    }
                    
                    childrenClusterDatasets.insert(std::make_pair(idVal, clusterMap));
                }
                else if (child->getDataType() == PointType)
                {
                    PointDataStructChild pointDataValues;
                    pointDataValues.numPoints = finalIndicesSize;
                    auto childData = mv::data().getDataset<Points>(idVal);
                    auto childNumofDims = childData->getNumDimensions();
                    pointDataValues.numDimensions = childNumofDims;
                    pointDataValues.dimensionNames = childData->getDimensionNames();
                    std::vector<float> pointVector(finalIndicesSize * mainPointsNumofDims);
                    
                    for (int i = 0; i < finalIndicesSize; i++)
                    {
                        for (int j = 0; j < childNumofDims; j++)
                        {
                            pointVector[i * childNumofDims + j] = childData->getValueAt(finalIndices[i] * childNumofDims + j);
                        }
                    }
                    pointDataValues.pointVector = pointVector;
                    childrenPointDatasets.insert(std::make_pair(idVal, pointDataValues));
                }
                qDebug() << "Child dataset processed.";
            }
        }
        
        qDebug() << "Number of children cluster datasets: " << childrenClusterDatasets.size();
        qDebug() << "Number of children point datasets: " << childrenPointDatasets.size();
        if (subsampleInplace) {
            qDebug() << "Replacing points inplace.";
            qDebug() << "Replacing parent point dataset inplace.";
            populatePointDataParent(mainPointsDatasetId, parentPointDataValues);
            qDebug() << "Replacing children point datasets inplace.";
            
            for (auto& childData : childrenPointDatasets)
            {
                QString dataId = childData.first;
                PointDataStructChild dataValues = childData.second;
                populatePointDataChild(dataId, dataValues);
            }
            qDebug() << "Replacing children cluster datasets inplace.";
            
            for (auto& childData : childrenClusterDatasets)
            {
                QString dataId = childData.first;
                std::map<QString, std::pair<QColor, std::vector<int>>> dataValues = childData.second;
                populateClusterData(dataId, dataValues);
            }
            qDebug() << "Inplace subsampling completed.";
        }
        else {
            qDebug() << "Creating new datasets for subsampled data.";
            //create parentPointDataset then children point datasets then childrencluster datasets
            qDebug() << "Creating parent point dataset.";
            auto parentDataset = mv::data().createDataset("Points", mainPointsDataset->getGuiName() + "_subsampled");
            events().notifyDatasetAdded(parentDataset);
            QString parentID = parentDataset->getId();
            qDebug() << "Populating parent point dataset.";
            populatePointDataParent(parentID, parentPointDataValues);
            qDebug() << "Creating children point datasets.";
            for (auto& childData : childrenPointDatasets)
            {
                QString dataId = childData.first;
                PointDataStructChild dataValues = childData.second;
                auto childDataset = mv::data().createDataset("Points", dataId + "_subsampled", parentDataset);
                events().notifyDatasetAdded(childDataset);
                QString childID = childDataset->getId();
                populatePointDataChild(childID, dataValues);
            }
            qDebug() << "Creating children cluster datasets.";
            for (auto& childData : childrenClusterDatasets)
            {
                QString dataId = childData.first;
                std::map<QString, std::pair<QColor, std::vector<int>>> dataValues = childData.second;
                auto childDataset = mv::data().createDataset("Clusters", dataId + "_subsampled", parentDataset);
                events().notifyDatasetAdded(childDataset);
                QString childID = childDataset->getId();
                populateClusterData(childID, dataValues);
            }
            qDebug() << "Subsampling completed.";
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        if (_subsamplingPopupMessage->isVisible())
        {
            _subsamplingPopupMessage->hide();
            QApplication::processEvents();
        }
        qDebug() << "Subsampling process: elapsed time:" << elapsed.count() << " seconds";
        _subsampleDataStart.setDisabled(false);
        };


    connect(&_subsampleDataStart, &TriggerAction::triggered, this, updateSubsampleDataStart);






    _mainPointsDataset.setDefaultWidgetFlags(DatasetPickerAction::WidgetFlag::ComboBox);
    _embeddingDataset.setDefaultWidgetFlags(DatasetPickerAction::WidgetFlag::ComboBox);
    _topHierarchyRelativeClusterCountInclusion.setDefaultWidgetFlags(OptionsAction::ComboBox || OptionsAction::File);
    _statusChangedAction.setDefaultWidgetFlags(StringAction::WidgetFlag::LineEdit);
    _hierarchyTopClusterDataset.setDefaultWidgetFlags(DatasetPickerAction::WidgetFlag::ComboBox);
    _hierarchyMiddleClusterDataset.setDefaultWidgetFlags(DatasetPickerAction::WidgetFlag::ComboBox);
    _hierarchyBottomClusterDataset.setDefaultWidgetFlags(DatasetPickerAction::WidgetFlag::ComboBox);
    _selectedClusterNamesVariant.setDefaultWidgetFlags(StringAction::WidgetFlag::LineEdit);
    //_filteredGeneNamesVariant.setDefaultWidgetFlags(StringAction::WidgetFlag::LineEdit);
    //_updateButtonForGeneFiltering.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);
    _speciesNamesDataset.setDefaultWidgetFlags(DatasetPickerAction::WidgetFlag::ComboBox);
    //_topNGenesFilter.setDefaultWidgetFlags(IntegralAction::WidgetFlag::SpinBox | IntegralAction::WidgetFlag::Slider);
    _referenceTreeDataset.setDefaultWidgetFlags(DatasetPickerAction::WidgetFlag::ComboBox);
    _filterEditTreeDataset.setDefaultWidgetFlags(DatasetPickerAction::WidgetFlag::ComboBox);
    _createPointSelectTree.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);
    _geneNamesConnection.setDefaultWidgetFlags(StringAction::WidgetFlag::LineEdit);
    _clusterOrder.setDefaultWidgetFlags(StringAction::WidgetFlag::LineEdit);
    _rightClickedCluster.setDefaultWidgetFlags(StringAction::WidgetFlag::LineEdit);
    _clearRightClickedCluster.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);
    _topSelectedHierarchyStatus.setDefaultWidgetFlags(StringAction::WidgetFlag::LineEdit);
    _removeTableSelection.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);
    _subsampleDataStart.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);
    _subsampleByLevel.setDefaultWidgetFlags(OptionsAction::ComboBox);
    _subsamplePercentValue.setDefaultWidgetFlags(DecimalAction::WidgetFlag::SpinBox | DecimalAction::WidgetFlag::Slider);
    _subsampleInplace.setDefaultWidgetFlags(ToggleAction::CheckBox );
    _generateTreeDataFilesPerClusterStart.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);
    //_treeSimilarity.setDefaultWidgetFlags(DecimalAction::WidgetFlag::SpinBox | DecimalAction::WidgetFlag::Slider);
    _statusChangedAction.setString("M");
}


void SettingsAction::populatePointDataParent(QString& datasetId, PointDataStructParent& pointDataValues)
{
    auto pointDataset = mv::data().getDataset<Points>(datasetId);

    if (pointDataset.isValid())
    {
        pointDataset->setSelectionIndices({});
        if (pointDataValues.pointVector.size() > 0 && pointDataValues.numPoints > 0 && pointDataValues.numDimensions > 0) {

            pointDataset->setData(pointDataValues.pointVector.data(), pointDataValues.numPoints, pointDataValues.numDimensions);
            pointDataset->setDimensionNames(pointDataValues.dimensionNames);
            mv::events().notifyDatasetDataChanged(pointDataset);
        }
    }
}

void SettingsAction::populatePointDataChild(QString& datasetId, PointDataStructChild& pointDataValues)
{
    auto pointDataset = mv::data().getDataset<Points>(datasetId);

    if (pointDataset.isValid())
    {
        pointDataset->setSelectionIndices({});
        if (pointDataValues.pointVector.size() > 0 && pointDataValues.numPoints > 0 && pointDataValues.numDimensions > 0) {

            pointDataset->setData(pointDataValues.pointVector.data(), pointDataValues.numPoints, pointDataValues.numDimensions);
            pointDataset->setDimensionNames(pointDataValues.dimensionNames);
            mv::events().notifyDatasetDataChanged(pointDataset);
        }
    }
}


void SettingsAction::populateClusterData(QString& datasetId, std::map<QString, std::pair<QColor, std::vector<int>>>& clusterMap)
{

    auto colorDataset = mv::data().getDataset<Clusters>(datasetId);
    if (colorDataset.isValid())
    {
        colorDataset->getClusters() = QVector<Cluster>();
        for (const auto& pair : clusterMap)
        {
            QString clusterName = pair.first;
            std::pair<QColor, std::vector<int>> value = pair.second;
            QColor clusterColor = value.first;
            std::vector<std::uint32_t> clusterIndices(value.second.begin(), value.second.end());

            if (clusterIndices.size() > 0)
            {
                Cluster clusterValue;
                clusterValue.setName(clusterName);
                clusterValue.setColor(clusterColor);
                clusterValue.setIndices(clusterIndices);
                colorDataset->addCluster(clusterValue);
            }
        }

        mv::events().notifyDatasetDataChanged(colorDataset);
    }


}
void SettingsAction::updateButtonTriggeredForTreeCreation()
{

}

SettingsAction::Widget::Widget(QWidget* parent, SettingsAction* SettingsAction) :
    WidgetActionWidget(parent, SettingsAction)
{ }

SettingsAction::OptionSelectionAction::Widget::Widget(QWidget* parent, OptionSelectionAction* optionSelectionAction) :
    WidgetActionWidget(parent, optionSelectionAction)
{ }

inline SettingsAction::OptionSelectionAction::OptionSelectionAction(SettingsAction& SettingsAction) :
    GroupAction(nullptr, "XSCGeneDetectPluginOptionSelectionAction"),
    _settingsAction(SettingsAction)
{
    setText("Options");
    setIcon(mv::util::StyledIcon("wrench"));
    //addAction(&_settingsAction.getMainPointsDataset());
    //addAction(&_settingsAction.getHierarchyTopClusterDataset());
    //addAction(&_settingsAction.getHierarchyMiddleClusterDataset());
    //addAction(&_settingsAction.getHierarchyBottomClusterDataset());
    //addAction(&_settingsAction.getSpeciesNamesDataset());
   ///addAction(&_settingsAction.getReferenceTreeDataset());
   // addAction(&_settingsAction.getSelectedClusterNames());
    //addAction(&_settingsAction.getFilteredGeneNames());
    //addAction(&_settingsAction.getTopNGenesFilter());
   // addAction(&_settingsAction.getUpdateButtonForGeneFiltering());
   
}


void SettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);

    _mainPointsDataset.fromParentVariantMap(variantMap);
    _embeddingDataset.fromParentVariantMap(variantMap);
    _topHierarchyRelativeClusterCountInclusion.fromParentVariantMap(variantMap);
    _statusChangedAction.fromParentVariantMap(variantMap);
    _hierarchyTopClusterDataset.fromParentVariantMap(variantMap);
    _hierarchyMiddleClusterDataset.fromParentVariantMap(variantMap);
    _hierarchyBottomClusterDataset.fromParentVariantMap(variantMap);
    _selectedClusterNamesVariant.fromParentVariantMap(variantMap);
    //_filteredGeneNamesVariant.fromParentVariantMap(variantMap);
    //_updateButtonForGeneFiltering.fromParentVariantMap(variantMap);
    //_topNGenesFilter.fromParentVariantMap(variantMap);
    _speciesNamesDataset.fromParentVariantMap(variantMap);
    _filterEditTreeDataset.fromParentVariantMap(variantMap);
    _createPointSelectTree.fromParentVariantMap(variantMap);
    _referenceTreeDataset.fromParentVariantMap(variantMap);
    _geneNamesConnection.fromParentVariantMap(variantMap);
    _removeTableSelection.fromParentVariantMap(variantMap);
    _clusterOrder.fromParentVariantMap(variantMap);
    _rightClickedCluster.fromParentVariantMap(variantMap);
    _clearRightClickedCluster.fromParentVariantMap(variantMap);
    _topSelectedHierarchyStatus.fromParentVariantMap(variantMap);
    //_treeSimilarity.fromParentVariantMap(variantMap);
}

QVariantMap SettingsAction::toVariantMap() const
{
    QVariantMap variantMap = WidgetAction::toVariantMap();

    _mainPointsDataset.insertIntoVariantMap(variantMap);
    _embeddingDataset.insertIntoVariantMap(variantMap);
    _topHierarchyRelativeClusterCountInclusion.insertIntoVariantMap(variantMap);
    _statusChangedAction.insertIntoVariantMap(variantMap);
    _hierarchyTopClusterDataset.insertIntoVariantMap(variantMap);
    _hierarchyMiddleClusterDataset.insertIntoVariantMap(variantMap);
    _hierarchyBottomClusterDataset.insertIntoVariantMap(variantMap);
    _selectedClusterNamesVariant.insertIntoVariantMap(variantMap);
    //_filteredGeneNamesVariant.insertIntoVariantMap(variantMap);
    //_updateButtonForGeneFiltering.insertIntoVariantMap(variantMap);
    //_topNGenesFilter.insertIntoVariantMap(variantMap);
    _speciesNamesDataset.insertIntoVariantMap(variantMap);
    _referenceTreeDataset.insertIntoVariantMap(variantMap);
    _geneNamesConnection.insertIntoVariantMap(variantMap);
    _filterEditTreeDataset.insertIntoVariantMap(variantMap);
    _createPointSelectTree.insertIntoVariantMap(variantMap);
    _removeTableSelection.insertIntoVariantMap(variantMap);
    _clusterOrder.insertIntoVariantMap(variantMap);
    _rightClickedCluster.insertIntoVariantMap(variantMap);
    _clearRightClickedCluster.insertIntoVariantMap(variantMap);
    _topSelectedHierarchyStatus.insertIntoVariantMap(variantMap);
    //_treeSimilarity.insertIntoVariantMap(variantMap);
    return variantMap;
}
