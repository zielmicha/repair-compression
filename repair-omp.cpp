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
#include <omp.h>
#include <mutex>
#include <boost/lockfree/queue.hpp>

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

template <typename T>
struct squeue {
    using R = pair<int, T>;

    struct Entry {
        T item;
        int count;
        typename list<Entry*>::iterator iter;
    };

    struct Level {
        list<Entry*> entries;
    };

    vector<Level> levels;
    int max_level;
    unordered_map<T, Entry, pairhash> entries;

    void init(int n) {}

    void modify(T p, int dir) {
        auto it = entries.find(p);
        if (it == entries.end()) {
            assert(dir == 1);
            Entry& e = entries[p];
            e.item = p;
            e.count = 1;
            add_entry(&e);
        } else {
            Entry& e = it->second;
            levels[e.count].entries.erase(e.iter);
            e.count += dir;
            add_entry(&e);
        }
    }

    void decrease(T p) {
        modify(p, -1);
    }

    void increase(T p) {
        modify(p, +1);
    }

    void add_entry(Entry* e) {
        Level& l = levels[e->count];
        e->iter = l.entries.insert(l.entries.begin(), e);
    }

    void insert(unordered_map<T, int, pairhash> data) {
        int max_count = 100000;
        for (auto it : data) {
            Entry& e = entries[it.first];
            e.item = it.first;
            e.count = it.second;
            max_count = max(max_count, e.count);
        }
        cerr << max_count << endl;

        levels.resize(max_count + 1);
        max_level = max_count;
        for (auto& it : entries) {
            add_entry(&it.second);
        }
    }

    void norm() {
        while (max_level >= 0 && levels[max_level].entries.empty()) {
            max_level --;
        }
    }

    bool empty() {
        norm();
        return max_level == -1;
    }

    pair<int, T> top() {
        norm();
        Entry* e = levels[max_level].entries.back();
        return {e->count, e->item};
    }

    void pop() {
        Entry* e = levels[max_level].entries.back();
        entries.erase(e->item);
        levels[max_level].entries.pop_back();
    }
};

template <int n> int& prevg(Entry& e) { return n==0?e.prevu:e.prevp; }
template <int n> int& nextg(Entry& e) { return n==0?e.nextu:e.nextp; }

vector<pair<Sym, Sym> > pairs;

extern vector<Entry> text;
extern unordered_map<pair<Sym, Sym>, int, pairhash> last_entry;
#pragma omp threadprivate(text, last_entry)
vector<Entry> text;
unordered_map<pair<Sym, Sym>, int, pairhash> last_entry;

vector<squeue<pair<Sym, Sym> > > freq_queue;
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
}

int main () {
    const int num_threads = NUMTHREADS;
    omp_set_num_threads(num_threads);
    std::string str((std::istreambuf_iterator<char>(cin)),
                    std::istreambuf_iterator<char>());

    static int thoffset, thsize;
    #pragma omp threadprivate(thoffset, thsize)

    #pragma omp parallel num_threads(NUMTHREADS)
    {
        int thid = omp_get_thread_num();
        int thnum = num_threads;
        thsize = str.size() / thnum;
        thoffset = thsize * thid;
        if (thid + 1 == thnum) thsize = str.size() - thoffset;

        text.push_back({TERMSYM, -1, 1, -1, -1});
        for (int i=0; i < thsize; i ++) {
            char ch = str[thoffset + i];
            text.push_back(Entry{-(int)ch,
                        i, i + 2, -1, -1});
        }
        text.push_back({TERMSYM, (int) str.size(), -1, -1, -1});
    }

    const Sym sym_offset = 1;
    Sym next_sym = sym_offset;

    unordered_map<pair<Sym, Sym>, int, pairhash> real_freq_init;
    #pragma omp parallel num_threads(NUMTHREADS)
    {
        unordered_map<pair<Sym, Sym>, int, pairhash> real_freq_local;

        // initialization
        for (int i=0; i < text.size() - 1; i ++) {
            pair<Sym, Sym> p (text[i].sym, text[i+1].sym);
            real_freq_local[p] ++;
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

        #pragma omp critical
        for (auto p : real_freq_local) {
            real_freq_init[p.first] += p.second;
        }
    }

    struct Ev {
        int dir;
        int a,b;

        int thid() { return pairhash()(make_pair(a, b)) % num_threads; }
    };

    freq_queue.resize(num_threads);
    vector<unordered_map<pair<Sym, Sym>, int, pairhash> > real_freq_threads (num_threads);
    for (auto p : real_freq_init) {
        int target_thid = (Ev{1, p.first.first, p.first.second}).thid();
        real_freq_threads[target_thid][p.first] = p.second;
    }
    for (int i=0; i < num_threads; i ++)
        freq_queue[i].insert(real_freq_threads[i]);

    //print_s();
    //print_pairs();
    int text_size = text.size();
    vector<boost::lockfree::queue<Ev>* > queues;
    for (int i=0; i < num_threads; i ++) {
        queues.emplace_back(new boost::lockfree::queue<Ev>(10000));
    }
    //cerr <<"qsize: " << queues.size() << " " << num_threads << endl;

    auto adjust_freq = [&](Ev ev) {
        //cerr << ev.thid() << ":" << queues.size() << endl;
        bool ok = queues[ev.thid()]->push(ev);
        assert(ok);
    };

    auto pop_item = [&]() -> pair<int, pair<Sym, Sym> > {
        pair<int, pair<Sym, Sym> > item;
        bool item_found = false;
        for (auto& q : freq_queue) {
            if (q.empty()) continue;
            if (!item_found || q.top().first > item.first) {
                item = q.top();
                item_found = true;
            }
        }
        if (!item_found) return make_pair(-2, make_pair(0, 0));
        return item;
    };

    while (true) {
        vector<pair<Sym, pair<int, pair<Sym, Sym> > > > titems;
        for (int i=0; i < 1; i ++) {
            auto item = pop_item();
            if (item.first == -2) continue;
            if (item.first == 0) continue;
            if (item.first == 1) continue;
            auto p = item.second;
            if (p.first == TERMSYM || p.second == TERMSYM) continue;

            const Sym my_sym = next_sym;
            next_sym ++;
            sym_meaning[my_sym] = item.second;

            titems.push_back(make_pair(my_sym, item));
            if (item.first > 50) break;
        }
        //cerr << items.size() << endl;
        if (titems.empty()) break;

        //cerr << "info: " << p.first << " " << p.second << endl;

#pragma omp parallel num_threads(NUMTHREADS)
        {
            for (auto item : titems) {
                const Sym my_sym = item.first;
                const int freq = item.second.first;
                const pair<Sym, Sym> p = item.second.second;

                //#pragma omp critical
                //std::cerr << "begin: " << text.size() << " " << text[1].sym << endl;

                // replace all xaby with xAy
                vector<int> items;
                auto pitemit = last_entry.find(p);
                int pitem = pitemit == last_entry.end() ? -1 : pitemit->second;
                while (pitem != -1) {
                    items.push_back(pitem);
                    pitem = text[pitem].prevp;
                }
                sort(items.begin(), items.end());
                //cerr << omp_get_thread_num() << " " << items.size() << endl;
                //cerr << freq << " " << items.size() << endl;

                int last_replaced = -999;
                for (int index1 : items) {
                    if (last_replaced == index1) continue;
                    int index0 = text[index1].prevu;
                    int index2 = text[index1].nextu;
                    int index3 = text[index2].nextu;
                    last_replaced = index2;

                    //cerr << "index0: " << index0 << " index1: " << index1 << endl;
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

                    adjust_freq({-1, s_a, s_b});
                    adjust_freq({-1, s_x, s_a});
                    adjust_freq({-1, s_b, s_y});
                    adjust_freq({1, s_x, my_sym});
                    adjust_freq({1, my_sym, s_y});
                }
            }
#pragma omp barrier
            auto myq = queues[omp_get_thread_num()];
            Ev ev;
            while (myq->pop(ev)) {
                freq_queue[omp_get_thread_num()].modify({ev.a, ev.b}, ev.dir);
            }
        }
    }

    #pragma omp parallel num_threads(NUMTHREADS)
    #pragma omp critical
    cerr << "symbol count: " << sym_meaning.size() << ", text size: " << text_size << endl;

    // print result only for first thread
    /*for (int i=0; i < sym_meaning.size(); i ++) {
        auto p = sym_meaning[i + sym_offset];
        cout << p.first << " " << p.second << endl;
        }*/
}
