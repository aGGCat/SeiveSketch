#ifndef EVALUATION
#define EVALUATION
#include "string.h"
#include "datatypes.hpp"
#include <iostream>
#include <time.h>
#include <sys/time.h>

class Evaluation {

    public:
        Evaluation();
        ~Evaluation();
        static inline double now_us ()
        {
            struct timespec tv;
            clock_gettime(CLOCK_MONOTONIC, &tv);
            return (tv.tv_sec * (uint64_t) 1000000 + (double)tv.tv_nsec/1000);
        }

        // static inline std::string to_ip(KEYTYPE key) {
        //     std::string ip = std::to_string((key>>24)&255) + "." + std::to_string((key>>16)&255) + "." + std::to_string((key>>8)&255) + "." + std::to_string(key&255);
        //     return ip;
        // }


        void get_accuracy(myvector &ground, myvector &detect, double *precision, double *recall, double *error);

        // void get_accuracy(Unmap &ground, Unmap &origround, Unmap &detect, double *precision, double *recall, double *error);


};


#endif


Evaluation::Evaluation() {};

Evaluation::~Evaluation() {};



void Evaluation::get_accuracy(myvector &ground, myvector &detect, double *precision, double *recall, double *error) {
    int tp=0, fp=0, fn=0;
    *error = 0;
    for (auto it = detect.begin(); it != detect.end(); it++) {
        int flag = 0;
        val_tp truth = 0;
        for(auto tmp = ground.begin(); tmp != ground.end(); tmp++) {
            if(tmp->first == it->first) {
                flag = 1;
                truth = tmp->second;
                // printf("tp: %016lx  %016lx\n",tmp->first,it->first);
                break;
            }
        }
        if(flag) {
            tp++;
            *error += 1.0*abs((long)it->second - (long)truth)/truth;
        }
        // else
        //     std::cout<<it->first<<" "<<it->second<<std::endl;
    }
    // for(auto tmp = ground.begin();tmp!=ground.end();tmp++) {
    //     int flag = 0;
    //     for(auto it = detect.begin(); it != detect.end(); it++){
    //         if(tmp->first == it->first) {
    //             flag = 1;
    //             break;
    //         }
    //     }
    //     if(!flag)
    //         std::cout<<tmp->first<<" "<<tmp->second<<std::endl;
    // }


    fn = ground.size() - tp; //remove root
    fp = detect.size() - tp;
    std::cout << "TP = " << tp << " FP = " << fp << " FN = " << fn << std::endl;
    *error = *error/tp;
    *precision = tp*1.0/(tp+fp);
    *recall = tp*1.0/(tp+fn);
}


