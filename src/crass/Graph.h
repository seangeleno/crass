//
//  Graph.h
//  crass
//
//  Created by Connor Skennerton on 10/01/14.
//  Copyright (c) 2014 Australian Centre for Ecogenomics. All rights reserved.
//

#ifndef __crass__Graph__
#define __crass__Graph__

#include <iostream>
#include <vector>
#include <unordered_map>
#include <deque>
#include <string>
#include <unordered_set>
#include <stack>

#include "StringCheck.h"
#include "Sequence.h"
namespace crass {
    
    /*
        Node Types
        REPEAT: all bases in node originate from a repeat
        MIXED:  some bases are from both the repeat and spacer
        SPACER: all bases in node originate from a spacer
        UNDEF: For when we don't know yet
     
     */
    enum graph_node_t {
        REPEAT,     
        MIXED,      
        SPACER,
        UNDEF
    };
    
    
    /*
        Normal and jumping edges
        Normal:     This is the standard edge generated by a debruijn graph.  It will point to a
                    kmer which differs by a single base from the current node
        Jumping:    A jumping edge is entirely different.  This type of edge links two nodes that
                    are not next to each other in the naive graph, instead they link two nodes that
                    are on either side of a repeat sequence.  You can therefore think of a jumping
                    edge as being symbolic of the repeat sequence itself but allows for walking through
                    the graph without going though the repeat, which may make determining the corect 
                    path difficult.
     */
    struct Node {
        std::unordered_set<Node *> mFwdEdges;                  // a list of nodes that are in front of this one
        std::unordered_set<Node *> mRevEdges;                  // a list of nodes behind this one
        std::unordered_set<Node *> mFwdJmpEdges;               // a list of nodes that are jumping forward
        std::unordered_set<Node *> mRevJmpEdges;               // a list of nodes that are jumping in reverse
        std::vector<RawRead *> mReadsContainingNode;    // a list of reads which contain this node
    public:
        StringToken mId;
        unsigned int mCov;
        graph_node_t mType;
        int mRepeatedReadCount;
        
        Node(StringToken i) : mId(i), mCov(1), mType(UNDEF), mRepeatedReadCount(0) {}
        
        int inDegree() {return mRevEdges.size();}
        int outDegree() {return mFwdEdges.size();}
        int degree(){return inDegree() + outDegree();}
        int inJmpDegree() {return mRevJmpEdges.size();}
        int outJmpDegree() {return mFwdJmpEdges.size();}
        int jmpDegree() {return inJmpDegree() + outJmpDegree();}
        
        void addFwdEdge(Node * n) {mFwdEdges.insert(n);}
        void addRevEdge(Node * n) {mRevEdges.insert(n);}
        void addFwdJmpEdge(Node * n) {mFwdJmpEdges.insert(n);}
        void addRevJmpEdge(Node * n) {mRevJmpEdges.insert(n);}
        
        Node * fwdEdge() {return *mFwdEdges.begin();}
        Node * revEdge() {return *mRevEdges.begin();}
        Node * fwdJmpEdge() {return *mFwdJmpEdges.begin();}
        Node * revJmpEdge() {return *mRevJmpEdges.begin();}
        
        /*  Remove reference to this node from all of the edges
            then remove all of the edges in the current node
            Does not delete the node from the graph
         */
        void detachNodeFromNeighbours();
        
        char * toGML(const char * kmerSeq);
         

    };

    class Graph {
        std::unordered_map<StringToken, Node *> mNodes;
        StringCheck mNodeLookup; // hash of the node Ids to their string representations
        int mKmerLength;
        int mTotalReadCount;

    public:
        Graph(int _k) : mKmerLength(_k){}
        ~Graph();
        void addReadToGraph(crass::RawRead& read);
        Node * getNode(StringToken T);
        void identifyRepeatNodes(std::deque<Node *>& rpath);
        void toGraphviz(FILE * out, const char * graphName);
        void toGML(FILE * out, const char * graphName);
        void computeCoverageHistogram(FILE * out);
       
        /* Walk along a linear path using the forwad edges from the
         * source node and add the walked nodes onto the path.
         *
         * source:    Node to begin walk from
         * path:      Stack onto which all nodes walked will be put in
         * seenNodes: a set of all nodes that have been encounters in any walk.
         *            If any node is encountered that has been seen before
         *            the walk will imeadiately terminate
         * maxDist:   The maximum number of nodes to walk forward before termination
         *
         * returns:   The number of nodes walked
         */
        unsigned int walkForward(Node * source, std::stack<Node *>& path, std::unordered_set<Node *>& seenNodes, unsigned int maxDist);
        /* Walk along a linear path using the reverse edges from the
         * source node and add the walked nodes onto the path.
         *
         * source:    Node to begin walk from
         * path:      Stack onto which all nodes walked will be put in
         * seenNodes: a set of all nodes that have been encounters in any walk.
         *            If any node is encountered that has been seen before
         *            the walk will imeadiately terminate
         * maxDist:   The maximum number of nodes to walk forward before termination
         *
         * returns:   The number of nodes walked
         */
        unsigned int walkReverse(Node * source, std::stack<Node *>& path, std::unordered_set<Node *>& seenNodes, unsigned int maxDist);
        /* Detach nodes from the stack starting at a tip as long as they have a degree < 2
         * 
         * return: number of nodes actually removed
         */
        unsigned int pruneBackToFork(std::stack<Node *>& path);
        /* remove nodes from the current path without removing them from the graph
         */
        void backtrack(std::stack<Node *>& path);
        /*  Given a source node remove any tips
            Implements Depth first search from the source node in the graph
            Identifies and removes all paths that are not cycles and are less
            than maxDepth in length if they terminate at a node with a 
            degree of 1
         */
        void removeTips(Node * source, unsigned int maxDepth);
        /* Finds all tips (degree = 1) and prunes them to the nearest fork node
         * as long as the distance between the tip and the fork is less than
         * maxDepth nodes
         */
        void removeTipsInward(unsigned int maxDepth);
        void deleteNode(Node * node);
        
        std::string kmerStr(StringToken n){return mNodeLookup.getString(n);}
        
        
    };
}
#endif /* defined(__crass__Graph__) */
