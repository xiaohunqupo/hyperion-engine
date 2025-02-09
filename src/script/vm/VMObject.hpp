#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <script/vm/Value.hpp>
#include <script/vm/VMTypeInfo.hpp>
#include <Types.hpp>

#include <sstream>
#include <cstdint>
#include <cmath>

#define DEFAULT_BUCKET_CAPACITY 4
#define COMPUTE_CAPACITY(size) 1ull << (unsigned)std::ceil(std::log(size) / std::log(2.0))

namespace hyperion {
namespace vm {

struct Member
{
    char name[255];
    UInt32 hash;
    Value value;
};

class ObjectMap
{
public:
    ObjectMap(SizeType size);
    ObjectMap(const ObjectMap &other);
    ~ObjectMap();

    ObjectMap &operator=(const ObjectMap &other);

    void Push(UInt32 hash, Member *member);
    Member *Get(UInt32 hash);

    SizeType GetSize() const { return m_size; }

private:
    struct ObjectBucket
    {
        Member **m_data;
        SizeType m_capacity;
        SizeType m_size;

        ObjectBucket();
        ObjectBucket(const ObjectBucket &other);
        ~ObjectBucket();

        ObjectBucket &operator=(const ObjectBucket &other);

        void Resize(SizeType capacity);
        void Push(Member *member);
        bool Lookup(UInt32 hash, Member **out);
    };

    ObjectBucket *m_buckets;
    SizeType m_size;
};

class VMObject
{
public:
    static const UInt32 PROTO_MEMBER_HASH;

    // construct from prototype (holds pointer)
    VMObject(HeapValue *proto);
    VMObject(const Member *members, SizeType size, HeapValue *proto = nullptr);
    VMObject(const VMObject &other);
    ~VMObject();

    VMObject &operator=(const VMObject &other) = delete;
    VMObject &operator=(VMObject &&other) noexcept = delete;

    // compare by memory address
    bool operator==(const VMObject &other) const { return this == &other; }

    Member *LookupMemberFromHash(uint32_t hash) const
        { return m_object_map->Get(hash); }
    Member *GetMembers() const
        { return m_members; }
    Member &GetMember(int index)
        { return m_members[index]; }
    const Member &GetMember(int index) const
        { return m_members[index]; }

    ObjectMap *GetObjectMap() const { return m_object_map; }

    SizeType GetSize() const { return m_object_map->GetSize(); }
    HeapValue *GetPrototype() const { return m_proto; }
    
    void GetRepresentation(
        std::stringstream &ss,
        bool add_type_name = true,
        int depth = 3
    ) const;

private:
    HeapValue *m_proto;

    ObjectMap *m_object_map;
    Member *m_members;
};

} // namespace vm
} // namespace hyperion

#endif
