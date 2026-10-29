#include "../RoN-Internal/PatchLedger.hpp"
#include "../RoN-Internal/GameplayPolicy.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace RuntimePatches;
    Ledger P;
    float Speed=320,Recoil=8.5f,OtherWeapon=5;
    unsigned char Flags=0b10100100;
    const Owner Player{100,1,10,1},Weapon{200,2,20,1},SecondWeapon{300,3,30,1};
    auto Valid=[](Owner O){return O.Index>0;};
    P.Begin();P.Set(Player,&Speed,900.0f);P.Set(Weapon,&Recoil,0.0f);P.Bit(Player,&Flags,4,false);P.Finish(Valid);
    assert(Speed==900 && Recoil==0 && Flags==0b10100000);
    P.Begin();P.Set(Player,&Speed,P.Original(Player,&Speed)*2);P.Set(SecondWeapon,&OtherWeapon,0.0f);P.Finish(Valid);
    assert(Speed==640 && Recoil==8.5f && OtherWeapon==0 && Flags==0b10100100);
    P.Begin();P.Set(Player,&Speed,P.Original(Player,&Speed)*2);P.Finish(Valid);
    assert(Speed==640 && OtherWeapon==5); // Sliders do not compound, switched weapons are restored.
    P.Restore(Valid);assert(Speed==320 && P.Size()==0);
    P.Begin();P.Bit(Player,&Flags,4,false);P.Finish(Valid);
    Flags|=2;P.Restore(Valid);assert(Flags==0b10100110); // Unrelated engine bits survive.
    P.Begin();P.Set(Player,&Speed,900.0f);P.Finish(Valid);
    Speed=350;P.Restore(Valid);assert(Speed==350); // A newer external value is preserved.
    P.Begin();P.Set(Player,&Speed,900.0f);P.Finish(Valid);
    P.Restore([](Owner){return false;});assert(Speed==900 && P.Size()==0); // Expired objects are never written.
    bool ExistingGod=true;P.Begin();P.Set(Player,&ExistingGod,true);P.Finish(Valid);P.Restore(Valid);assert(ExistingGod);
    assert(GameplayPolicy::CanRequestSurrender(100,0,false,false));
    assert(!GameplayPolicy::CanRequestSurrender(200,100,false,false));
    assert(!GameplayPolicy::CanRequestSurrender(3000,100,true,false));
    assert(!GameplayPolicy::CanRequestSurrender(3000,100,false,true));
    assert(GameplayPolicy::CanRequestSurrender(3000,100,false,false));
    std::cout<<"PASS: reversible gameplay values, stable multipliers, weapon switches, bit isolation, external writes and stale objects\n";
}
