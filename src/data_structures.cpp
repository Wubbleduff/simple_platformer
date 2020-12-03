
#include "data_structures.h"


#if 0

template<typename T>
DynamicArray<T>::DynamicArray()
{
    capacity = 0;
    size = 0;
    data = nullptr;
}

template<typename T>
void DynamicArray<T>::push_back(T item)
{
    maybe_grow(size + 1);
    data[size++] = item;
}

template<typename T>
T DynamicArray<T>::pop_back()
{
    T item = data[size - 1];
    size--;
    return item;
}

template<typename T>
void DynamicArray<T>::resize(int n)
{
    maybe_grow(n);
    size = n;
}

template<typename T>
void DynamicArray<T>::reserve(int items) { maybe_grow(items); }

template<typename T>
void DynamicArray<T>::clear() { size = 0; }


template<typename T>
T &DynamicArray<T>::operator[](int n) { return data[n]; }

template<typename T>
const T &DynamicArray<T>::operator[](int n) const { return data[n]; }

template<typename T>
void DynamicArray<T>::maybe_grow(int items_to_fit)
{
    if(capacity > items_to_fit) return;

    while(capacity <= items_to_fit) capacity *= 2;

    T *old = data;
    data = (T *)Platform::Memory::allocate(capacity * sizeof(T));

    Platform::Memory::memcpy(data, old, size * sizeof(T));

    Patform::Memory::free(old);
}
#endif


