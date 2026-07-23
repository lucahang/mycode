#include <iostream>

template<typename T>
class unique_ptr{
public:
    explicit unique_ptr():m_ptr(nullptr){}
    ~unique_ptr(){
        if(m_ptr){
            delete m_ptr; m_ptr=nullptr;
        }
    }
    explicit unique_ptr(T* ptr= nullptr):m_ptr(ptr){}
    
    unique_ptr(unique_ptr& other)=delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    unique_ptr(unique_ptr&& other)noexcept{
        m_ptr=other.m_ptr;
        other.m_ptr=nullptr;
    }
    unique_ptr& operator=(unique_ptr&& other)noexcept{
        if(m_ptr!=other){
            delete m_ptr;
            m_ptr=other.m_ptr;
            other.m_ptr=nullptr;
        }
        return *this;
    }
    T* get(){return m_ptr;}
    T& operator*(){if(m_ptr!=nullptr)return *m_ptr;}
    T* operator->(){
        return m_ptr;
    }
private:
    T* m_ptr=nullptr;
};

int main(){
    unique_ptr<int>p1=unique_ptr<int>(new int (99));
    std::cout<<*p1<<std::endl;
    *p1=88;
    std::cout<<*p1.get()<<std::endl;
    return 0;
}