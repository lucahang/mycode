#include <iostream>


template<typename T>
class shared_ptr{
public:
    void check(){
        if(m_count==nullptr) return;
        (*m_count)--;
        if(*m_count==0){
            delete m_ptr;
            delete m_count;
            m_count=nullptr;
            m_ptr=nullptr;
        }
    }
    T* m_ptr=nullptr;
    int* m_count=nullptr;
    shared_ptr(){
        m_ptr=nullptr;
        m_count=nullptr;
    }
    ~shared_ptr(){
        check();
    }
    explicit shared_ptr(T* ptr){
        m_ptr=ptr;
        if(m_ptr)
            m_count=new int(1);

    }
    explicit shared_ptr(T val){
        m_ptr=new T(val);
        m_count=new int(1);
    }
    shared_ptr(const shared_ptr& other){
        m_ptr=other.m_ptr;
        m_count=other.m_count;
        if(m_count) (*m_count)++;
    }
    shared_ptr( shared_ptr&& other)noexcept{
        if (this == &other) return ;
        m_ptr=other.m_ptr;
        m_count=other.m_count;
        other.m_ptr=nullptr;
        other.m_count=nullptr;
    }
    const T* get()const{return m_ptr;}
    int count(){
        if(!m_count)return 0;
        return *m_count;
    }
    const T& operator*()const {return *m_ptr;}
    const T* operator->()const {return m_ptr;}
    shared_ptr& operator=(const shared_ptr& other){
        if(&other==this) return *this;
        check();
        m_ptr=other.m_ptr;
        m_count=other.m_count;
        if(m_count) (*m_count)++;
        return *this;
    }
    shared_ptr& operator=( shared_ptr&& other)noexcept{
        if (this == &other) return *this;
        check();
        m_ptr=other.m_ptr;
        m_count=other.m_count;
        other.m_ptr=nullptr;
        other.m_count=nullptr;
        return *this;
    }
};
int main (){
    shared_ptr<int> p2;
    {
        std::cout<<"p1.get():"<<std::endl;
        {   
            shared_ptr<int> p1(99);
            std::cout<<"p1.get():"<<p1.get()<<std::endl;
            std::cout<<"*p1:"<<*p1<<std::endl;
            std::cout<<"p1.count():"<<p1.count()<<std::endl;
            p2=p1;
            std::cout<<"p1.get():"<<p1.get()<<std::endl;
            std::cout<<"*p1:"<<*p1<<std::endl;
            std::cout<<"p1.count():"<<p1.count()<<std::endl;
        }
        std::cout<<"p2.get():"<<p2.get()<<std::endl;
        std::cout<<"*p2:"<<*p2<<std::endl;
        std::cout<<"p2.count():"<<p2.count()<<std::endl;
    }
    std::cout<<"p2.get():"<<p2.get()<<std::endl;
    std::cout<<"*p2:"<<*p2<<std::endl;
    std::cout<<"p2.count():"<<p2.count()<<std::endl;
    return 0;
}