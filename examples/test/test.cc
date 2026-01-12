#include<cstdio>
#include<iostream>
#include<list>
using namespace std;
int main(){
    std::list<int> lst = {1, 2, 3, 4, 5};

    std::cout<<"{1, 2, 3, 4, 5}"<<std::endl;

    std::cout<<"============== use rbegin() ==============="<<std::endl;
    auto it = lst.rbegin();  
    while (it != lst.rend()) {
        std::cout<<*it<<" ";
        it++;
    }
    std::cout<<std::endl;

    std::cout<<"============= use base() =============="<<std::endl;
    it = lst.rbegin();
    it++; it++; it++;
    auto itb = it.base();
    itb--;
    std::cout<<"Current it:"<<*it<<std::endl;
    std::cout<<"Current itb:"<<*itb<<std::endl;
    itb++;
    std::cout<<"After itb:"<<*itb<<std::endl;

    return 0;
}