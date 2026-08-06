#include <iostream>
#include <thread>
#include "channel.hpp"

int main(){
    Channel<int> ch(3);
    std::thread t1([&ch](){
        int a=10;
        while(a--){
            ch.send(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
        }
    });
    std::thread t2([&ch](){
        int a=10;
        while(a--){
            ch.send(2);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
        }
    });
    std::thread t3([&ch](){
        while(1){
            auto value=ch.receive();
            if(!value.has_value()){
                std::cout<<"receiving end"<<std::endl;
                break;
            }
            std::cout<<"receive: "<<value.value()<<std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    t1.join();
    t2.join();
    ch.close();
    t3.join();
    return 0;
}