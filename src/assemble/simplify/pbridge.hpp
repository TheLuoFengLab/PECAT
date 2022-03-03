
#pragma once

#include "./simplifier.hpp"
namespace fsa {

class PathBridgeSimplifier : public Simplifier {
public:
    PathBridgeSimplifier(PathGraph& graph) : Simplifier(graph), graph_(graph) {
        name_ = "pbridge";
        desc_ = "Remove abnormal bridging edges";
    }

    virtual bool ParseParameters(const std::vector<std::string> &params);
    virtual void Running();
    bool IsAbnormalBridge(const PathGraph::LinearPath &path);
    bool TestOutExtend(PathEdge* e, int minlen, int minnode);
    bool TestInExtend(PathEdge* e, int minlen, int minnode);
    int IsLowestScoreOutEdge(PathEdge* e) ;
    int IsLowestScoreInEdge(PathEdge* e) ;
    PathGraph& graph_;   

    size_t max_length { 1000000 };
    size_t max_nodesize { 20 };
};


} // namespace fsa {