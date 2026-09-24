#include<iostream>
#include <cstdint>

template<typename T>
struct Node{
    T val_;
    Node* next;
    Node(const T& val):val_(val),next(nullptr){}
};

template<typename T>
class Stack{
private:
    Node<T>* head;
    int32_t size_;

public:
    Stack():head(nullptr),size_(0){}

    Stack(const Stack<T>& other):head(nullptr),size_(other.size_){
        if(other.head == nullptr) return;

        head = new Node<T>(other.head->val_);
        Node<T>* old_node=other.head->next;
        Node<T>* new_node=head;
        while(old_node){
            new_node->next=new Node<T>(old_node->val_);
            new_node=new_node->next;
            old_node = old_node->next;
        }
    }
    
    ~Stack(){
        clear();
    }
    
    void swap(Stack<T>& other){
        Node<T>* temp_node=other.head;
        other.head = head;
        head = temp_node;

        int32_t temp_size=other.size_;
        other.size_ = size_;
        size_=temp_size;
    }

    void clear(){
        while(head){
            Node<T>* cur_node=head;
            head=head->next;
            delete cur_node;
        }
        size_=0;
    }
    
    int32_t size(){return size_;}

    void push(const T& val){
        Node<T>* node = new Node<T>(val);
        node->next=head;
        head=node;
        size_++;
    }
    
    void pop(){
        if(!head) return;
        Node<T>* node = head;
        head=head->next;
        delete node;
        size_--;
    }
    
    void print(){
        std::cout<<"print: ";
        Node<T>* node=head;
        while(node){
            std::cout<<node->val_<<' ';
            node=node->next;
        }
        std::cout<<std::endl;
    }

    Stack<T>& operator=(const Stack<T>& other){
        if (this == &other) return *this;
        Stack tmp(other);
        swap(tmp);
        return *this;
    }
};

