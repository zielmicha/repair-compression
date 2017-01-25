#include <string>
#include <fstream>
#include <streambuf>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>


using namespace std;

int main () {
    int n = 8;
    std::string str((std::istreambuf_iterator<char>(cin)),
                    std::istreambuf_iterator<char>());

    unordered_map<string, int> counts;
    for (int i=0; i < str.size() - n; i ++) {
        string sub = str.substr(i, n);
        counts[sub] ++;
    }

    vector<pair<int, string> > best;
    for (auto p : counts) best.push_back({p.second, p.first});
    sort(best.begin(), best.end());
    reverse(best.begin(), best.end());

    for (int i=99; i >= 0; i --) {
        auto p = best[i];
        cout << p.first << " " << p.second << endl;
    }
}
