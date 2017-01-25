#include <string>
#include <fstream>
#include <streambuf>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <list>
#include <queue>
#include <cassert>

using namespace std;

using Sym = int;

struct pairhash {
  template <typename T, typename U>
  std::size_t operator()(const std::pair<T, U>& x) const {
    return std::hash<T>()(x.first) * 27231 + std::hash<U>()(x.second);
  }
};

struct Entry {
    Sym sym;

    int prevu;
    int nextu;

    int prevp;
    int nextp;
};

template <int n> int& prevg(Entry& e) { return n==0?e.prevu:e.prevp; }
template <int n> int& nextg(Entry& e) { return n==0?e.nextu:e.nextp; }

vector<pair<Sym, Sym> > pairs;
vector<Entry> text;
unordered_map<pair<Sym, Sym>, int, pairhash> last_entry;
priority_queue<pair<int, pair<Sym, Sym> > > freq_queue;
unordered_map<pair<Sym, Sym>, int, pairhash> real_freq;
unordered_map<Sym, pair<Sym, Sym> > sym_meaning;

string symstr(Sym s) {
    if ((-s) == (int)'\n') return "'\\n'";
    if (s < 0) return string("'") + ((char)-s) + "'";
    else return "s" + to_string(s);
}

void print_s() {
    cerr << "text: ";
    int item = 0;
    while (item != -1) {
        cerr << symstr(text[item].sym) << " ";
        item = text[item].nextu;
    }
    cerr << endl;
}

void print_pairs() {
    for (auto it : last_entry) {
        int item = it.second;
        if (item == -1) continue;
        cerr << "pair " << symstr(it.first.first) << " " << symstr(it.first.second) << ": ";
        assert(item == -1 || text[item].nextp == -1);
        while(item != -1) {
            cerr << item << " ";
            item = text[item].prevp;
        }
        cerr << endl;
    }
}

const Sym TERMSYM = 0;

void remove_u(int index) {
    text[text[index].prevu].nextu = text[index].nextu;
    text[text[index].nextu].prevu = text[index].prevu;
    text[index].prevu = -666;
    text[index].nextu = -666;
}

void remove_p(int index, pair<Sym, Sym> p) {
    if (text[index].prevp != -1) {
        text[text[index].prevp].nextp = text[index].nextp;
    }
    if (text[index].nextp != -1) {
        text[text[index].nextp].prevp = text[index].prevp;
    } else {
        last_entry[p] = text[index].prevp;
    }
    text[index].prevp = -666;
    text[index].nextp = -666;

    int v = (-- real_freq[p]);
    freq_queue.push({v, p});
}

void insert_p(int index, pair<Sym, Sym> p) {
    auto it = last_entry.find(p);
    text[index].nextp = -1;

    assert(text[index].sym == p.first);
    assert(text[text[index].nextu].sym == p.second);

    if (it == last_entry.end() || it->second == -1) {
        text[index].prevp = -1;
        last_entry[p] = index;
    } else {
        text[index].prevp = it->second;
        text[it->second].nextp = index;
        last_entry[p] = index;
    }

    int v = (++ real_freq[p]);
    freq_queue.push({v, p});
}

int main () {
    std::string str((std::istreambuf_iterator<char>(cin)),
                    std::istreambuf_iterator<char>());

    text.push_back({TERMSYM, -1, 1, -1, -1});
    for (int i=0; i < str.size(); i ++) {
        char ch = str[i];
        text.push_back(Entry{-(int)ch,
                    i, i + 2, -1, -1});
    }
    text.push_back({TERMSYM, (int) str.size(), -1, -1, -1});

    const Sym sym_offset = 1;
    Sym next_sym = sym_offset;

    {
        // initialization
        for (int i=0; i < text.size() - 1; i ++) {
            pair<Sym, Sym> p (text[i].sym, text[i+1].sym);
            real_freq[p] ++;
            auto it = last_entry.find(p);
            if (it == last_entry.end()) {
                last_entry[p] = i;
            } else {
                int prev = it->second;
                text[i].prevp = prev;
                text[prev].nextp = i;
                it->second = i;
            }
        }

        for (auto p : real_freq) {
            freq_queue.push(make_pair(p.second, p.first));
        }
    }

    //print_s();
    //print_pairs();
    int text_size = text.size();

    while (!freq_queue.empty()) {
        auto item = freq_queue.top();
        freq_queue.pop();
        int freq = item.first;
        pair<Sym, Sym> p = item.second;
        if (real_freq[p] != freq || freq == 0) continue;
        if (freq == 1) break;

        if (p.first == TERMSYM || p.second == TERMSYM) continue;

        // replace all xaby with xAy
        vector<int> items;
        int pitem = last_entry[p];
        while (pitem != -1) {
            items.push_back(pitem);
            pitem = text[pitem].prevp;
        }
        sort(items.begin(), items.end());

        Sym my_sym = next_sym;
        next_sym ++;
        sym_meaning[my_sym] = p;

        int last_replaced = -999;
        // xaby
        for (int index1 : items) {
            if (last_replaced == index1) continue;
            int index0 = text[index1].prevu;
            int index2 = text[index1].nextu;
            int index3 = text[index2].nextu;
            last_replaced = index2;

            Sym s_x = text[index0].sym;
            Sym s_a = text[index1].sym;
            Sym s_b = text[index2].sym;
            Sym s_y = text[index3].sym;

            assert (text[index1].sym == p.first && text[index2].sym == p.second);

            remove_p(index1, {s_a, s_b});
            remove_u(index2);
            remove_p(index0, {s_x, s_a});
            remove_p(index2, {s_b, s_y});

            text[index1].sym = my_sym;
            insert_p(index0, {s_x, my_sym});
            insert_p(index1, {my_sym, s_y});
            text_size --;
        }
    }

    cerr << "symbol count: " << sym_meaning.size() << ", text size: " << text_size << endl;
    for (int i=0; i < sym_meaning.size(); i ++) {
        auto p = sym_meaning[i + sym_offset];
        cout << p.first << " " << p.second << endl;
    }
}
