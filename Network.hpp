#pragma once

#include "raylib.h"
#include "raymath.h"
#include "Weapon.hpp"
#include "SquadAI.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <array>
#include <vector>
#include <string>
#include <algorithm>

enum class NetRole : uint8_t {
    NONE = 0,
    HOST,
    CLIENT
};

enum class NetPacketType : uint8_t {
    JOIN_REQUEST = 1,
    JOIN_ACCEPT  = 2,
    JOIN_REJECT  = 3,
    START_MATCH  = 4,
    CLIENT_INPUT = 5,
    WORLD_SNAP   = 6,
    DISCONNECT   = 7
};

enum class NetEventType : uint8_t {
    NONE = 0,
    SHOT_TRACER = 1,
    EXPLOSION   = 2
};

#pragma pack(push, 1)

struct NetJoinPacket {
    uint32_t magic = 0x53515544; // "SQUD"
    uint8_t type = 0;
    uint8_t assignedSlot = 0;    // 0 = Alpha (Host), 1 = Bravo, 2 = Charlie, 3 = Delta
    uint8_t selectedPhase = 1;
    uint8_t matchStarted = 0;
};

struct NetClientInputPacket {
    uint32_t magic = 0x53515544;
    uint8_t type = (uint8_t)NetPacketType::CLIENT_INPUT;
    uint8_t slot = 1;
    uint8_t weaponSlot = 1;
    uint8_t flags = 0;           // bit0: crouched, bit1: sprinting, bit2: interactF, bit3: pingMid
    uint32_t sequence = 0;

    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float yaw = 0.0f, pitch = 0.0f;

    uint8_t hasShot = 0;
    uint8_t weaponId = 0;
    uint8_t isMelee = 0;
    uint8_t pad1 = 0;
    float shotOriginX = 0.0f, shotOriginY = 0.0f, shotOriginZ = 0.0f;
    float shotDirX = 0.0f, shotDirY = 0.0f, shotDirZ = 0.0f;

    uint8_t hasGrenade = 0;
    uint8_t isFlashbang = 0;
    uint16_t pad2 = 0;
    float nadePosX = 0.0f, nadePosY = 0.0f, nadePosZ = 0.0f;
    float nadeVelX = 0.0f, nadeVelY = 0.0f, nadeVelZ = 0.0f;
};

struct NetSquadMemberState {
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float yaw = 0.0f, pitch = 0.0f;
    float hp = 100.0f;
    float aggro = 0.0f;
    float suppression = 0.0f;
    float bleedoutTimer = 25.0f;
    float reviveProgress = 0.0f;
    uint8_t flags = 0;           // bit0: isHuman, bit1: isCrouched, bit2: isDowned, bit3: isSuppressed, bit4: hasPingOrder
    uint8_t weaponSlot = 1;
    uint16_t pad = 0;
};

struct NetEnemyState {
    int16_t id = 0;
    int16_t currentTargetId = -1;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float hp = 0.0f;
    float maxHp = 150.0f;
    float suppression = 0.0f;
    uint8_t flags = 0;           // bit0: alive, bit1: isSuppressed
    uint8_t pad[3] = { 0, 0, 0 };
};

struct NetTerminalState {
    float progress = 0.0f;
    uint8_t hacked = 0;
    uint8_t pad[3] = { 0, 0, 0 };
};

struct NetBarrelState {
    float hp = 45.0f;
    uint8_t exploded = 0;
    uint8_t pad[3] = { 0, 0, 0 };
};

struct NetWorldEvent {
    uint32_t eventId = 0;
    uint8_t type = 0;            // NetEventType
    uint8_t sourceSlot = 0;
    uint8_t extra = 0;           // isFlashbang ou isStealth
    uint8_t pad = 0;
    float startX = 0.0f, startY = 0.0f, startZ = 0.0f;
    float endX = 0.0f, endY = 0.0f, endZ = 0.0f;
};

struct NetWorldSnapshotPacket {
    static constexpr int MAX_ENEMIES = 12;
    static constexpr int MAX_TERMINALS = 6;
    static constexpr int MAX_BARRELS = 8;
    static constexpr int MAX_EVENTS = 6;

    uint32_t magic = 0x53515544;
    uint8_t type = (uint8_t)NetPacketType::WORLD_SNAP;
    uint8_t phase = 1;
    uint8_t missionState = 0;
    uint8_t currentOrder = 1;
    uint8_t connectedMask = 1;   // bit0: Alpha, bit1: Bravo, bit2: Charlie, bit3: Delta
    uint8_t extractionActive = 0;
    uint8_t enemyCount = 0;
    uint8_t terminalCount = 0;
    uint8_t barrelCount = 0;
    uint8_t eventCount = 0;
    uint16_t pad = 0;

    uint32_t tick = 0;
    float extractionProgress = 0.0f;
    float missionTimer = 0.0f;

    NetSquadMemberState squad[4];
    NetEnemyState enemies[MAX_ENEMIES];
    NetTerminalState terminals[MAX_TERMINALS];
    NetBarrelState barrels[MAX_BARRELS];
    NetWorldEvent events[MAX_EVENTS];
};

#pragma pack(pop)

struct RemoteClientSlot {
    bool connected = false;
    sockaddr_in addr = {};
    float timeoutTimer = 0.0f;
    std::string ipString = "";
    NetClientInputPacket latestInput = {};
    bool hasPendingShot = false;
    bool hasPendingGrenade = false;
    bool hasPendingPing = false;
};

class LanNetworkManager {
public:
    static constexpr uint16_t DEFAULT_PORT = 27015;
    static constexpr float CLIENT_TIMEOUT_SEC = 6.0f;

    NetRole role = NetRole::NONE;
    int localSlot = 0; // 0 = Alpha (Host), 1 = Bravo, 2 = Charlie, 3 = Delta
    uint16_t port = DEFAULT_PORT;

    std::string hostIpInput = "127.0.0.1";
    std::string detectedLocalIp = "127.0.0.1";
    std::string statusMessage = "Pronto para criar sala ou conectar via IP local.";

    bool clientConnected = false;
    bool matchStarted = false;

    // Slots 1..3 correspondem a Bravo, Charlie e Delta
    std::array<RemoteClientSlot, 4> clientSlots;

    // Último snapshot recebido pelo cliente
    NetWorldSnapshotPacket latestSnapshot = {};
    bool hasSnapshot = false;
    uint32_t lastProcessedEventId = 0;

    // Fila de eventos gerados no Host para replicar aos clientes
    std::vector<NetWorldEvent> outgoingEvents;
    uint32_t nextEventId = 1;

    // Inputs de disparo/granada enviados pelo cliente neste frame
    bool queuedClientShot = false;
    uint8_t queuedShotWeaponId = 0;
    bool queuedShotMelee = false;
    Vector3 queuedShotOrigin = { 0, 0, 0 };
    Vector3 queuedShotDir = { 0, 0, -1 };

    bool queuedClientGrenade = false;
    bool queuedGrenadeFlash = false;
    Vector3 queuedGrenadePos = { 0, 0, 0 };
    Vector3 queuedGrenadeVel = { 0, 0, 0 };
    bool queuedClientPing = false;

    LanNetworkManager() {
        detectedLocalIp = DetectLocalIPv4();
    }

    ~LanNetworkManager() {
        Shutdown();
    }

    bool IsActive() const {
        return role != NetRole::NONE;
    }

    bool IsHost() const {
        return role == NetRole::HOST;
    }

    bool IsClient() const {
        return role == NetRole::CLIENT;
    }

    bool IsSlotHuman(int slot) const {
        if (slot == 0) return true;
        if (slot < 1 || slot > 3) return false;
        if (role == NetRole::HOST) return clientSlots[slot].connected;
        if (role == NetRole::CLIENT) return ((latestSnapshot.connectedMask & (1 << slot)) != 0);
        return false;
    }

    int GetConnectedPlayerCount() const {
        if (role == NetRole::HOST) {
            int count = 1;
            for (int s = 1; s <= 3; s++) {
                if (clientSlots[s].connected) count++;
            }
            return count;
        }
        if (role == NetRole::CLIENT && hasSnapshot) {
            int count = 0;
            for (int s = 0; s < 4; s++) {
                if (latestSnapshot.connectedMask & (1 << s)) count++;
            }
            return std::max(1, count);
        }
        return 1;
    }

    static const char* GetSlotCallsign(int slot) {
        switch (slot) {
            case 0:  return "ALPHA (Lider)";
            case 1:  return "BRAVO";
            case 2:  return "CHARLIE";
            case 3:  return "DELTA";
            default: return "OPERADOR";
        }
    }

    bool StartHost(uint16_t bindPort = DEFAULT_PORT) {
        Shutdown();
        detectedLocalIp = DetectLocalIPv4();

        sockFd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockFd < 0) {
            statusMessage = "Erro ao criar socket UDP.";
            return false;
        }

        int opt = 1;
        setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (!SetNonBlocking(sockFd)) {
            close(sockFd);
            sockFd = -1;
            statusMessage = "Erro ao configurar socket nao-bloqueante.";
            return false;
        }

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(bindPort);

        if (bind(sockFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sockFd);
            sockFd = -1;
            statusMessage = "Porta UDP 27015 ocupada. Feche outra instancia.";
            return false;
        }

        port = bindPort;
        role = NetRole::HOST;
        localSlot = 0;
        matchStarted = false;
        outgoingEvents.clear();
        nextEventId = 1;

        for (int i = 0; i < 4; i++) {
            clientSlots[i] = RemoteClientSlot();
        }

        statusMessage = TextFormat("Sala criada! Compartilhe seu IP: %s", detectedLocalIp.c_str());
        return true;
    }

    bool ConnectToHost(const std::string& targetIp, uint16_t targetPort = DEFAULT_PORT) {
        Shutdown();

        sockaddr_in dest = {};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(targetPort);
        if (inet_pton(AF_INET, targetIp.c_str(), &dest.sin_addr) <= 0) {
            statusMessage = "Endereco IP invalido. Use formato ex: 192.168.1.10";
            return false;
        }

        sockFd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockFd < 0) {
            statusMessage = "Erro ao abrir socket UDP de cliente.";
            return false;
        }

        if (!SetNonBlocking(sockFd)) {
            close(sockFd);
            sockFd = -1;
            statusMessage = "Erro ao configurar socket UDP.";
            return false;
        }

        hostAddr = dest;
        port = targetPort;
        role = NetRole::CLIENT;
        clientConnected = false;
        matchStarted = false;
        hasSnapshot = false;
        lastProcessedEventId = 0;
        joinRetryTimer = 0.0f;

        SendJoinRequest();
        statusMessage = TextFormat("Conectando a %s:%d...", targetIp.c_str(), targetPort);
        return true;
    }

    void Shutdown() {
        if (sockFd >= 0) {
            if (role == NetRole::CLIENT && clientConnected) {
                NetJoinPacket bye = {};
                bye.type = (uint8_t)NetPacketType::DISCONNECT;
                bye.assignedSlot = (uint8_t)localSlot;
                sendto(sockFd, &bye, sizeof(bye), 0, (sockaddr*)&hostAddr, sizeof(hostAddr));
            }
            close(sockFd);
            sockFd = -1;
        }

        role = NetRole::NONE;
        localSlot = 0;
        clientConnected = false;
        matchStarted = false;
        hasSnapshot = false;
        outgoingEvents.clear();
        for (int i = 0; i < 4; i++) {
            clientSlots[i] = RemoteClientSlot();
        }
    }

    void BroadcastStartMatch(uint8_t selectedPhase) {
        if (role != NetRole::HOST || sockFd < 0) return;
        matchStarted = true;

        NetJoinPacket pkt = {};
        pkt.type = (uint8_t)NetPacketType::START_MATCH;
        pkt.selectedPhase = selectedPhase;
        pkt.matchStarted = 1;

        for (int s = 1; s <= 3; s++) {
            if (!clientSlots[s].connected) continue;
            pkt.assignedSlot = (uint8_t)s;
            sendto(sockFd, &pkt, sizeof(pkt), 0, (sockaddr*)&clientSlots[s].addr, sizeof(clientSlots[s].addr));
        }
    }

    void QueueEvent(NetEventType type, uint8_t sourceSlot, Vector3 start, Vector3 end, uint8_t extra = 0) {
        if (role != NetRole::HOST) return;
        NetWorldEvent ev = {};
        ev.eventId = nextEventId++;
        ev.type = (uint8_t)type;
        ev.sourceSlot = sourceSlot;
        ev.extra = extra;
        ev.startX = start.x; ev.startY = start.y; ev.startZ = start.z;
        ev.endX = end.x;     ev.endY = end.y;     ev.endZ = end.z;

        outgoingEvents.push_back(ev);
        if (outgoingEvents.size() > 16) {
            outgoingEvents.erase(outgoingEvents.begin());
        }
    }

    void QueueClientFire(WeaponId wId, bool isMelee, Vector3 origin, Vector3 dir) {
        if (role != NetRole::CLIENT) return;
        queuedClientShot = true;
        queuedShotWeaponId = (uint8_t)wId;
        queuedShotMelee = isMelee;
        queuedShotOrigin = origin;
        queuedShotDir = dir;
    }

    void QueueClientGrenade(bool isFlash, Vector3 origin, Vector3 vel) {
        if (role != NetRole::CLIENT) return;
        queuedClientGrenade = true;
        queuedGrenadeFlash = isFlash;
        queuedGrenadePos = origin;
        queuedGrenadeVel = vel;
    }

    void QueueClientPing() {
        if (role != NetRole::CLIENT) return;
        queuedClientPing = true;
    }

    // Atualização de rede no Lobby e durante a Partida
    void PollPackets(float dt, uint8_t currentPhase) {
        if (role == NetRole::NONE || sockFd < 0) return;

        if (role == NetRole::HOST) {
            PollHostPackets(dt, currentPhase);
        } else if (role == NetRole::CLIENT) {
            PollClientPackets(dt);
        }
    }

    void SendClientInput(float dt, const SourcePlayer& player, int weaponSlot, bool interactF) {
        if (role != NetRole::CLIENT || !clientConnected || sockFd < 0) return;

        netSendTimer += dt;
        bool hasInstantAction = (queuedClientShot || queuedClientGrenade || queuedClientPing);
        if (!hasInstantAction && netSendTimer < (1.0f / 45.0f)) return;
        netSendTimer = 0.0f;

        NetClientInputPacket pkt = {};
        pkt.slot = (uint8_t)localSlot;
        pkt.weaponSlot = (uint8_t)weaponSlot;
        pkt.sequence = ++clientSeq;

        uint8_t flags = 0;
        if (player.isCrouched)  flags |= (1 << 0);
        if (player.isSprinting) flags |= (1 << 1);
        if (interactF)          flags |= (1 << 2);
        if (queuedClientPing)   flags |= (1 << 3);
        pkt.flags = flags;

        pkt.posX = player.pos.x;
        pkt.posY = player.pos.y;
        pkt.posZ = player.pos.z;
        pkt.yaw = player.yaw;
        pkt.pitch = player.pitch;

        if (queuedClientShot) {
            pkt.hasShot = 1;
            pkt.weaponId = queuedShotWeaponId;
            pkt.isMelee = queuedShotMelee ? 1 : 0;
            pkt.shotOriginX = queuedShotOrigin.x;
            pkt.shotOriginY = queuedShotOrigin.y;
            pkt.shotOriginZ = queuedShotOrigin.z;
            pkt.shotDirX = queuedShotDir.x;
            pkt.shotDirY = queuedShotDir.y;
            pkt.shotDirZ = queuedShotDir.z;
        }

        if (queuedClientGrenade) {
            pkt.hasGrenade = 1;
            pkt.isFlashbang = queuedGrenadeFlash ? 1 : 0;
            pkt.nadePosX = queuedGrenadePos.x;
            pkt.nadePosY = queuedGrenadePos.y;
            pkt.nadePosZ = queuedGrenadePos.z;
            pkt.nadeVelX = queuedGrenadeVel.x;
            pkt.nadeVelY = queuedGrenadeVel.y;
            pkt.nadeVelZ = queuedGrenadeVel.z;
        }

        sendto(sockFd, &pkt, sizeof(pkt), 0, (sockaddr*)&hostAddr, sizeof(hostAddr));

        queuedClientShot = false;
        queuedClientGrenade = false;
        queuedClientPing = false;
    }

    void SendHostSnapshot(float dt, const NetWorldSnapshotPacket& snap) {
        if (role != NetRole::HOST || sockFd < 0) return;

        netSendTimer += dt;
        if (netSendTimer < (1.0f / 30.0f) && snap.eventCount == 0) return;
        netSendTimer = 0.0f;

        for (int s = 1; s <= 3; s++) {
            if (!clientSlots[s].connected) continue;
            sendto(sockFd, &snap, sizeof(snap), 0, (sockaddr*)&clientSlots[s].addr, sizeof(clientSlots[s].addr));
        }

        if (snap.eventCount > 0) {
            outgoingEvents.clear();
        }
    }

private:
    int sockFd = -1;
    sockaddr_in hostAddr = {};
    float joinRetryTimer = 0.0f;
    float netSendTimer = 0.0f;
    uint32_t clientSeq = 0;

    static bool SetNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) return false;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
    }

    static std::string DetectLocalIPv4() {
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == -1) return "127.0.0.1";

        std::string found = "127.0.0.1";
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                char buf[INET_ADDRSTRLEN] = { 0 };
                sockaddr_in* sa = (sockaddr_in*)ifa->ifa_addr;
                inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
                std::string ip(buf);
                if (ip != "127.0.0.1" && ip.rfind("169.254.", 0) != 0) {
                    found = ip;
                    break;
                }
            }
        }
        freeifaddrs(ifaddr);
        return found;
    }

    static bool SameSockAddr(const sockaddr_in& a, const sockaddr_in& b) {
        return (a.sin_addr.s_addr == b.sin_addr.s_addr) && (a.sin_port == b.sin_port);
    }

    void SendJoinRequest() {
        if (sockFd < 0) return;
        NetJoinPacket req = {};
        req.type = (uint8_t)NetPacketType::JOIN_REQUEST;
        sendto(sockFd, &req, sizeof(req), 0, (sockaddr*)&hostAddr, sizeof(hostAddr));
    }

    void PollHostPackets(float dt, uint8_t currentPhase) {
        // Verifica timeout dos clientes conectados
        for (int s = 1; s <= 3; s++) {
            if (!clientSlots[s].connected) continue;
            clientSlots[s].timeoutTimer += dt;
            if (clientSlots[s].timeoutTimer >= CLIENT_TIMEOUT_SEC) {
                clientSlots[s] = RemoteClientSlot();
                statusMessage = TextFormat("%s desconectou (Bot IA assumiu).", GetSlotCallsign(s));
            }
        }

        uint8_t buffer[2048];
        sockaddr_in sender = {};
        socklen_t senderLen = sizeof(sender);

        while (true) {
            ssize_t bytes = recvfrom(sockFd, buffer, sizeof(buffer), 0, (sockaddr*)&sender, &senderLen);
            if (bytes < 5) break;

            uint32_t magic = 0;
            std::memcpy(&magic, buffer, sizeof(uint32_t));
            if (magic != 0x53515544) continue;

            uint8_t pktType = buffer[4];

            if (pktType == (uint8_t)NetPacketType::JOIN_REQUEST && bytes >= (ssize_t)sizeof(NetJoinPacket)) {
                int assigned = -1;
                for (int s = 1; s <= 3; s++) {
                    if (clientSlots[s].connected && SameSockAddr(clientSlots[s].addr, sender)) {
                        assigned = s;
                        break;
                    }
                }
                if (assigned == -1) {
                    for (int s = 1; s <= 3; s++) {
                        if (!clientSlots[s].connected) {
                            assigned = s;
                            break;
                        }
                    }
                }

                if (assigned != -1) {
                    char ipBuf[INET_ADDRSTRLEN] = { 0 };
                    inet_ntop(AF_INET, &sender.sin_addr, ipBuf, sizeof(ipBuf));

                    clientSlots[assigned].connected = true;
                    clientSlots[assigned].addr = sender;
                    clientSlots[assigned].timeoutTimer = 0.0f;
                    clientSlots[assigned].ipString = ipBuf;

                    NetJoinPacket ack = {};
                    ack.type = (uint8_t)NetPacketType::JOIN_ACCEPT;
                    ack.assignedSlot = (uint8_t)assigned;
                    ack.selectedPhase = currentPhase;
                    ack.matchStarted = matchStarted ? 1 : 0;
                    sendto(sockFd, &ack, sizeof(ack), 0, (sockaddr*)&sender, sizeof(sender));

                    statusMessage = TextFormat("%s entrou na sala (%s)!", GetSlotCallsign(assigned), ipBuf);
                } else {
                    NetJoinPacket rej = {};
                    rej.type = (uint8_t)NetPacketType::JOIN_REJECT;
                    sendto(sockFd, &rej, sizeof(rej), 0, (sockaddr*)&sender, sizeof(sender));
                }
            } else if (pktType == (uint8_t)NetPacketType::CLIENT_INPUT && bytes >= (ssize_t)sizeof(NetClientInputPacket)) {
                NetClientInputPacket inPkt;
                std::memcpy(&inPkt, buffer, sizeof(NetClientInputPacket));
                int s = (int)inPkt.slot;
                if (s >= 1 && s <= 3 && clientSlots[s].connected) {
                    clientSlots[s].timeoutTimer = 0.0f;
                    clientSlots[s].addr = sender;
                    if (inPkt.sequence >= clientSlots[s].latestInput.sequence) {
                        clientSlots[s].latestInput = inPkt;
                    }
                    if (inPkt.hasShot)    clientSlots[s].hasPendingShot = true;
                    if (inPkt.hasGrenade) clientSlots[s].hasPendingGrenade = true;
                    if (inPkt.flags & (1 << 3)) clientSlots[s].hasPendingPing = true;
                }
            } else if (pktType == (uint8_t)NetPacketType::DISCONNECT && bytes >= (ssize_t)sizeof(NetJoinPacket)) {
                NetJoinPacket bye;
                std::memcpy(&bye, buffer, sizeof(NetJoinPacket));
                int s = (int)bye.assignedSlot;
                if (s >= 1 && s <= 3 && clientSlots[s].connected) {
                    clientSlots[s] = RemoteClientSlot();
                    statusMessage = TextFormat("%s saiu da partida.", GetSlotCallsign(s));
                }
            }
        }
    }

    void PollClientPackets(float dt) {
        if (!clientConnected) {
            joinRetryTimer += dt;
            if (joinRetryTimer >= 0.75f) {
                joinRetryTimer = 0.0f;
                SendJoinRequest();
            }
        }

        uint8_t buffer[2048];
        sockaddr_in sender = {};
        socklen_t senderLen = sizeof(sender);

        while (true) {
            ssize_t bytes = recvfrom(sockFd, buffer, sizeof(buffer), 0, (sockaddr*)&sender, &senderLen);
            if (bytes < 5) break;

            uint32_t magic = 0;
            std::memcpy(&magic, buffer, sizeof(uint32_t));
            if (magic != 0x53515544) continue;

            uint8_t pktType = buffer[4];

            if (pktType == (uint8_t)NetPacketType::JOIN_ACCEPT && bytes >= (ssize_t)sizeof(NetJoinPacket)) {
                NetJoinPacket ack;
                std::memcpy(&ack, buffer, sizeof(NetJoinPacket));
                clientConnected = true;
                localSlot = std::clamp((int)ack.assignedSlot, 1, 3);
                if (ack.matchStarted) matchStarted = true;
                statusMessage = TextFormat("Conectado como %s! Aguardando inicio...", GetSlotCallsign(localSlot));
            } else if (pktType == (uint8_t)NetPacketType::JOIN_REJECT) {
                statusMessage = "Sala cheia (4/4 jogadores ativos).";
                Shutdown();
                break;
            } else if (pktType == (uint8_t)NetPacketType::START_MATCH && bytes >= (ssize_t)sizeof(NetJoinPacket)) {
                NetJoinPacket st;
                std::memcpy(&st, buffer, sizeof(NetJoinPacket));
                clientConnected = true;
                if (st.assignedSlot >= 1 && st.assignedSlot <= 3) localSlot = st.assignedSlot;
                matchStarted = true;
                statusMessage = "Host iniciou a missao!";
            } else if (pktType == (uint8_t)NetPacketType::WORLD_SNAP && bytes >= (ssize_t)sizeof(NetWorldSnapshotPacket)) {
                NetWorldSnapshotPacket snap;
                std::memcpy(&snap, buffer, sizeof(NetWorldSnapshotPacket));
                if (!hasSnapshot || snap.tick >= latestSnapshot.tick) {
                    latestSnapshot = snap;
                    hasSnapshot = true;
                    clientConnected = true;
                    matchStarted = true;
                }
            }
        }
    }
};