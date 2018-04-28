/**
 * -----------------------------------------------------------------------------
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ISTREE_H
#define	ISTREE_H

#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <unistd.h>
#include <sys/types.h>
#include "record_manager.h"
#include "dcss_impl.h"

#define KVPAIR_MASK (0x2) /* 0x1 is used by DCSS */
#define IS_KVPAIR(x) ((x)&KVPAIR_MASK)
#define CASWORD_TO_KVPAIR(x) ((KVPair<K,V> *) ((x)&~KVPAIR_MASK))
#define KVPAIR_TO_CASWORD(x) ((casword_t) (((casword_t) (x))|KVPAIR_MASK))

#define REBUILDOP_MASK (0x4)
#define IS_REBUILDOP(x) ((x)&REBUILDOP_MASK)
#define CASWORD_TO_REBUILDOP(x) ((RebuildOperation<K,V> *) ((x)&~REBUILDOP_MASK))
#define REBUILDOP_TO_CASWORD(x) ((casword_t) (((casword_t) (x))|REBUILDOP_MASK))

#define CASWORD_TO_NODE(x) ((Node<K,V> *) (x))
#define NODE_TO_CASWORD(x) ((casword_t) (x))

#define REBUILD_AFTER_CHANGE_FRACTION (0.25)

template <typename K, typename V>
struct Node {
    size_t degree;
    size_t initSize;
    size_t changeSum;
    size_t dirty;
    void * data; // layout: keys first, then pointers
    
    K& key(const int ix) {
        K * const firstKey = ((K *) &data);
        return firstKey[ix];
    }
    // conceptually returns &node.ptrs[ix]
    casword_t * ptrAddr(const int ix) {
        K * const firstKey = ((K *) &data);
        K * const firstKeyAfter = &firstKey[degree - 1];
        casword_t * const firstPtr = (casword_t *) firstKeyAfter;
        return &firstPtr[ix];
    }

    // conceptually returns node.ptrs[ix]
    casword_t ptr(const int ix) {
        return *ptrAddr(ix);
    }
};

template <typename K, typename V>
struct RebuildOperation {
    Node<K,V> * candidate;
    size_t candidateChangeSum;
    Node<K,V> * parent;
    size_t index;
};

template <typename K, typename V>
struct KVPair {
    K k;
    V v;
    KVPair(const K& key, const V& value)
    : k(key), v(value) {}
};

enum InsertResultType {
    RESTART, PROPAGATE, NO_PROPAGATE, DONE
};

template <typename K, typename V>
struct PropagateResult {
    Node<K,V> * candidate;
    size_t candidateChangeSum;
    size_t childChangeSum;
    V oldValue;
    
    PropagateResult() {}
    PropagateResult(Node<K,V> * const _candidate, size_t _candidateChangeSum, size_t _childChangeSum, V _oldValue)
    : candidate(_candidate)
    , candidateChangeSum(_candidateChangeSum)
    , childChangeSum(_childChangeSum)
    , oldValue(_oldValue)
    {}
};

template <typename K, typename V>
struct NoPropagateResult {
    casword_t newObject;
    V oldValue;
};

template <typename K, typename V>
struct InsertResult {
    InsertResultType type;
    PropagateResult<K,V> prop;
    NoPropagateResult<K,V> noprop;
};

template <typename K, typename V, class Interpolate, class RecManager>
class istree {
private:
    PAD;
    RecManager * const recordmgr;
    dcssProvider * const prov;
    Interpolate cmp;

    Node<K,V> * root;

    #define arraycopy(src, srcStart, dest, destStart, len) \
        for (int ___i=0;___i<(len);++___i) { \
            (dest)[(destStart)+___i] = (src)[(srcStart)+___i]; \
        }

private:
    int interpolationSearch(const K& key, Node<K,V> * const node);
    void rebuildSubtree(PropagateResult<K,V> result);
    void helpRebuildSubtree(RebuildOperation<K,V> * op);
    V doInsert(const int tid, const K& key, const V& value, const bool replace);
    InsertResult<K,V> doInsert(const int tid, Node<K,V> * const node, const K& key, const V& value, const bool replace);

    Node<K,V> * allocateNode(const int tid, const int degree);
    KVPair<K,V> * allocateKVPair(const int tid, const K& key, const V& value);

    int init[MAX_THREADS_POW2] = {0,};
public:
    const K INF_KEY;
    const V NO_VALUE;
    const int NUM_PROCESSES;
    PAD;

    void initThread(const int tid) {
        if (init[tid]) return; else init[tid] = !init[tid];

        prov->initThread(tid);
        recordmgr->initThread(tid);
    }
    void deinitThread(const int tid) {
        if (!init[tid]) return; else init[tid] = !init[tid];

        prov->deinitThread(tid);
        recordmgr->deinitThread(tid);
    }

    istree(const int numProcesses
         , const K infinity
         , const V noValue
    )
    : recordmgr(new RecManager(numProcesses, SIGQUIT))
    , prov(new dcssProvider(numProcesses))
    , INF_KEY(infinity)
    , NO_VALUE(noValue)
    , NUM_PROCESSES(numProcesses) 
    {
        cmp = Interpolate();

        const int tid = 0;
        initThread(tid);

        Node<K,V> * _root = allocateNode(tid, 1);
        KVPair<K,V> * _rootPair = allocateKVPair(tid, INF_KEY, NO_VALUE);
        _root->degree = 1;
        _root->initSize = 0;
        _root->changeSum = 0;
        _root->dirty = 0;
        *(_root->ptrAddr(0)) = KVPAIR_TO_CASWORD(_rootPair);
        
        root = _root;
    }

    ~istree() {
//        int nodes = 0;
//        freeSubtree(root, &nodes);
////            COUTATOMIC("main thread: deleted tree containing "<<nodes<<" nodes"<<std::endl);
////            recordmgr->printStatus();
        delete recordmgr;
    }

    Node<K,V> * debug_getEntryPoint() { return root; }

private:
    /*******************************************************************
     * Utility functions for integration with the test harness
     *******************************************************************/

    int sequentialSize(const casword_t ptr) {
        if (IS_KVPAIR(ptr)) {
            return 1;
        }
        int retval = 0;
        auto node = CASWORD_TO_NODE(ptr);
        for (int i=0;i<node->degree;++i) {
            const casword_t child = node->ptr(i);
            retval += sequentialSize(child);
        }
        return retval;
    }
    int sequentialSize() {
        return sequentialSize(root->ptr(0));
    }

    int getNumberOfLeaves(Node<K,V>* node) {
//        if (node == NULL) return 0;
//        if (node->isLeaf()) return 1;
//        int result = 0;
//        for (int i=0;i<node->getABDegree();++i) {
//            result += getNumberOfLeaves(node->ptrs[i]);
//        }
//        return result;
        return 0;
    }
    const int getNumberOfLeaves() {
//        return getNumberOfLeaves(root->ptrs[0]);
        return 0;
    }
    int getNumberOfInternals(Node<K,V>* node) {
//        if (node == NULL) return 0;
//        if (node->isLeaf()) return 0;
//        int result = 1;
//        for (int i=0;i<node->getABDegree();++i) {
//            result += getNumberOfInternals(node->ptrs[i]);
//        }
//        return result;
        return 0;
    }
    const int getNumberOfInternals() {
        return 0;
//        return getNumberOfInternals(root->ptrs[0]);
    }
    const int getNumberOfNodes() {
        return 0;
//        return getNumberOfLeaves() + getNumberOfInternals();
    }

    int getSumOfKeyDepths(Node<K,V>* node, int depth) {
        return 0;
//        if (node == NULL) return 0;
//        if (node->isLeaf()) return depth * node->getKeyCount();
//        int result = 0;
//        for (int i=0;i<node->getABDegree();i++) {
//            result += getSumOfKeyDepths(node->ptrs[i], 1+depth);
//        }
//        return result;
    }
    const int getSumOfKeyDepths() {
        return 0;
//        return getSumOfKeyDepths(root->ptrs[0], 0);
    }
    const double getAverageKeyDepth() {
        return 0;
//        long sz = sequentialSize();
//        return (sz == 0) ? 0 : getSumOfKeyDepths() / sz;
    }

    int getHeight(Node<K,V>* node, int depth) {
//        if (node == NULL) return 0;
//        if (node->isLeaf()) return 0;
//        int result = 0;
//        for (int i=0;i<node->getABDegree();i++) {
//            int retval = getHeight(node->ptrs[i], 1+depth);
//            if (retval > result) result = retval;
//        }
//        return result+1;
        return 0;
    }
    const int getHeight() {
        return 0;
//        return getHeight(root->ptrs[0], 0);
    }

    int getKeyCount(Node<K,V>* root) {
//        if (root == NULL) return 0;
//        if (root->isLeaf()) return root->getKeyCount();
//        int sum = 0;
//        for (int i=0;i<root->getABDegree();++i) {
//            sum += getKeyCount(root->ptrs[i]);
//        }
//        return sum;
        return 0;
    }
    int getTotalDegree(Node<K,V>* root) {
//        if (root == NULL) return 0;
//        int sum = root->getKeyCount();
//        if (root->isLeaf()) return sum;
//        for (int i=0;i<root->getABDegree();++i) {
//            sum += getTotalDegree(root->ptrs[i]);
//        }
//        return 1+sum; // one more children than keys
        return 0;
    }
    int getNodeCount(Node<K,V>* root) {
//        if (root == NULL) return 0;
//        if (root->isLeaf()) return 1;
//        int sum = 1;
//        for (int i=0;i<root->getABDegree();++i) {
//            sum += getNodeCount(root->ptrs[i]);
//        }
//        return sum;
        return 0;
    }
    double getAverageDegree() {
        return 0;
//        return getTotalDegree(root) / (double) getNodeCount(root);
    }
    double getSpacePerKey() {
        return 0;
//        return getNodeCount(root)*2*b / (double) getKeyCount(root);
    }

    long long getSumOfKeys(const casword_t ptr) {
        if (IS_KVPAIR(ptr)) {
            long long key = (long long) CASWORD_TO_KVPAIR(ptr)->k;
            return (key == INF_KEY) ? 0 : key;
        }
        auto node = CASWORD_TO_NODE(ptr);
        long long sum = 0;
        for (int i=0;i<node->degree;++i) {
            sum += getSumOfKeys(node->ptr(i));
        }
        return sum;
    }
    long long getSumOfKeys() {
        return getSumOfKeys(root->ptr(0));
    }

    void istree_error(std::string s) {
        std::cerr<<"ERROR: "<<s<<std::endl;
        exit(-1);
    }

    void debugPrint() {
        std::cout<<"averageDegree="<<getAverageDegree()<<std::endl;
        std::cout<<"averageDepth="<<getAverageKeyDepth()<<std::endl;
        std::cout<<"height="<<getHeight()<<std::endl;
        std::cout<<"internalNodes="<<getNumberOfInternals()<<std::endl;
        std::cout<<"leafNodes="<<getNumberOfLeaves()<<std::endl;
    }

public:
    V insert(const int tid, const K& key, const V& val) {
        return doInsert(tid, key, val, true);
    }
    V insertIfAbsent(const int tid, const K& key, const V& val) {
        return doInsert(tid, key, val, false);
    }
    const std::pair<V,bool> erase(const int tid, const K& key);
    const std::pair<V,bool> find(const int tid, const K& key);
    bool contains(const int tid, const K& key);
    int rangeQuery(const int tid, const K& low, const K& hi, K * const resultKeys, void ** const resultValues) {
        istree_error("not implemented");
    }
    bool validate(const long long keysum, const bool checkkeysum) {
        if (checkkeysum) {
            long long treekeysum = getSumOfKeys();
            if (treekeysum != keysum) {
                std::cerr<<"ERROR: tree keysum "<<treekeysum<<" did not match thread keysum "<<keysum<<std::endl;
                return false;
            }
        }
        return true;
    }

    long long getSizeInNodes() {
        return getNumberOfNodes();
    }
    std::string getSizeString() {
        std::stringstream ss;
        ss<<getSizeInNodes()<<" nodes in tree";
        return ss.str();
    }
    long long getSize(Node<K,V> * node) {
        return sequentialSize(node);
    }
    long long getSize() {
        return sequentialSize();
    }
    RecManager * const debugGetRecMgr() {
        return recordmgr;
    }
    long long debugKeySum() {
        return getSumOfKeys();
    }
};

#endif	/* ISTREE_H */
