/**
 * Implementation of a lock-free unbalanced binary search tree using LLX/SCX.
 * Trevor Brown, 2018.
 */

#ifndef BST_ADAPTER_H
#define BST_ADAPTER_H

#include <iostream>
#include "errors.h"
#include "random_fnv1a.h"
#include "bst_impl.h"

#define RECORD_MANAGER_T record_manager<Reclaim, Alloc, Pool, bst_ns::Node<K, V>>
#define DATA_STRUCTURE_T bst_ns::bst<K, V, std::less<K>, RECORD_MANAGER_T>

template <typename K, typename V, class Reclaim = reclaimer_debra<K>, class Alloc = allocator_new<K>, class Pool = pool_none<K>>
class ds_adapter {
private:
    const V NO_VALUE;
    DATA_STRUCTURE_T * const ds;

public:
    ds_adapter(const int NUM_THREADS,
               const K& KEY_RESERVED,
               const K& unused1,
               const V& VALUE_RESERVED,
               RandomFNV1A * const unused2)
    : NO_VALUE(VALUE_RESERVED)
    , ds(new DATA_STRUCTURE_T(KEY_RESERVED, NO_VALUE, NUM_THREADS))
    {}
    ~ds_adapter() {
        delete ds;
    }
    
    V getNoValue() {
        return NO_VALUE;
    }
    
    void initThread(const int tid) {
        ds->initThread(tid);
    }
    void deinitThread(const int tid) {
        ds->deinitThread(tid);
    }

    bool contains(const int tid, const K& key) {
        return ds->contains(tid, key);
    }
    V insert(const int tid, const K& key, const V& val) {
        return ds->insert(tid, key, val);
    }
    V insertIfAbsent(const int tid, const K& key, const V& val) {
        return ds->insertIfAbsent(tid, key, val);
    }
    V erase(const int tid, const K& key) {
        return ds->erase(tid, key).first;
    }
    V find(const int tid, const K& key) {
        return ds->find(tid, key).first;
    }
    int rangeQuery(const int tid, const K& lo, const K& hi, K * const resultKeys, V * const resultValues) {
        return ds->rangeQuery(tid, lo, hi, resultKeys, resultValues);
    }
    /**
     * Sequential operation to get the number of keys in the set
     */
    int getSize() {
        return ds->getSize();
    }
    void printSummary() {
        std::stringstream ss;
        ss<<ds->getSizeInNodes()<<" nodes in tree";
        std::cout<<ss.str()<<std::endl;
        
        auto recmgr = ds->debugGetRecMgr();
        recmgr->printStatus();
    }
    long long getKeyChecksum() {
        return ds->debugKeySum();
    }
    bool validateStructure() {
        return ds->validate(0, false);
    }
    void printObjectSizes() {
        std::cout<<"sizes: node="
                 <<(sizeof(bst_ns::Node<K, V>))
                 <<" descriptor="<<(sizeof(bst_ns::SCXRecord<K, V>))
                 <<std::endl;
    }
};

#endif
