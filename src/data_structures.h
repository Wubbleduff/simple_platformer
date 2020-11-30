
#pragma once

template<class T>
struct DynamicArray
{
    public:
        int capacity; // How many items alloacted
        int size; // How many items in list
        T *data;

        void push_back(const T &item);
        T pop_back();
        void resize(int items);
        void reserve(int items);
        void clear();

        

        T &operator[](int n);
        const T &operator[](int n) const;

    private:
        void maybe_grow(int items_to_fit);
};

