#pragma once
#include "../Module.hpp"

class WorldEditModule : public Module {
public:
    WorldEditModule() : Module("WorldEdit", "//pos1 //pos2 //size //set") {}
    void onInit() override;
};
