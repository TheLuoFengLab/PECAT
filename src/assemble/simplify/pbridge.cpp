#include "pbridge.hpp"

namespace fsa {


bool PathBridgeSimplifier::ParseParameters(const std::vector<std::string> &params) { 
    //assert(params[0] == "bridge");

    CreateDebugFile();
    for (size_t i = 1; i < params.size(); ++i) {
        auto it = SplitStringByChar(params[i], '=');
        if (it[0] == "length") {
            max_length = (size_t) std::stoul(it[1]);
        } else if (it[0] == "nodesize") {
            max_nodesize = (size_t) std::stoul(it[1]);
        } else {
            return false;
        }
    }
    return true;
}

// void PathBridgeSimplifier::Running() {
//     // graph_.MarkRepeatBridge();

//     int DEPTH = graph_.Options().max_spur_nodesize;
//     int LENGTH_THRESHOLD = graph_.Options().max_spur_length;

//     auto cands = graph_.CollectEdges([](SgEdge* e) {
//         auto pe = static_cast<PathEdge*>(e);
//         return  !pe->IsReduced() && 
//                 pe->InNode()->InDegree() == 1 && pe->InNode()->OutDegree() >= 2 ;
//     });

//     std::vector<PathGraph::LinearPath> removed;
        
//     LOG(INFO)("PathBridgeSimplifier %zd", cands.size());

//     for (auto &i : cands) {
//         auto e =  static_cast<PathEdge*>(i);

//         assert(!e->IsReduced() && e->InNode()->InDegree() == 1 && e->InNode()->OutDegree() >= 2);
       
//         auto edge = graph_.FindBridgePath(e, LENGTH_THRESHOLD, DEPTH);

//         if (edge.path.size() > 0) {
//             // check the end is located in other branches.
//             if (graph_.HasBridgeJunction(edge, DEPTH)) {
//                 bool abnormal = IsAbnormalBridge(edge);

//                 if (abnormal) {
//                     removed.push_back(edge);
//                 }
//             }
//         }
//     }

//     std::sort(removed.begin(), removed.end(), [](const PathGraph::LinearPath& a, const PathGraph::LinearPath& b) {
//         return a.length < b.length;
//     });

//     LOG(INFO)("candiate repeat_bridge: %zd", removed.size());
//     for (const auto &e : removed) {
//         if (e.path.front()->InNode()->InDegree() == 1 && e.path.front()->InNode()->OutDegree() >= 2 && 
//             e.path.back()->OutNode()->InDegree() >= 2 && e.path.back()->OutNode()->OutDegree() == 1) { 


//             if (!e.path.front()->IsReduced() && e.path.front()->NodeSize() < 10) {
//                 e.path.front()->Reduce("repeat_bridge", true);
//                 graph_.ReverseEdge(e.path.front())->Reduce("repeat_bridge", true);
//             }

//             if (!e.path.back()->IsReduced() && e.path.back()->NodeSize() < 10 ) {
//                 e.path.back()->Reduce("repeat_bridge", true);
//                 graph_.ReverseEdge(e.path.back())->Reduce("repeat_bridge", true);
//             }
            
//         } 
//     }
// }


void PathBridgeSimplifier::Running() {

    auto nodes = graph_.CollectNodes([](SgNode* n) {
        return n->OutDegree() > 1;
    });

    std::unordered_map<PathEdge*, int> removed;
    LOG(INFO)("Bridging: find condidate nodes: %zd", nodes.size());

    for (auto &n : nodes) {

        Debug("bbb: node: %s\n", n->Id().ToString(graph_.GetAsmData().GetStringPool()).c_str());
        bool nontrivial = false;
        std::vector<PathGraph::LinearPath> conds;
        for (size_t i = 0; i < n->OutDegree(); ++i) {
            auto e = n->OutEdge<PathEdge>(i);
            auto p = graph_.FindBridgePath1(e, max_length, max_nodesize);

            if (p.path.size() > 0 && graph_.HasBridgeJunction(p, max_nodesize)) {
                Debug("bbb: conds: %s\n", p.path.front()->ToString(graph_.GetAsmData().GetStringPool()).c_str());
                conds.push_back(p);
            } else {
                nontrivial = true;
            }
        }

        size_t start = 0;
        if (!nontrivial) {
            std::sort(conds.begin(), conds.end(), [](const PathGraph::LinearPath& a, const PathGraph::LinearPath& b) {
                return a.nodesize > b.nodesize || (a.nodesize == b.nodesize && a.length > b.length);
            });
            start = 1;
        }
       
        Debug("bbb: sort: %s, %d %zd\n", n->Id().ToString(graph_.GetAsmData().GetStringPool()).c_str(), nontrivial, conds.size());

        for (size_t i = start; i < conds.size(); ++i) {
            Debug("bbb: rm: %s\n", conds[i].path.front()->ToString(graph_.GetAsmData().GetStringPool()).c_str());
            for (auto e : conds[i].path) {
                Debug("bbb: rm e: %s\n", e->ToString(graph_.GetAsmData().GetStringPool()).c_str());
                Debug("bbb: rm e: %s\n", graph_.ReverseEdge(e)->ToString(graph_.GetAsmData().GetStringPool()).c_str());
                removed[e] ++;
                removed[graph_.ReverseEdge(e)]++;
            }
        }
    }

    for (auto &i : removed) {
        Debug("bbb: rz e: %d %s\n", i.second, i.first->ToString(graph_.GetAsmData().GetStringPool()).c_str());
        if (i.second >= 2) {
            i.first->Reduce("repeat_bridge", true);
        }
    }
}


bool PathBridgeSimplifier::IsAbnormalBridge(const PathGraph::LinearPath& path) {

    double r = 2;
    for (size_t i = 0; i < path.path.front()->InNode()->OutDegree(); ++i) {
        auto ie = static_cast<PathEdge*>(path.path.front()->InNode()->OutEdge(i));
        
        if (ie != path.path.front()) {
            if (!TestOutExtend(ie, path.length*r, path.nodesize*r)) {
                return false;
            }
        }
    }

    for (size_t i = 0; i < path.path.back()->OutNode()->InDegree(); ++i) {
        auto ie = static_cast<PathEdge*>(path.path.back()->OutNode()->InEdge(i));
        if (ie != path.path.back()) {   
            if (!TestInExtend(ie, path.length*r, path.nodesize*r)) {
                return false;
            }
        }
    }

    return TestInExtend(path.path.front()->InNode()->InEdge<PathEdge>(0), path.length*r, path.nodesize*r) &&  
           TestOutExtend(path.path.back()->OutNode()->OutEdge<PathEdge>(0), path.length*r, path.nodesize*r);
}

bool PathBridgeSimplifier::TestOutExtend(PathEdge* e, int minlen, int minnode) {
    int len = e->Length();
    int node = e->NodeSize();
    const PathNode* curr = e->OutNode();

    while ((len < minlen || node < minnode) && curr->InDegree() == 1 && curr->OutDegree() == 1) {
        len += curr->GetOutEdge(0)->Length();
        node += curr->GetOutEdge(0)->NodeSize(); 
        curr = curr->GetOutEdge(0)->OutNode();
        
    }
    
    Debug("test %d >= %d %d >= %d\n", len, minlen, node, minnode);
    return len >= minlen && node >= minnode;
}

bool PathBridgeSimplifier::TestInExtend(PathEdge* e, int minlen, int minnode) {
    int len = e->Length();
    int node = e->NodeSize();
    const SgNode* curr = e->InNode();

    while ((len < minlen || node < minnode) && curr->InDegree() == 1 && curr->OutDegree() == 1) {
        len += curr->InEdge(0)->Length();
        node += curr->InEdge(0)->NodeSize(); 
        curr = curr->InEdge(0)->InNode();
    }
    Debug("test %d >= %d %d >= %d\n", len, minlen, node, minnode);

    return len >= minlen && node >= minnode;
}


int PathBridgeSimplifier::IsLowestScoreOutEdge(PathEdge* e) {

    auto rvs = graph_.GetAsmData().GetReadVariants();

    size_t idx = 0;
    std::vector<std::array<int, 3>> scores;
    for (size_t i = 0; i < e->InNode()->OutDegree(); ++i) {
        auto ie = static_cast<PathEdge*>(e->InNode()->OutEdge(i));
        if ( !ie->IsType("simple")) return 0;
        auto ps = ie->GetSimplePath(0);
        scores.push_back(GetEdgeScore(ps.front(), rvs));
        if (ie == e) {
            idx = i;
        }
    }

    std::array<size_t, 2> stat = {0, 0};
    for (size_t i = 0; i < scores.size(); ++i) {
        if (i == idx) continue;
        if (EdgeScoreSignificantlyGreater(scores[i], scores[idx])) {
            stat[0] ++;
        }
        if (EdgeScoreSignificantlyGreater(scores[idx], scores[i])) {
            stat[1] ++;
        }
    }

    if (stat[0] + 1 == scores.size()) return -1;
    if (stat[1] + 1 == scores.size()) return 1;
    return 0;
}

int PathBridgeSimplifier::IsLowestScoreInEdge(PathEdge* e) {

    auto rvs = graph_.GetAsmData().GetReadVariants();

    size_t idx = 0;
    std::vector<std::array<int, 3>> scores;
    for (size_t i = 0; i < e->OutNode()->InDegree(); ++i) {
        auto ie = static_cast<PathEdge*>(e->OutNode()->InEdge(i));
        if ( !ie->IsType("simple")) return 0;
        auto ps = e->GetSimplePath(0);
        scores.push_back(GetEdgeScore(ps.back(), rvs));
        if (ie == e) {
            idx = i;
        }
    }

    std::array<size_t, 2> stat = {0, 0};
    for (size_t i = 0; i < scores.size(); ++i) {
        if (i == idx) continue;
        if (EdgeScoreSignificantlyGreater(scores[i], scores[idx])) {
            stat[0] ++;
        }
        if (EdgeScoreSignificantlyGreater(scores[idx], scores[i])) {
            stat[1] ++;
        }
    }

    if (stat[0] + 1 == scores.size()) return -1;
    if (stat[1] + 1 == scores.size()) return 1;
    return 0;
}


} // namespace fsa