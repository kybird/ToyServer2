#include "Game/Emitter/ZoneEmitter.h"
#include "Common/GamePackets.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Entity/ProjectileFactory.h"
#include "Game/DamageEmitter.h"
#include "Game/Room.h"
#include "System/Utility/FastRandom.h"
#include <algorithm>
#include <vector>

namespace SimpleGame {

ZoneEmitter::ZoneEmitter(float initialTimer) : _timer(initialTimer)
{
}

void ZoneEmitter::Update(
    float dt, Room *room, DamageEmitter *emitter, std::shared_ptr<Player> owner, const WeaponStats &stats
)
{
    _timer += dt;
    if (_timer >= stats.tickInterval)
    {
        _timer -= stats.tickInterval;

        float px = owner->GetX();
        float py = owner->GetY();

        int targetCount = std::max<int>(1, stats.projectileCount);
        auto monsters = room->GetMonstersInRange(px, py, 30.0f);

        System::Utility::FastRandom rng;

        for (int i = 0; i < targetCount; ++i)
        {
            if (monsters.empty())
                break;

            int randomIndex = rng.NextInt(0, static_cast<int>(monsters.size()) - 1);
            auto targetMonster = monsters[randomIndex];
            monsters.erase(monsters.begin() + randomIndex);

            if (targetMonster && !targetMonster->IsDead())
            {
                // Applying Lightning Ring splash radius logic
                float splashRadius = (stats.width > 0.0f ? stats.width : 1.5f) * stats.areaMult;

                auto splashTargets =
                    room->GetMonstersInRange(targetMonster->GetX(), targetMonster->GetY(), splashRadius);

                float cx = targetMonster->GetX();
                float cy = targetMonster->GetY();

                std::vector<int32_t> hitTargetIds;
                std::vector<int32_t> hitDamageValues;
                std::vector<bool> hitCrits;

                for (auto &sMonster : splashTargets)
                {
                    if (!sMonster->IsDead())
                    {
                        int damage = stats.damage;
                        bool isCrit = false;
                        if (stats.critChance > 0.0f && rng.NextFloat() < stats.critChance)
                        {
                            damage = static_cast<int>(damage * stats.critDamageMult);
                            isCrit = true;
                        }

                        sMonster->TakeDamage(damage, room);

                        hitTargetIds.push_back(sMonster->GetId());
                        hitDamageValues.push_back(damage);
                        hitCrits.push_back(isCrit);
                    }
                }

                if (!hitTargetIds.empty())
                {
                    Protocol::S_DamageEffect damageMsg;
                    damageMsg.set_skill_id(emitter->GetSkillId());
                    for (size_t k = 0; k < hitTargetIds.size(); ++k)
                    {
                        damageMsg.add_target_ids(hitTargetIds[k]);
                        damageMsg.add_damage_values(hitDamageValues[k]);
                        damageMsg.add_is_critical(hitCrits[k]);
                    }
                    room->BroadcastPacket(S_DamageEffectPacket(std::move(damageMsg)));
                }

                // Broadcast skill effect (Visuals) at the center
                Protocol::S_SkillEffect skillMsg;
                skillMsg.set_skill_id(emitter->GetSkillId());
                skillMsg.set_caster_id(owner->GetId());
                skillMsg.set_x(cx);
                skillMsg.set_y(cy);
                skillMsg.set_radius(splashRadius);
                skillMsg.set_duration_seconds(0.2f);
                room->BroadcastPacket(S_SkillEffectPacket(std::move(skillMsg)));
            }
        }
    }
}

} // namespace SimpleGame
