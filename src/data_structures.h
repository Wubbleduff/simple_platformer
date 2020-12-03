
#pragma once

#include "Platform.h"

template<typename T>
class DynamicArray
{
    public:
        int capacity = 1; // How many items alloacted
        int size = 0; // How many items in list
        T *data = nullptr;

        void init()
        {
            capacity = 1;
            size = 0;
            data = nullptr;
        }

        void push_back(T item)
        {
            maybe_grow(size + 1);
            data[size++] = item;
        }

        T pop_back()
        {
            T item = data[size - 1];
            size--;
            return item;
        }

        void resize(int items)
        {
            maybe_grow(n);
            size = n;
        }

        void reserve(int items)
        {
            maybe_grow(items);
        }

        void clear()
        {
            size = 0;
        }


        //DynamicArray() {}

        T &operator[](int n)
        {
            return data[n];
        }

        const T &operator[](int n) const
        {
            return data[n];
        }


    private:
        void maybe_grow(int items_to_fit)
        {
            if(capacity > items_to_fit) return;

            while(capacity <= items_to_fit) capacity *= 2;

            T *old = data;
            data = (T *)Platform::Memory::allocate(capacity * sizeof(T));

            Platform::Memory::memcpy(data, old, size * sizeof(T));

            Platform::Memory::free(old);
        }
};

