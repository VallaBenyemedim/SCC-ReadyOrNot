#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Keeps only changes owned by this session; expired objects and external writes are preserved.
namespace RuntimePatches {
    struct Owner {
        uintptr_t Address=0;
        int Index=-1;
        uint64_t Name=0,OuterName=0;
        bool operator==(const Owner&) const=default;
    };
    class Ledger {
        struct Entry {
            Owner Object;
            void* Address=nullptr;
            size_t Size=0;
            std::array<unsigned char,32> Original{},Last{},Mask{};
            bool Seen=false;
        };
        std::vector<Entry> Entries;
        Entry* Find(Owner Object,void* Address,size_t Size,unsigned char Mask) {
            auto I=std::find_if(Entries.begin(),Entries.end(),[&](const Entry& E){return E.Object==Object && E.Address==Address && E.Size==Size && E.Mask[0]==Mask;});
            return I==Entries.end()?nullptr:&*I;
        }
        void Write(Owner Object,void* Address,const void* Value,size_t Size,unsigned char Mask=255) {
            if (!Address || Size>32) return;
            auto* E=Find(Object,Address,Size,Mask);
            if (!E) {
                Entry New;New.Object=Object;New.Address=Address;New.Size=Size;New.Mask.fill(Mask);
                std::memcpy(New.Original.data(),Address,Size);Entries.push_back(New);E=&Entries.back();
            }
            const auto* Input=static_cast<const unsigned char*>(Value);
            auto* Output=static_cast<unsigned char*>(Address);
            for(size_t I=0;I<Size;++I) E->Last[I]=Output[I]=(Output[I]&~E->Mask[I])|(Input[I]&E->Mask[I]);
            E->Seen=true;
        }
    public:
        void Begin() {for(auto& E:Entries) E.Seen=false;}
        template<class T> T Original(Owner Object,T* Address) {
            T Result=*Address;
            if(auto* E=Find(Object,Address,sizeof(T),255)) std::memcpy(&Result,E->Original.data(),sizeof(T));
            return Result;
        }
        template<class T> void Set(Owner Object,T* Address,T Value) {static_assert(sizeof(T)<=32);Write(Object,Address,&Value,sizeof(T));}
        void Bit(Owner Object,unsigned char* Address,unsigned char Mask,bool On) {
            const unsigned char Value=On?Mask:0;Write(Object,Address,&Value,1,Mask);
        }
        template<class Valid> void Finish(Valid IsValid) {
            std::erase_if(Entries,[&](const Entry& E) {
                if (!IsValid(E.Object)) return true;
                if(E.Seen) return false;
                auto* Value=static_cast<unsigned char*>(E.Address);
                bool Ours=true;
                for(size_t I=0;I<E.Size;++I) if((Value[I]&E.Mask[I])!=(E.Last[I]&E.Mask[I])) Ours=false;
                if(Ours) for(size_t I=0;I<E.Size;++I) Value[I]=(Value[I]&~E.Mask[I])|(E.Original[I]&E.Mask[I]);
                return true;
            });
        }
        template<class Valid> void Restore(Valid IsValid) {Begin();Finish(IsValid);}
        size_t Size() const {return Entries.size();}
    };
}
