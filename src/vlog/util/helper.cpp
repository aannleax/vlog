#include <vector>
#include <stack>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include "../../../include/vlog/helper.h"

using namespace std;

void getRandomTupleIndexes(uint64_t m, uint64_t n, vector<int>& indexes) {
    if (m >= n) {
        for (uint64_t i = 0; i < n; ++i) {
            indexes[i] =i;
        }
        return;
    }
    srand(time(0));
    for (uint64_t i = 0; i < m; ++i) {
        uint64_t r;
        do {
            r = rand() % n;
        } while(std::find(indexes.begin(), indexes.end(), r) != indexes.end());
        indexes[i] = r;
    }
}

// http://www.cplusplus.com/forum/general/125094/
std::vector<std::string> split( std::string str, char sep) {
    std::vector<std::string> ret ;

    std::istringstream stm(str) ;
    std::string token ;
    while( getline( stm, token, sep ) ) ret.push_back(token) ;

    return ret ;
}

std::string stringJoin(vector<string>& vec, char delimiter) {
    string result;
    for (int i = 0; i < vec.size(); ++i) {
        result += vec[i];
        if (i != vec.size()-1) {
            result += delimiter;
        }
    }
    return result;
}

vector<string> rsplit(string logLine, char sep, int maxSplits) {
    vector<string> tokens;
    stack<int> spaceIndexes;
    size_t index = string::npos;
    int cntIndexes = 0;
    while (cntIndexes < maxSplits) {
        index = logLine.find_last_of(sep, index);
        if (index == string::npos) {
            break;
        }
        index--;
        spaceIndexes.push(index);
        cntIndexes++;
    }

    int startIndex = 0;
    while(!spaceIndexes.empty()) {
        int index = spaceIndexes.top();
        spaceIndexes.pop();
        tokens.push_back(logLine.substr(startIndex, (index - startIndex)+1));
        startIndex = index+2;
    }
    tokens.push_back(logLine.substr(startIndex, (index - startIndex)+1));
    return tokens;
}
