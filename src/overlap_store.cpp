#include "overlap_store.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cassert>

#include "read_store.hpp"
#include "./utils/logger.hpp"


namespace fsa {

struct IterCigar {

    IterCigar(const std::string &c) : cigar(c) {}
    bool Next(size_t &n, char &t) {
        n = 0;
        t = 0;
        if (index < cigar.size()) {
            size_t start = index;
            for (; index<cigar.size(); ++index) {
                if (!::isdigit(cigar[index])) break;
            }
            n = stoul(cigar.substr(start, index-start));
    
            if (index < cigar.size()) {
                t = cigar[index++];
            }
        }
        return n != 0 && t != 0;
    }

    const std::string &cigar;
    size_t index {0};
};



std::vector<std::string> SplitString(const std::string &str, const std::string sep) {
    std::vector<std::string> substrs;

    std::string::size_type begin = str.find_first_not_of(sep);
    while (begin != std::string::npos) {
        std::string::size_type end = str.find(sep, begin);
        if (end != std::string::npos) {
            substrs.push_back(str.substr(begin, end - begin));
            begin = end + sep.length();
        }
        else {
            substrs.push_back(str.substr(begin));
            begin = end;
        }
    }
    return substrs;
}

std::string OverlapStore::DetectFileType(const std::string &fname) {
    if (fname.size() >= 3 && fname.substr(fname.size()-3) == ".m4") {
        return "m4";
    } else if (fname.size() >= 6 && fname.substr(fname.size()-6) == ".m4.gz") {
        return "m4.gz";
    } else if (fname.size() >= 4 && fname.substr(fname.size()-4) == ".m4a") {
        return "m4a";
    } else if (fname.size() >= 7 && fname.substr(fname.size()-7) == ".m4a.gz") {
        return "m4a.gz";
    } else if (fname.size() >= 4 && fname.substr(fname.size()-4) == ".paf") {
        return "paf";
    } else if (fname.size() >= 7 && fname.substr(fname.size()-7) == ".paf.gz") {
        return "paf.gz";
    } else if (fname.size() >= 4 && fname.substr(fname.size()-7) == ".txt") {
        return "txt";
    } else {
        auto i = fname.find_last_of('.');
        return fname.substr(i == fname.npos ? 0 : i+1);
    }
}

bool OverlapStore::FromM4Line(const std::string &line, Overlap& o, StringPool::NameId& ni) {

    std::vector<std::string> items = SplitStringBySpace(line);

    if (items.size() >= 12) {

        // M4?????????Id??????read???fasta??????????????????
        o.a_.id = atoi(items[0].c_str()) - 1;
        o.b_.id = atoi(items[1].c_str()) - 1;

        o.identity_ = atof(items[2].c_str());

        o.a_.strand = atoi(items[4].c_str());
        o.a_.start = atoi(items[5].c_str());
        o.a_.end = atoi(items[6].c_str());
        o.a_.len = atoi(items[7].c_str());

        o.b_.strand = atoi(items[8].c_str());
        o.b_.start = atoi(items[9].c_str());
        o.b_.end = atoi(items[10].c_str());
        o.b_.len = atoi(items[11].c_str());

        // ??????strand?????????a.strand = 0, ?????????b????????????a
        o.b_.strand = o.a_.strand == o.b_.strand ? 0 : 1;
        o.a_.strand = 0;

        return true;
    }
    else {
        return false;
    }
}


bool OverlapStore::FromM4aLine(const std::string &line, Overlap& o, StringPool::NameId& ni) {

    std::vector<std::string> items = SplitStringBySpace(line);

    if (items.size() >= 12) {

        o.a_.id = ni.GetIdByName(items[0]);
        o.b_.id = ni.GetIdByName(items[1]);
        if (o.a_.id == -1 || o.b_.id == -1) return false;

        o.identity_ = atof(items[2].c_str());

        o.a_.strand = atoi(items[4].c_str());
        o.a_.start = atoi(items[5].c_str());
        o.a_.end = atoi(items[6].c_str());
        o.a_.len = atoi(items[7].c_str());

        o.b_.strand = atoi(items[8].c_str());
        o.b_.start = atoi(items[9].c_str());
        o.b_.end = atoi(items[10].c_str());
        o.b_.len = atoi(items[11].c_str());

        // cigar
        if (items.size() >= 13) {
            auto iter = IterCigar(items[12]);
            size_t n = 0; char t = '\0';

            while (iter.Next(n, t)) {
                o.detail_.push_back({int(n), t});
            }
        }
        return true;
    }
    else {
        return false;
    }
}


bool OverlapStore::FromPafLine(const std::string &line, Overlap& o, StringPool::NameId& ni) {
    std::vector<std::string> items = SplitStringBySpace(line);

    if (items.size() >= 12) {
        // query_name, query_length, query_start, query_end, 
        // relative_strand, 
        // target_name, target_lenght, target_start, target_end, 
        // number_residue_matches, alignment_block_length, mapping_quality


        // query_name, query_length, query_start, query_end, 
        o.a_.id = ni.GetIdByName(items[0]);
        o.a_.len = atoi(items[1].c_str());
        o.a_.start = atoi(items[2].c_str());
        o.a_.end = atoi(items[3].c_str());
        
        // relative_strand, 
        o.a_.strand = items[4] == "+" ? 0 : 1;
        o.b_.strand = 0;
        
        // target_name, target_lenght, target_start, target_end, 
        o.b_.id = ni.GetIdByName(items[5]);
        o.b_.len = atoi(items[6].c_str());
        o.b_.start = atoi(items[7].c_str());
        o.b_.end = atoi(items[8].c_str());

        if (o.a_.id == -1 || o.b_.id == -1) return false;

        // number_residue_matches, alignment_block_length, mapping_quality
        auto match = std::stoi(items[9]);
        auto len = std::stoi(items[10]);
        o.identity_ = match*100.0 / len;

        int rep = 0;
        for (size_t i=12; i<items.size(); ++i) {
            if (items[i].size() >= 5 && items[i].compare(0, 5, "cg:Z:") == 0) {
                auto cigar = items[i].substr(5);
                auto iter = IterCigar(cigar);
                size_t n = 0; char t = '\0';

                while (iter.Next(n, t)) {
                    o.detail_.push_back({int(n), t});
                }
            }
            else if (items[i].size() >= 5 && items[i].compare(0, 5, "rl:i:") == 0) {
                rep = std::stoi(items[i].substr(5));
            }
        }

        if (o.detail_.size() == 0 && rep != 0) {
            o.identity_ = ((rep*1.0 / o.a_.len) * (o.a_.end - o.a_.start - match) + match ) * 100.0 / len;
        }

        return true;
    } else {
        return false;
    }
}

bool OverlapStore::FromPafLineEx(const std::string &line, Overlap& o, StringPool::NameId& ni, int &replen) {
    std::vector<std::string> items = SplitStringBySpace(line);
    replen = 0;
    if (items.size() >= 12) {
        // query_name, query_length, query_start, query_end, 
        // relative_strand, 
        // target_name, target_lenght, target_start, target_end, 
        // number_residue_matches, alignment_block_length, mapping_quality


        // query_name, query_length, query_start, query_end, 
        o.a_.id = ni.GetIdByName(items[0]);
        o.a_.len = atoi(items[1].c_str());
        o.a_.start = atoi(items[2].c_str());
        o.a_.end = atoi(items[3].c_str());
        
        // relative_strand, 
        o.a_.strand = items[4] == "+" ? 0 : 1;
        o.b_.strand = 0;
        
        // target_name, target_lenght, target_start, target_end, 
        o.b_.id = ni.GetIdByName(items[5]);
        o.b_.len = atoi(items[6].c_str());
        o.b_.start = atoi(items[7].c_str());
        o.b_.end = atoi(items[8].c_str());
        if (o.a_.id == -1 || o.b_.id == -1) return false;

        // number_residue_matches, alignment_block_length, mapping_quality
        
        auto match = std::stoi(items[9]);
        auto len = std::stoi(items[10]);
        o.identity_ = match*100.0 / len;

        int rep = 0;
        for (size_t i=12; i<items.size(); ++i) {
            if (items[i].size() >= 5 && items[i].compare(0, 5, "cg:Z:") == 0) {
                auto cigar = items[i].substr(5);
                auto iter = IterCigar(cigar);
                size_t n = 0; char t = '\0';

                while (iter.Next(n, t)) {
                    o.detail_.push_back({int(n), t});
                }
            }
            else if (items[i].size() >= 5 && items[i].compare(0, 5, "rl:i:") == 0) {
                rep = std::stoi(items[i].substr(5));
            }
        }

        if (o.detail_.size() == 0 && rep != 0) {
            o.identity_ = ((rep*1.0 / o.a_.len) * (o.a_.end - o.a_.start - match) + match ) * 100.0 / len;
        }
        replen = rep;
        return true;
    } else {
        return false;
    }
}

 std::array<Seq::Id, 2> OverlapStore::GetReadIdRange() const { 
    std::array<Seq::Id, 2> range = {0, string_pool_.Size() }; 
    if (range[1] == 0) {
        auto cmp_range = [&range](Seq::Id i) {
            if (i < range[0]) range[0] = i;
            else if (i >= range[1]) range[1] = i+1;
        };
        
        for (size_t i=0; i < Size(); ++i) {
            auto &o = Get(i);

            cmp_range(o.a_.id);
            cmp_range(o.b_.id);
            
        }
    }

    return range;
}

std::unordered_map<int, std::unordered_map<int, Overlap*>> OverlapStore::Group() {
    std::unordered_map<int, std::unordered_map<int, Overlap*>> groups;

    for (size_t i=0; i < Size(); ++i) {
        auto &o = Get(i);

        auto add_overlap = [&](int a, int b, Overlap& o) {
            auto iter = groups.find(a);
            if (iter != groups.end()) {
                assert(iter->second.find(b) == iter->second.end()); // ????????????????????????overlap
                iter->second[b] = &o;
            }
            else {
                auto iter = groups.insert(std::make_pair(a, std::unordered_map<int, Overlap*>()));
                iter.first->second[b] = &o;
            }
        };

        add_overlap(o.a_.id, o.b_.id, o);
        add_overlap(o.b_.id, o.a_.id, o);
        
    }

    return groups;
}

std::unordered_map<int, std::unordered_map<int, const Overlap*>> OverlapStore::Group(bool(*better)(const Overlap& a, const Overlap &b)) const {
    std::unordered_map<int, std::unordered_map<int, const Overlap*>> groups;

    for (size_t i=0; i < Size(); ++i) {
        auto &o = Get(i);

        auto add_overlap = [&](int a, int b, const Overlap& o) {
            auto iter = groups.find(a);
            if (iter != groups.end()) {
                auto itit = iter->second.find(b);
                if (itit == iter->second.end()) {
                    iter->second.insert(std::make_pair(b, &o));
                } else {
                    if (better(o, *itit->second)) {
                        itit->second = &o;
                    }
                }
            }
            else {
                auto iter = groups.insert(std::make_pair(a, std::unordered_map<int, const Overlap*>()));
                iter.first->second[b] = &o;
            }
        };

        add_overlap(o.a_.id, o.b_.id, o);
        add_overlap(o.b_.id, o.a_.id, o);
        
    }

    return groups;
}

std::unordered_map<int, std::unordered_map<int, Overlap*>> OverlapStore::GroupTarget(bool(*better)(const Overlap* a, const Overlap *b)) {
    std::unordered_map<int, std::unordered_map<int, Overlap*>> groups;

    for (size_t i=0; i < Size(); ++i) {
        auto &o = Get(i);

        auto add_overlap = [&](int a, int b, Overlap& o) {
            auto iter = groups.find(a);
            if (iter != groups.end()) {
                auto biter = iter->second.find(b);
                if (biter == iter->second.end()) {
                    iter->second[b] = &o;
                }
                else {
                    if (!better(biter->second, &o)) {
                        iter->second[b] = &o;
                    }
                }
            }
            else {
                auto iter = groups.insert(std::make_pair(a, std::unordered_map<int, Overlap*>()));
                iter.first->second[b] = &o;
            }
        };

        add_overlap(o.b_.id, o.a_.id, o);
    
    }

    return groups;
}

std::unordered_map<int, std::unordered_map<int, Overlap*>> OverlapStore::GroupQuery(bool(*better)(const Overlap* a, const Overlap *b)) {
    std::unordered_map<int, std::unordered_map<int, Overlap*>> groups;

    for (size_t i=0; i < Size(); ++i) {
        auto &o = Get(i);

        auto add_overlap = [&](int a, int b, Overlap& o) {
            auto iter = groups.find(a);
            if (iter != groups.end()) {
                auto biter = iter->second.find(b);
                if (biter == iter->second.end()) {
                    iter->second[b] = &o;
                }
                else {
                    if (!better(biter->second, &o)) {
                        iter->second[b] = &o;
                    }
                }
            }
            else {
                auto iter = groups.insert(std::make_pair(a, std::unordered_map<int, Overlap*>()));
                iter.first->second[b] = &o;
            }
        };

        add_overlap(o.a_.id, o.b_.id, o);
    
    }

    return groups;
}


void OverlapStore::Group(std::unordered_map<int, std::unordered_map<int, const Overlap*>>& groups, size_t thread_size) const {
    
    std::mutex mutex;

    auto add_overlap = [](int low, int a, int b, const Overlap& o, std::vector<std::unordered_map<Seq::Id, const Overlap*>>& gs) {
        
        auto it = gs[a-low].find(b);
        if (it == gs[a-low].end()) {
            gs[a-low][b] = &o;
        } else {
            if (o.AlignedLength() > it->second->AlignedLength()) {
                //SetOlReason(*(it->second), OlReason::Duplicate());
                it->second = &o;
            } else {
                //SetOlReason(o, OlReason::Duplicate());
            }
        }

    };

    auto split_func = [this,thread_size]() {
        auto r = GetReadIdRange();
        return SplitRange(thread_size, r[0], r[1]);
    };

    auto comb_func = [&groups, &mutex](int low, const std::vector<std::unordered_map<Seq::Id, const Overlap*>>&& gs) {
        std::lock_guard<std::mutex> lock(mutex);
        for (size_t i=0; i<gs.size(); ++i) {
            if (gs[i].size() > 0) {
                groups[low+(int)i] = std::move(gs[i]);
            }
        }
    };

    auto work_func = [this, add_overlap, comb_func](std::array<Seq::Id, 2> r) {
        std::vector<std::unordered_map<Seq::Id, const Overlap*>> gs(r[1] - r[0]);

        for (size_t i=0; i<Size(); ++i) {
            const auto &o = Get(i);
            if (o.a_.id >= r[0] && o.a_.id < r[1]) {
                add_overlap(r[0], o.a_.id, o.b_.id, o, gs); 
            }
            if (o.b_.id >= r[0] && o.b_.id < r[1]) {
                add_overlap(r[0], o.b_.id, o.a_.id, o, gs);
            }
        }

        comb_func(r[0], std::move(gs));
    };

    MultiThreadRun((int)thread_size, split_func, work_func);
}

void OverlapStore::Group(std::unordered_map<int, std::unordered_map<int, const Overlap*>>& groups, const std::unordered_set<int>& keys, size_t thread_size) const {
    
    std::mutex mutex;

    auto add_overlap = [](int low, int a, int b, const Overlap& o, std::vector<std::unordered_map<Seq::Id, const Overlap*>>& gs) {
        
        auto it = gs[a-low].find(b);
        if (it == gs[a-low].end()) {
            gs[a-low][b] = &o;
        } else {
            if (o.AlignedLength() > it->second->AlignedLength()) {
                //SetOlReason(*(it->second), OlReason::Duplicate());
                it->second = &o;
            } else {
                //SetOlReason(o, OlReason::Duplicate());
            }
        }

    };

    auto split_func = [this,thread_size]() {
        auto r = GetReadIdRange();
        return SplitRange(thread_size, r[0], r[1]);
    };

    auto comb_func = [&groups, &mutex](int low, const std::vector<std::unordered_map<Seq::Id, const Overlap*>>&& gs) {
        std::lock_guard<std::mutex> lock(mutex);
        for (size_t i=0; i<gs.size(); ++i) {
            if (gs[i].size() > 0) {
                groups[low+(int)i] = std::move(gs[i]);
            }
        }
    };

    auto work_func = [this, add_overlap, comb_func, &keys](std::array<Seq::Id, 2> r) {
        std::vector<std::unordered_map<Seq::Id, const Overlap*>> gs(r[1] - r[0]);

        for (size_t i=0; i<Size(); ++i) {
            const auto &o = Get(i);
            if (o.a_.id >= r[0] && o.a_.id < r[1] && keys.find(o.a_.id) != keys.end()) {
                add_overlap(r[0], o.a_.id, o.b_.id, o, gs); 
            }
            if (o.b_.id >= r[0] && o.b_.id < r[1] && keys.find(o.b_.id) != keys.end()) {
                add_overlap(r[0], o.b_.id, o.a_.id, o, gs);
            }
        }

        comb_func(r[0], std::move(gs));
    };

    MultiThreadRun((int)thread_size, split_func, work_func);
}


void OverlapStore::GroupTarget(std::unordered_map<Seq::Id, std::unordered_map<Seq::Id, std::vector<const Overlap*>>> &groups, size_t threads) const {
    
    std::mutex mutex;

    auto split_func = [this,threads]() {
        return SplitRange<size_t>(threads, 0, Size());
    };

    auto comb_func = [&groups, &mutex](std::unordered_map<Seq::Id, std::unordered_map<Seq::Id, std::vector<const Overlap*>>>& gp) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& t : gp) {
            for (auto &q : t.second) {
                groups[t.first][q.first].insert(groups[t.first][q.first].end(), q.second.begin(), q.second.end());
            }
        }
    };

    auto work_func = [this, comb_func](std::array<size_t, 2> r) {

        std::unordered_map<Seq::Id, std::unordered_map<Seq::Id, std::vector<const Overlap*>>> gp;

        for (size_t i=r[0]; i<r[1]; ++i) {
            auto &o = Get(i);
            gp[o.b_.id][o.a_.id].push_back(&o);
        }

        comb_func(gp);
    };

    MultiThreadRun((int)threads, split_func, work_func);
}



std::string OverlapStore::ToM4aLine(const Overlap& o, const StringPool::NameId& ni) {

    std::ostringstream oss;

    oss << ni.QueryNameById(o.a_.id) << " " << ni.QueryNameById(o.b_.id) << " " 
        << o.identity_ << " " << -(int)o.AlignedLength() << " "
        << o.a_.strand << " " << o.a_.start << " " << o.a_.end << " " << o.a_.len << " "
        << o.b_.strand << " " << o.b_.start << " " << o.b_.end << " " << o.b_.len;

    return oss.str();
}

std::string OverlapStore::ToM4Line(const Overlap &o, const StringPool::NameId& ni) {
    return o.ToM4Line();
}

std::string OverlapStore::ToPafLine(const Overlap &o, const StringPool::NameId& ni) {

    std::ostringstream oss;

    oss << ni.QueryNameById(o.a_.id) << "\t" << o.a_.len << "\t" << o.a_.start << "\t" << o.a_.end << "\t" 
        << ((o.a_.strand == o.b_.strand) ? "+" : "-") << "\t"
        << ni.QueryNameById(o.b_.id) << "\t" << o.b_.len << "\t" << o.b_.start << "\t" << o.b_.end << "\t" 
        << (int)((o.a_.end-o.a_.start)*o.identity_/100) << "\t" << ((o.a_.end-o.a_.start)) << "\t0";

    return oss.str();
}

} // namespace fsa {

