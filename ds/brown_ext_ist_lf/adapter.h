/**
 * Implementation of a lock-free relaxed (a,b)-tree using LLX/SCX.
 * Trevor Brown, 2018.
 */

#ifndef DS_ADAPTER_H
#define DS_ADAPTER_H

#include <iostream>
#include "errors.h"
#include "random.h"
#include "brown_ext_ist_lf_impl.h"

#define RECORD_MANAGER_T record_manager<Reclaim, Alloc, Pool, Node<K,V>, KVPair<K,V>>
#define DATA_STRUCTURE_T istree<K, V, Interpolator<K>, RECORD_MANAGER_T>

template <typename T>
struct ValidAllocatorTest { static constexpr bool value = false; };

template <typename T>
struct ValidAllocatorTest<allocator_new<T>> { static constexpr bool value = true; };

template <typename Alloc>
static bool isValidAllocator(void) {
    return ValidAllocatorTest<Alloc>::value;
}

template <typename K>
class Interpolator {
public:
    bool less(const K& a, const K& b);
    double interpolate(const K& key, const K& rangeLeft, const K& rangeRight);
};

template <>
class Interpolator<long long> {
public:
    int compare(const long long& a, const long long& b) {
        return (a < b) ? -1 : (a > b) ? 1 : 0;
    }
    double interpolate(const long long& key, const long long& rangeLeft, const long long& rangeRight) {
        if (rangeRight == rangeLeft) return 0;
        return (key - rangeLeft) / (double) (rangeRight - rangeLeft);
    }
};

template <typename K, typename V, class Reclaim = reclaimer_debra<K>, class Alloc = allocator_new<K>, class Pool = pool_none<K>>
class ds_adapter {
private:
    DATA_STRUCTURE_T * const ds;

public:
    ds_adapter(const int NUM_THREADS,
               const K& unused1,
               const K& KEY_MAX,
               const V& NO_VALUE,
               Random * const unused3)
    : ds(new DATA_STRUCTURE_T(NUM_THREADS, KEY_MAX, NO_VALUE))
    {
        if (!isValidAllocator<Alloc>()) {
            setbench_error("This data structure must be used with allocator_new.")
        }
        if (NUM_THREADS > MAX_THREADS_POW2) {
            setbench_error("NUM_THREADS exceeds MAX_THREADS_POW2");
        }
    }
    ~ds_adapter() {
        delete ds;
    }
    
    void * getNoValue() {
        return ds->NO_VALUE;
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
    void * insert(const int tid, const K& key, void * const val) {
        return ds->insert(tid, key, val);
    }
    void * insertIfAbsent(const int tid, const K& key, void * const val) {
        return ds->insertIfAbsent(tid, key, val);
    }
    void * erase(const int tid, const K& key) {
        return ds->erase(tid, key).first;
    }
    void * find(const int tid, const K& key) {
        return ds->find(tid, key).first;
    }
    int rangeQuery(const int tid, const K& lo, const K& hi, K * const resultKeys, void ** const resultValues) {
        setbench_error("not implemented");
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
        return true;
    }
    void printObjectSizes() {
        std::cout<<"sizes: node="
                 <<(sizeof(Node<K, V>))
                 <<std::endl;
    }
};

#undef RECORD_MANAGER_T
#undef DATA_STRUCTURE_T

#endif