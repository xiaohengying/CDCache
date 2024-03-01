#ifndef CDCACHE_CACHE_POLICY_H
#define CDCACHE_CACHE_POLICY_H

#include <algorithm>
#include <deque>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

#include "abstract_cache.h"
template <typename T>
class AbstractCachePolicy {
   public:
    AbstractCachePolicy() = default;

    virtual ~AbstractCachePolicy() = default;

    // Get the oldest key in the cache list for eviction (only get it without additional operations)
    virtual T oldest() = 0;

    // for debug
    virtual std::vector<T> oldests(size_t n) { return {oldest()}; }

    // Move a key to the head (do nothing if the key does not exist)
    virtual void promote(T t) = 0;

    // Remove a key
    virtual void evict(T t) = 0;
    // insert a key to cache policy (do nothing if the key dost not exist)
    virtual void insert(T t) = 0;

    [[nodiscard]] virtual size_t size() const = 0;
};

template <typename T>
class LRUPolicy : public AbstractCachePolicy<T> {
   public:
    T oldest() override { return this->list_.back(); }
    std::vector<T> oldests(size_t n) override {
        auto real_size = std::min(n, this->list_.size());
        auto it = this->list_.rbegin();
        std::vector<T> res;
        while (it != this->list_.rend()) {
            res.push_back(*it);
            it++;
        }
        return res;
    }

    void promote(T t) override {
        auto it = this->cache_.find(t);
        if (it == this->cache_.end()) {
            // Assert(false, "Can not promote an non-exist cache");
        } else {
            this->list_.erase(this->cache_[t]);
            this->list_.push_front(t);
            this->cache_[t] = this->list_.begin();
        }
    }
    void evict(T t) override {
        auto it = this->cache_.find(t);
        if (it != this->cache_.end()) {
            this->list_.erase(it->second);
            this->cache_.erase(it);
        }
    }
    void insert(T t) override {
        auto it = this->cache_.find(t);
        if (it == this->cache_.end()) {
            this->list_.push_back(t);
            this->cache_[t] = std::prev(this->list_.end());
        }
    }

    [[nodiscard]] size_t size() const override { return this->list_.size(); }

   private:
    std::list<T> list_;
    std::unordered_map<T, typename std::list<T>::iterator> cache_;
};

// template <typename T>
// class LFUPolicy : public AbstractCachePolicy<T> {
//    public:
//     struct Node {
//         T key{};
//         int freq{0};
//     };

//    public:
//     // 频率最低的最后访问的一个
//     T oldest() override {
//         Assert(!this->freq_table_.empty() && (!this->freq_table_.begin()->second.empty()), "Oldest inconsistent
//         Data"); return this->freq_table_.begin()->second.back().key;
//     }

//     void promote(T t) override {}

//     // remove from table
//     void evict(T t) override {
//         auto it = key_table_.find(t);
//         if (it == key_table_.end()) return;  // return if not found
//         auto freq = it->second->freq;
//         this->key_table_.erase(it);
//         this->freq_table_[freq].erase(it->second);
//         if (freq_table_[freq].empty()) {
//             freq_table_.erase(freq);
//         }
//     }

//     void insert(T t) override {
//         auto it = this->key_table_.find(t);
//         if (it != key_table_.end()) {
//             // update freq
//             auto freq = it->second->freq;                     // get freq
//             this->evict(t);                                   // remove old entry
//             freq_table_[freq + 1].push_front({t, freq + 1});  // add new entry
//             key_table_[t] = freq_table_[freq + 1].begin();
//         } else {
//             freq_table_[1].push_front({t, 1});       // insert into freq[1]
//             key_table_[t] = freq_table_[1].begin();  // update key_table//        min_freq_ = 1; // update min_freq
//         }
//     }

//     [[nodiscard]] size_t size() const override { return this->key_table_.size(); }

//    private:
//     std::unordered_map<T, typename std::list<Node>::iterator> key_table_;
//     // 频率 -> 该频率对应的所有数据块
//     std::map<int, std::list<Node>> freq_table_;
// };

// template <typename T>
// class ARCPolicy : public AbstractCachePolicy<T> {
//    public:
//     T oldest() override {
//         if (t1_.empty() && t2_.empty()) {
//             throw std::runtime_error("[ARC.oldest] invalid cache content");
//         }
//         if (t1_.empty()) {
//             return t2_.back();
//         }
//         if (t2_.empty()) {
//             return t1_.back();
//         }

//         double current_prop = static_cast<double>(t1_.size()) / static_cast<double>(t2_.size());
//         if (current_prop > this->prop_) {
//             return t1_.back();
//         } else {
//             return t2_.back();
//         }
//     }
//     void promote(T t) override {}
//     void evict(T key) override {
//         auto it1 = this->t1_table_.find(key);
//         if (it1 != this->t1_table_.end()) {
//             move_list(t1_, t1_table_, it1->second, b1_, b1_table_, key);
//             auto last = this->b1_.back();
//             this->b1_table_.erase(last);
//             this->b1_.pop_back();

//             return;
//         }

//         auto it2 = this->t2_table_.find(key);
//         if (it2 != this->t2_table_.end()) {
//             move_list(t2_, t2_table_, it2->second, b2_, b2_table_, key);
//             auto last = this->b2_.back();
//             this->b2_table_.erase(last);
//             this->b2_.pop_back();
//             return;
//         }
//         throw std::runtime_error("[ARC.evict] invalid cache content");
//     }

//     void insert(T key) override {
//         // CASE 1
//         auto it1 = t1_table_.find(key);
//         if (it1 != t1_table_.end()) {
//             // hit T1 --> move to T2 MRU
//             move_list(t1_, t1_table_, it1->second, t2_, t2_table_, key);
//             return;
//         }

//         auto it2 = t2_table_.find(key);
//         if (it2 != t2_table_.end()) {
//             // hit T2 move to T2 MRU
//             move_list(t2_, t2_table_, it2->second, t2_, t2_table_, key);

//             replace(key);
//             return;
//         }

//         auto bit1 = b1_table_.find(key);
//         if (bit1 != b1_table_.end()) {
//             // in B1 move to t2 MRU
//             move_list(b1_, b1_table_, bit1->second, t2_, t2_table_, key);
//             auto diff = this->b1_.size() >= this->b2_.size()
//                             ? 0.1
//                             : static_cast<double>(this->b2_.size()) / static_cast<double>(this->b1_.size());

//             this->prop_ = std::min(this->prop_ + diff, 0.5);

//             // p1++ p2--
//             return;
//         }

//         auto bit2 = b2_table_.find(key);
//         if (bit2 != b2_table_.end()) {
//             // in B2 move to t2 MRU
//             // p1-- p2++
//             auto diff = this->b2_.size() >= this->b1_.size()
//                             ? 0.1
//                             : static_cast<double>(this->b1_.size()) / static_cast<double>(this->b2_.size());
//             this->prop_ = std::min(this->prop_ - diff, 0.0);
//             move_list(b2_, b2_table_, bit2->second, t2_, t2_table_, key);
//             return;
//         }

//         // insert into T1 MRU
//         this->t1_.push_front(key);
//         this->t1_table_[key] = this->t1_.begin();
//     }
//     [[nodiscard]] size_t size() const override { return 0; }

//    private:
//     void move_list(std::list<T> &from, std::unordered_map<T, typename std::list<T>::iterator> &from_table_,
//                    typename std::list<T>::iterator iter, std::list<T> &to,
//                    std::unordered_map<T, typename std::list<T>::iterator> &to_table_, const T &key) {
//         assert(iter != from.end());
//         from.erase(iter);
//         from_table_.erase(key);
//         to.push_front(key);
//         to_table_[key] = to.begin();
//     }

//     void replace(int key) {
//         // TODO: 这里不能手动逐出，会导致数据不同步

//         // move LRU T1 to B1
//         // or
//         // move LRU T2 to B2
//     }

//     std::list<T> t1_;
//     std::list<T> t2_;
//     std::unordered_map<T, typename std::list<T>::iterator> t1_table_, t2_table_;
//     std::list<T> b1_;
//     std::list<T> b2_;
//     std::unordered_map<T, typename std::list<T>::iterator> b1_table_, b2_table_;
//     // 定义T1 和 T2 缓存的一个比例
//     double prop_{1.0};
// };

// template <typename T>
// class LIRSPolicy : public AbstractCachePolicy<T> {
//     enum PageType { LIR = 0, R_HIR = 1, N_HIR = 2, Invalid = 3 };

//     struct Node {
//         T value{};
//         PageType type{Invalid};
//     };

//    public:
//     T oldest() override {
//         if (!this->q_.empty()) return this->q_.back().value;
//         //        printf("Warn: Empty queue Q");
//         //        if (!this->s_.empty()) return this->s_.back().value;
//         Assert(false, "Oldest: Invalid data_block content");
//         return T{};
//     }

//     void promote(T t) override {}

//     // 逐出
//     void evict(T t) override {
//         if (this->q_table_.count(t)) {
//             Assert(!this->q_.empty() && this->q_.back().value == t, "Evict1: Invalid DataBlock Content");
//             this->q_.pop_back();
//             this->q_table_.erase(t);

//             auto it = this->s_table_.find(t);
//             if (it != this->s_table_.end()) {
//                 if (it->second->type != N_HIR) {
//                     this->s_size_--;
//                 }
//                 it->second->type = N_HIR;
//             }
//         } else if (this->s_table_.count(t)) {
//             Assert(!this->s_.empty() && this->s_.back().value == t && this->q_.empty(),
//                    "Evict2: Invalid DataBlock Content");
//             this->s_.pop_back();
//             this->s_table_.erase(t);
//             this->pruning(this->s_, this->s_table_);
//         }
//     }
//     void insert(T t) override {
//         auto it = this->s_table_.find(t);
//         // Case LIR, erase and move to top
//         if (it != this->s_table_.end() && it->second->type == LIR) {
//             this->moveToTop(this->s_, this->s_table_, t, LIR);
//             this->pruning(this->s_, this->s_table_);
//             return;
//         }

//         // N-HIR
//         if (it != this->s_table_.end() && it->second->type == N_HIR) {
//             // 移动到顶部并标记为LIR
//             this->moveToTop(this->s_, this->s_table_, t, LIR);
//             this->s_size_++;  // N_HIR -> LIR
//             if (current_prop() > this->prop) {
//                 // 如果LIR缓存满了，从S挪动一个到Q
//                 this->s_size_--;  // 挪动保证数量-1
//                 auto back = this->s_.back();
//                 this->s_table_.erase(back.value);
//                 this->q_.push_front({back.value, R_HIR});
//             }
//             this->pruning(this->s_, this->s_table_);  // 减
//             // TODO:Q满了淘汰Q，由于是手动逐出了，因此不需要这个东西了
//             return;
//         }

//         auto it2 = this->q_table_.find(t);
//         if (it2 != this->q_table_.end()) {  // hit R_HIR in Q
//             auto node = it2->second;
//             auto ins_it = this->s_table_.find(t);
//             if (ins_it == this->s_table_.end()) {  // in Q but not in S
//                 // push to Q front
//                 this->s_.push_front({node->value, node->type});
//                 this->s_table_[node->value] = this->s_.begin();
//                 // push to Q front
//                 this->q_.erase(it2->second);
//                 this->q_.push_front({node->value, node->type});
//                 this->q_table_[node->value] = this->q_.begin();
//             } else {
//                 this->moveToTop(this->s_, this->s_table_, t, LIR);  // push to S
//                 this->q_.erase(it2->second);                        // erase from q
//                 if (current_prop() > this->prop) {
//                     // 如果LIR缓存满了，从S挪动一个到Q
//                     this->s_size_--;  // 挪动保证数量-1
//                     auto back = this->s_.back();
//                     this->s_table_.erase(back.value);
//                     this->q_.push_front({back.value, R_HIR});
//                 }
//                 this->pruning(this->s_, this->s_table_);  // 减
//             }
//             return;
//         }

//         // 同时写入S和Q的头部
//         this->s_.push_front({t, R_HIR});
//         this->s_table_[t] = this->s_.begin();
//         this->q_.push_front({t, R_HIR});
//         this->q_table_[t] = this->q_.begin();
//         // 最大的难题，都是两部分缓存，缓存的容量很不好确定，循环依赖bug
//     }

//     [[nodiscard]] size_t size() const override { return 0; }

//    private:
//     // promote列表中的某个数据块到头部，并更新散列
//     void moveToTop(std::list<Node> &list, std::unordered_map<int, typename std::list<Node>::iterator> &map,
//                    const T &key, PageType type) {
//         auto it = map.find(key);
//         assert(it != map.end());  // 预期行为
//         auto list_iter = it->second;
//         list.erase(list_iter);
//         list.push_front({key, type});
//         map[key] = list.begin();
//     }
//     /*
//      * 移除栈底部所有的非LIR元素(包括R_HIR / N_HIR)
//      */
//     void pruning(std::list<Node> &list, std::unordered_map<int, typename std::list<Node>::iterator> &map) {
//         printf("Pruning");
//         while (!list.empty()) {
//             auto back = list.back();
//             /*if (back.type == R_HIR || back.type == N_HIR) {*/
//             if (back.type == N_HIR) {  // 原论文是两种都要删除，这里只删一种是为了不一致
//                 map.erase(back.value);
//                 list.pop_back();
//                 if (back.type == R_HIR) {
//                     this->s_size_--;
//                 }
//             } else {
//                 break;
//             }
//         }
//     }

//     // 当前LIR缓存 占当前总缓存的比例
//     double current_prop() {
//         return static_cast<double>(this->s_size_) / static_cast<double>(this->s_size_ + this->q_.size());
//     }

//     std::list<Node> s_;
//     std::unordered_map<int, typename std::list<Node>::iterator> s_table_;
//     std::list<Node> q_;
//     std::unordered_map<int, typename std::list<Node>::iterator> q_table_;
//     size_t s_size_{0};  // 记录L中HIR 和 R_HIR中的数量
//     double prop{0.1};
// };

// template <typename T>
// class TwoQPolicy : public AbstractCachePolicy<T> {
//    public:
//     T oldest() override {
//         Assert(this->lru_.size() == this->lru_table_.size(), "Inconsistent LRU Data");
//         Assert(this->fifo_.size() == this->fifo_table_.size(), "Inconsistent FIFO Data");
//         Assert(!(this->lru_.empty() && this->fifo_table_.empty()), "Empty data_block content");
//         double lru_prop =
//             static_cast<double>(this->lru_.size()) / static_cast<double>(this->lru_.size() + this->fifo_.size());
//         if (lru_prop > this->lru_prop_) {
//             Assert(!lru_.empty(), "Empty LRU");
//             return this->lru_.back();
//         } else {
//             Assert(!fifo_.empty(), "Empty FIFO");
//             return this->fifo_.back();
//         }
//     }
//     void promote(T t) override {}
//     void evict(T t) override {
//         if (this->lru_table_.count(t)) {
//             Assert(!lru_.empty() && lru_.back() == t, "Evict: Invalid cache content");
//             this->lru_table_.erase(t);
//             this->lru_.pop_back();
//         } else {
//             Assert(!fifo_.empty() && fifo_.back() == t, "Evict: Invalid cache content");
//             this->fifo_table_.erase(t);
//             this->fifo_.pop_back();
//         }
//     }

//     void insert(T t) override {
//         auto it = this->fifo_table_.find(t);
//         if (it != this->fifo_table_.end()) {
//             // hit fifo
//             Assert(!lru_table_.count(t), "Insert duplicated Content");
//             // insert into lru
//             this->lru_.push_front(t);
//             this->lru_table_[t] = this->lru_.begin();
//             this->fifo_.erase(it->second);
//             this->fifo_table_.erase(it);
//             return;
//         }

//         auto it2 = this->lru_table_.find(t);
//         if (it2 != this->lru_table_.end()) {
//             this->lru_.erase(it2->second);
//             this->lru_.push_front(t);
//             this->lru_table_[t] = this->lru_.begin();
//             return;
//         }
//         // not hit
//         this->fifo_.push_front(t);
//         this->fifo_table_[t] = this->fifo_.begin();
//     }
//     std::list<T> fifo_;
//     std::unordered_map<T, typename std::list<T>::iterator> fifo_table_;
//     std::list<T> lru_;
//     std::unordered_map<T, typename std::list<T>::iterator> lru_table_;
//     double lru_prop_{0.5};
//     [[nodiscard]] size_t size() const override { return this->fifo_.size() + this->lru_.size(); }
// };

#endif
