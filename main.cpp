#include <vector> 
#include <iostream>
#include <cstring>

using namespace std;


enum { STRUCT_SZ = 0, REF_COUNT = 1 };
struct gcHeader {
    union {
        int gcData[2];
        gcHeader* post_gcAddress;
    };
};

vector<gcHeader**> referencesStack;

template <class T>
class stackRef {
public:
    T* ref;
    stackRef() {
        ref = nullptr;
        referencesStack.push_back(reinterpret_cast<gcHeader**>(&ref));
    }

    ~stackRef() {
        referencesStack.pop_back();
    }
};

const int CHUNK_SIZE = 4096;
const int OVERDRAFT = 128;
const int ACTUAL_SIZE = CHUNK_SIZE + OVERDRAFT; 
struct gcChunk {
    gcChunk* next;
    char data[ACTUAL_SIZE];
};

gcChunk* firstChunk;
gcChunk* currentChunk;
int chunkCount;
int currentOffset;

gcHeader* gcRawAlloc(int size, int refCount) {
    if (size > CHUNK_SIZE)
        return nullptr;
    if (currentOffset + size > CHUNK_SIZE) {  
        ++chunkCount;
        currentChunk->next = new gcChunk();
        currentChunk = currentChunk->next;
        currentChunk->next = nullptr;
        currentOffset = 0;
    }
    gcHeader* new_obj = reinterpret_cast<gcHeader*>(&(currentChunk->data[currentOffset]));
    new_obj->gcData[STRUCT_SZ] = size;
    new_obj->gcData[REF_COUNT] = (refCount << 1) | 1;
    currentOffset += size;
    if (currentOffset % 4)
        currentOffset += 4 - currentOffset % 4;
    return new_obj;
}

template <class T>
T* gcAlloc() {
    return reinterpret_cast<T*>(gcRawAlloc(sizeof(T), T::refCount));

}


void gcInit() {
    firstChunk = currentChunk = new gcChunk;
    firstChunk->next = nullptr;
    currentOffset = 0;
    chunkCount = 1;
}



bool isPointer(gcHeader a) {
    return (a.gcData[REF_COUNT] & 1) == 0;
}

void gcMove(gcHeader** current) {
    if (*current == nullptr)
        return;
    if (isPointer(**current)) {
        (*current) = (*current)->post_gcAddress;
        return;
    }
    gcHeader* new_obj = gcRawAlloc((*current)->gcData[STRUCT_SZ], (*current)->gcData[REF_COUNT]);
    memcpy(new_obj, (*current), sizeof(char) * (*current)->gcData[STRUCT_SZ]);

    gcHeader** iterator = reinterpret_cast<gcHeader**>(new_obj) + 1;


    (*current)->post_gcAddress = new_obj;
    (*current) = new_obj;
    int refCount = new_obj->gcData[REF_COUNT] >> 1;
    for (int i = 0; i < refCount; ++i, ++iterator)
        gcMove(iterator);
}

void gcCollect() {
    gcChunk* newFirstChunk = currentChunk = new gcChunk;
    currentChunk->next = nullptr;
    currentOffset = 0;
    chunkCount = 1;
    for (auto i = referencesStack.begin(); i != referencesStack.end(); ++i)
        gcMove(*i);

    gcChunk* iter = firstChunk;
    firstChunk = newFirstChunk;
    while (iter != nullptr) {
        gcChunk* t = iter->next;
        delete[] iter;
        iter = t;
    }
}


struct searchTree {
    gcHeader gc;
    searchTree* left;
    searchTree* right;
    int key;
    static const int refCount = 2;
};

void stAdd(searchTree*& target, int key) {
    if (target == nullptr) {

        target = gcAlloc<searchTree>();
        target->left = target->right = nullptr;
        target->key = key;

        return;
    }
    if (target->key == key)
        return;
    if (target->key < key)
        stAdd(target->left, key);
    else
        stAdd(target->right, key);
}



searchTree* stFind(searchTree* target, int key) {
    if (target == nullptr || target->key == key)
        return target;
    if (target->key < key)
        return stFind(target->left, key);
    else
        return stFind(target->right, key);
}


void stPrint(searchTree* t, int indent = 0) {
    if (t == nullptr)
        return;
    for (int i = 0; i < indent; ++i)
        cout << "  ";
    cout << t << ' ' << t->key << endl;
    stPrint(t->left, indent + 1);
    stPrint(t->right, indent + 1);

}


void stCut(searchTree*& target, int key) {
    if (target == nullptr || target->key == key) {
        target = nullptr;
        return;
    }
    if (target->key < key)
        stCut(target->left, key);
    else
        stCut(target->right, key);
}




int main() {
    gcInit();
    stackRef<searchTree> root;

    stAdd(root.ref, 2);
    stAdd(root.ref, 1);
    stAdd(root.ref, 3);
    stAdd(root.ref, 6);
    stAdd(root.ref, 5);
    stAdd(root.ref, 4);
    stAdd(root.ref, 8);
    stackRef<searchTree> additionalRef;
    additionalRef.ref = stFind(root.ref, 3);
    cout << "Before GC" << endl;
    cout << additionalRef.ref << ' ' << currentOffset << endl << endl;
    stPrint(root.ref);
    cout << endl;

    gcCollect();
    cout << "After GC" << endl;
    cout << additionalRef.ref << ' ' << currentOffset << endl << endl;
    stPrint(root.ref);

    cout << endl;
    stCut(root.ref, 5);
    gcCollect();
    cout << "Deleted some elements and GC'd." << endl;
    cout << additionalRef.ref << ' ' << currentOffset << endl << endl;
    stPrint(root.ref);

    return 0;


}
