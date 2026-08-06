#include <map>
#include <string>
#include <iostream>

int main(){

    std::multimap<int,std::string> mmp;
    mmp.insert({1,"luca"});
    mmp.insert({1,"spe"});
    auto er=mmp.equal_range(1);
    for(auto it=er.first;it!=er.second;it++){
        std::cout<<it->first<<' '<<it->second<<std::endl;
    }
    er=mmp.equal_range(1);
    for(auto& it=er.first;it!=er.second;it++){
        if(it->second=="luca"){
            mmp.erase(it);
        }
    }
    er=mmp.equal_range(1);
    std::cout<<"--------------------------------------"<<std::endl;
    for(auto it=er.first;it!=er.second;it++){
        std::cout<<it->first<<' '<<it->second<<std::endl;
    }
    return 0;
}