#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <thread>
#include <vector>
#include <pthread.h>
#include <cstdlib>
size_t DATA_SIZE = 1000000;
size_t THRESHOLD = 10000;
int core_count = std::thread::hardware_concurrency();
template <typename Func>
long long benchmark(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}
void generate_random_data(std::vector<int>&data,bool nearly_sorted=false){
    data.resize(DATA_SIZE);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0,1000000);
    for (size_t i = 0; i < DATA_SIZE; ++i) {
        data[i] = dis(gen);
    }
    if(nearly_sorted){
        std::sort(data.begin(), data.end());
        std::uniform_int_distribution<> shuffle_dis(0, DATA_SIZE - 1);
        size_t shuffle_count = DATA_SIZE * 0.1;
        for (size_t i = 0; i < shuffle_count;++i){
            size_t a=shuffle_dis(gen);
            size_t b = shuffle_dis(gen);
            std::swap(data[a], data[b]);
        }
    }
}
void merge(std::vector<int>& data,size_t left,size_t mid,size_t right){
    std::vector<int> temp(right-left+1);
    size_t i = left;
    size_t j = mid + 1;
    size_t k = 0;
    while(i<=mid&&j<=right){
        if(data[i]<=data[j]){
            temp[k++] = data[i++];
        }else{
            temp[k++] = data[j++];
        }
    }
    while(i<=mid){
        temp[k++] = data[i++];
    }
    while(j<=right){
        temp[k++] = data[j++];
    }
    std::copy(temp.begin(), temp.end(), data.begin() + left);
}
void merge_sort_single(std::vector<int>& data, size_t left, size_t right) {
    if(left>=right){
        return;
    }
    if(right-left+1<=THRESHOLD){
        std::sort(data.begin() + left, data.begin() + right + 1);
        return;
    }
    size_t mid = left + (right - left) / 2;
    merge_sort_single(data, left, mid);
    merge_sort_single(data, mid + 1, right);
    merge(data, left, mid, right);
}
void pthread_merge(std::vector<int>&data) {
    size_t count = data.size();
    if (count < THRESHOLD) {
        merge_sort_single(data,0,count-1);
        return;
    }
    std::vector<std::thread> Pthread;
    Pthread.reserve(core_count);
    int size = count / core_count;
    for (int i = 0; i < core_count; i++) {
        size_t left = i * size;
        size_t right = (i == core_count - 1) ? count - 1 : ((i + 1) * size - 1);
        Pthread.emplace_back(
            [&data, left, right]() { merge_sort_single(data, left, right); });
    }
    for (int i = 0; i < core_count;i++){
        Pthread[i].join();
    }
    while(size<count){
        for (size_t left = 0; left < count;left+=2*size){
            size_t mid = left + size - 1;
            size_t right = std::min(left + 2 * size - 1, count - 1);
            if(mid<right){
                merge(data, left, mid, right);
            }
        }
        size *= 2;
    }
}
int main(){
    std::vector<int> data1,data2;
    generate_random_data(data1, false);
    data2 = data1;
    long long time1 =
        benchmark([&]() { merge_sort_single(data1, 0, data1.size() - 1); });
    long long time2 = benchmark([&]() { pthread_merge(data2); });
    std::cout << "单线程排序耗时"<<time1<<"毫秒" << std::endl;
    std::cout << "多线程排序耗时"<<time2<<"毫秒" << std::endl;
    return 0;
}
