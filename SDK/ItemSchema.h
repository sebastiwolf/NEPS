#pragma once

#include <cstddef>

#include "Pad.h"
#include "UtlVector.h"
#include "VirtualMethod.h"

enum class WeaponId : short;

template <typename T>
struct UtlMemory {
    T& operator[](int i) noexcept { return memory[i]; };

    T* memory;
    int allocationCount;
    int growSize;
};

template <typename Key, typename Value>
struct Node {
    int left;
    int right;
    int parent;
    int type;
    Key key;
    Value value;
};

template <typename Key, typename Value>
struct UtlMap {
    void* lessFunc;
    UtlMemory<Node<Key, Value>> memory;
    int root;
    int numElements;
    int firstFree;
    int lastAlloc;
    Node<Key, Value>* elements;
};

struct String {
    UtlMemory<char> buffer;
    int length;

    char* data() noexcept { return buffer.memory; }
};

struct PaintKit {
    int id;
    String name;
    String description;
    String itemName;
};

struct StickerKit {
    int id;
    int rarity;
    String name;
    String description;
    String itemName;
};

class EconItemDefinition {
public:
    VIRTUAL_METHOD(WeaponId, getWeaponId, 0, (), (this))
    VIRTUAL_METHOD(const char*, getItemBaseName, 2, (), (this))
    VIRTUAL_METHOD(const char*, getItemTypeName, 3, (), (this))
};

struct ItemListEntry {
    int itemDef;
    int paintKit;
    PAD(20)

    auto weaponId() const noexcept
    {
        return static_cast<WeaponId>(itemDef);
    }
};

class EconLootListDefinition {
public:
    VIRTUAL_METHOD(const char*, getName, 0, (), (this))
    VIRTUAL_METHOD(const UtlVector<ItemListEntry>&, getLootListContents, 1, (), (this))
};

class EconItemSetDefinition {
public:
    VIRTUAL_METHOD(const char*, getLocKey, 1, (), (this))
    VIRTUAL_METHOD(int, getItemCount, 4, (), (this))
    VIRTUAL_METHOD(WeaponId, getItemDef, 5, (int index), (this, index))
    VIRTUAL_METHOD(int, getItemPaintKit, 6, (int index), (this, index))
};

struct EconItemQualityDefinition {
    int value;
    const char* name;
    unsigned weight;
    bool explicitMatchesOnly;
    bool canSupportSet;
    const char* hexColor;
};

class ItemSchema {
public:
    PAD(0x88)
    UtlMap<int, EconItemQualityDefinition> qualities;
    PAD(0x1DC)
    UtlMap<int, PaintKit*> paintKits;
    UtlMap<int, StickerKit*> stickerKits;

    VIRTUAL_METHOD(EconItemDefinition*, getItemDefinitionInterface, 4, (WeaponId id), (this, id))
    VIRTUAL_METHOD(const char*, getRarityName, 19, (uint8_t rarity), (this, rarity))
    VIRTUAL_METHOD(int, getItemSetCount, 28, (), (this))
    VIRTUAL_METHOD(EconItemSetDefinition*, getItemSet, 29, (int index), (this, index))
    VIRTUAL_METHOD(EconLootListDefinition*, getLootList, 32, (int index), (this, index))
    VIRTUAL_METHOD(int, getLootListCount, 34, (), (this))
    VIRTUAL_METHOD(EconItemDefinition*, getItemDefinitionByName, 42, (const char* name), (this, name))
};

class ItemSystem {
public:
    VIRTUAL_METHOD(ItemSchema*, getItemSchema, 0, (), (this))
};
