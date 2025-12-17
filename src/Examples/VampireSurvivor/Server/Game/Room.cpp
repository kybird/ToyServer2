#include "Room.h"
#include "../../Protocol/game.pb.h"
#include "System/Dispatcher/MessagePool.h"
#include "System/Network/PacketUtils.h"
#include <limits>

namespace SimpleGame {

Room::Room(int roomId, std::shared_ptr<System::ITimer> timer, std::shared_ptr<UserDB> userDB) 
    : _roomId(roomId), _timer(timer), _waveMgr(_objMgr, _grid, roomId), _userDB(userDB)
{
}

Room::~Room()
{
    Stop();
}

void Room::Start()
{
    if (_timer)
    {
        // 50ms = 20 FPS
        _timerHandle = _timer->SetInterval(1, 50, this);
        LOG_INFO("Room {} Game Loop Started (20 FPS)", _roomId);
    }
    _waveMgr.Start();
}

void Room::Stop()
{
    if (_timer && _timerHandle)
    {
        _timer->CancelTimer(_timerHandle);
        _timerHandle = 0;
    }
}

void Room::OnTimer(uint32_t timerId, void *pParam)
{
    // Fixed Delta Time = 0.05f
    Update(0.05f);
}



// Helper to broadcast Protobuf message
template <typename T>
void BroadcastProto(Room* room, PacketID id, const T& msg) {
    // TODO: Optimize buffer usage
    size_t bodySize = msg.ByteSizeLong();
    
    // Handle large packets by using heap allocation directly
    bool usedHeap = false;
    System::PacketMessage* packet = nullptr;
    
    if (bodySize > System::MessagePool::MAX_PACKET_BODY_SIZE) {
        // Fallback to direct heap allocation for large packets
        size_t allocSize = System::PacketMessage::CalculateAllocSize(static_cast<uint16_t>(bodySize));
        packet = reinterpret_cast<System::PacketMessage*>(::operator new(allocSize));
        new (packet) System::PacketMessage();
        usedHeap = true;
    } else {
        packet = System::MessagePool::AllocatePacket(static_cast<uint16_t>(bodySize));
    }
    
    if (!packet) {
        LOG_ERROR("Failed to allocate packet for broadcast, bodySize={}", bodySize);
        return;
    }
    
    PacketHeader* header = reinterpret_cast<PacketHeader*>(packet->Payload());
    header->size = static_cast<uint16_t>(sizeof(PacketHeader) + bodySize);
    header->id = static_cast<uint16_t>(id);
    msg.SerializeToArray(packet->Payload() + sizeof(PacketHeader), static_cast<int>(bodySize));
    room->Broadcast(packet);
    
    // Release the packet
    if (usedHeap) {
        packet->~PacketMessage();
        ::operator delete(packet);
    } else {
        System::PacketUtils::ReleasePacket(packet);
    }
}

void Room::Enter(std::shared_ptr<Player> player)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _players[player->GetSessionId()] = player;
    
    // Load Persistent Data
    // TODO: Need UserId properly. Assuming SessionId maps to UserId for now or passed separately.
    // In real app, Player object should store UserId from Login.
    // For now, cast SessionId to int (Warning: data loss if big)
    int userId = (int)player->GetSessionId(); 
    if (_userDB) {
        auto skills = _userDB->GetUserSkills(userId);
        player->ApplySkills(skills);
        LOG_INFO("Applied {} skills to Player {}", skills.size(), userId);
    }

    // Add to Game Systems
    _objMgr.AddObject(player);
    _grid.Add(player);

    LOG_INFO("Player {} entered Room {}. Total Players: {}", player->GetSessionId(), _roomId, _players.size());

    // Send Initial State (S_CREATE_ROOM or S_JOIN_ROOM success handled by caller usually, but here we spawn objects)
    // S_SPAWN_OBJECT for this player
    Protocol::S_SpawnObject spawnSelf;
    auto* info = spawnSelf.add_objects();
    info->set_object_id(player->GetId());
    info->set_type(Protocol::ObjectType::PLAYER);
    info->set_x(player->GetX());
    info->set_y(player->GetY());
    info->set_hp(player->GetHp()); // Updated HP
    info->set_max_hp(player->GetMaxHp());
    
    // Broadcast spawn to others
    BroadcastProto(this, PacketID::S_SPAWN_OBJECT, spawnSelf);
}

void Room::Leave(uint64_t sessionId)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _players.find(sessionId);
    if (it != _players.end()) {
        auto player = it->second;
        
        // Remove from Systems
        _grid.Remove(player);
        _objMgr.RemoveObject(player->GetId());
        
        // Broadcast Despawn
        Protocol::S_DespawnObject despawn;
        despawn.add_object_ids(player->GetId());
        BroadcastProto(this, PacketID::S_DESPAWN_OBJECT, despawn);

        _players.erase(it);
        LOG_INFO("Player {} left Room {}", sessionId, _roomId);
    }
}

void Room::Update(float deltaTime)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    // 1. Physics & Logic Update
    auto objects = _objMgr.GetAllObjects();
    Protocol::S_MoveObjectBatch moveBatch;
    
    for (auto& obj : objects) {
        // AI / Logic Update
        obj->Update(deltaTime, this);

        float oldX = obj->GetX();
        float oldY = obj->GetY();

        // Simple Euler Integration
        // TODO: Server-side validation of input? 
        // For now, assume Velocity is set by Input Packet (C_MOVE)
        if (obj->GetVX() != 0 || obj->GetVY() != 0) {
            float newX = oldX + obj->GetVX() * deltaTime;
            float newY = oldY + obj->GetVY() * deltaTime;
            
            // Map Bounds Check (TODO)
             
            obj->SetPos(newX, newY);
            _grid.Update(obj, oldX, oldY);

            // Add to Batch
            auto* move = moveBatch.add_moves();
            move->set_object_id(obj->GetId());
            move->set_x(newX);
            move->set_y(newY);
            move->set_vx(obj->GetVX());
            move->set_vy(obj->GetVY());
        }
    }

    // 2. Broadcast Batched Movement
    if (moveBatch.moves_size() > 0) {
        BroadcastProto(this, PacketID::S_MOVE_OBJECT_BATCH, moveBatch);
    }
}

std::shared_ptr<Player> Room::GetNearestPlayer(float x, float y)
{
    std::shared_ptr<Player> nearest = nullptr;
    float minDistSq = std::numeric_limits<float>::max();

    for (auto& pair : _players) {
        auto p = pair.second;
        float dx = p->GetX() - x;
        float dy = p->GetY() - y;
        float dSq = dx*dx + dy*dy;
        if (dSq < minDistSq) {
            minDistSq = dSq;
            nearest = p;
        }
    }
    return nearest;
}

void Room::Broadcast(System::PacketMessage *packet)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    for (auto &pair : _players)
    {
        // Send to each session
        // Note: Packet refCount is managed. Send increments.
        pair.second->GetSession()->Send(packet);
    }
}

size_t Room::GetPlayerCount()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _players.size();
}

} // namespace SimpleGame
