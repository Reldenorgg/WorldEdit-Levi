#include "worldedit.hpp"
#include "we_core.hpp"
#include "core/GameHooks.hpp"
#include "core/memory/Hooks.hpp"
#include <bedrocktools/memory/Signatures.hpp>
#include <bedrocktools/sdk/Offsets.hpp>
#include <bedrocktools/sdk/client/ClientInstance.hpp>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace {
using namespace bedrocktools;
using SendFn = void* (*)(void*, void*);
using GetSenderFn = void* (*)(void*);
using CreateFn = std::shared_ptr<void> (*)(int);

constexpr int kCmdId = 0x4D;
SendFn gSendOrig = nullptr;
GetSenderFn gGetSender = nullptr;
CreateFn gCreate = nullptr;
void* gCmdVtable = nullptr;
we::Region gRegion;

void ensure() {
    if (!gGetSender) gGetSender = reinterpret_cast<GetSenderFn>(memory::resolve(memory::SignatureId::ClientInstanceGetPacketSender));
    if (!gCreate) gCreate = reinterpret_cast<CreateFn>(memory::resolve(memory::SignatureId::MinecraftPacketsCreatePacket));
}

void sendCmd(const std::string& command) {
    ensure();
    void* client = core::gamehooks::clientInstance();
    if (!gCreate || !gGetSender || !gSendOrig || !client) return;
    auto packet = gCreate(kCmdId);
    if (!packet) return;
    auto* payload = reinterpret_cast<std::byte*>(packet.get()) + sdk::offsets::Packet::Size;
    *reinterpret_cast<std::string*>(payload + sdk::offsets::CommandRequestPacketPayload::mCommand) = command;
    *reinterpret_cast<std::uint8_t*>(payload + sdk::offsets::CommandRequestPacketPayload::mOrigin + sdk::offsets::CommandOriginData::mType) = 0;
    *reinterpret_cast<bool*>(payload + sdk::offsets::CommandRequestPacketPayload::mInternalSource) = true;
    void* sender = gGetSender(client);
    if (sender) gSendOrig(sender, packet.get());
}

void say(const std::string& m) {
    const std::string S = "\xC2\xA7";
    sendCmd("/tellraw @s {\"rawtext\":[{\"text\":\"" + S + "b[WE] " + S + "f" + m + "\"}]}");
}

bool playerBlock(we::Pos& out) {
    void* client = core::gamehooks::clientInstance();
    if (!client) return false;
    auto* player = reinterpret_cast<sdk::ClientInstance*>(client)->localPlayer();
    if (!player) return false;
    auto p = player->position();
    out = {(int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z)};
    return true;
}

bool validBlock(const std::string& b) {
    if (b.empty() || b.size() > 64) return false;
    for (char c : b)
        if (!(std::isalnum((unsigned char)c) || c == '_' || c == ':')) return false;
    return true;
}

std::string str(const we::Pos& p) {
    return std::to_string(p.x) + " " + std::to_string(p.y) + " " + std::to_string(p.z);
}

void handle(const std::string& line) {
    std::string rest = line.substr(2);
    auto sp = rest.find(' ');
    std::string name = rest.substr(0, sp);
    std::string arg = sp == std::string::npos ? "" : rest.substr(sp + 1);
    while (!arg.empty() && arg.back() == ' ') arg.pop_back();

    if (name == "pos1" || name == "pos2") {
        we::Pos p;
        if (!playerBlock(p)) { say("Jogador nao encontrado."); return; }
        bool one = name == "pos1";
        (one ? gRegion.a : gRegion.b) = p;
        (one ? gRegion.hasA : gRegion.hasB) = true;
        say(std::string(one ? "Pos1: " : "Pos2: ") + str(p));
        return;
    }
    if (name == "size") {
        if (!gRegion.ready()) { say("Defina //pos1 e //pos2."); return; }
        say("Regiao: " + std::to_string(gRegion.volume()) + " blocos.");
        return;
    }
    if (name == "set") {
        if (!gRegion.ready()) { say("Defina //pos1 e //pos2."); return; }
        if (!validBlock(arg)) { say("Bloco invalido."); return; }
        if (gRegion.volume() > 2000000) { say("Regiao grande demais (max 2000000)."); return; }
        auto cmds = we::fillCommands(gRegion, arg);
        for (auto& c : cmds) sendCmd(c);
        say(std::to_string(gRegion.volume()) + " blocos em " + std::to_string(cmds.size()) + " comandos.");
        return;
    }
    say("Comandos: //pos1 //pos2 //size //set <bloco>");
}

void* sendDetour(void* sender, void* packet) {
    ensure();
    if (!gCmdVtable && gCreate) {
        auto probe = gCreate(kCmdId);
        if (probe) gCmdVtable = *reinterpret_cast<void**>(probe.get());
    }
    if (packet && gCmdVtable && *reinterpret_cast<void**>(packet) == gCmdVtable) {
        auto* payload = reinterpret_cast<std::byte*>(packet) + sdk::offsets::Packet::Size;
        const std::string& cmd = *reinterpret_cast<std::string*>(payload + sdk::offsets::CommandRequestPacketPayload::mCommand);
        if (we::isWeCommand(cmd)) { handle(cmd); return nullptr; }
    }
    return gSendOrig ? gSendOrig(sender, packet) : nullptr;
}
}  // namespace

void WorldEditModule::onInit() {
    const auto target = bedrocktools::memory::resolve(bedrocktools::memory::SignatureId::LoopbackPacketSenderSendToServer);
    if (target && !gSendOrig) {
        bedrocktools::hooks::install(reinterpret_cast<void*>(target),
                                     reinterpret_cast<void*>(sendDetour),
                                     reinterpret_cast<void**>(&gSendOrig));
    }
}
