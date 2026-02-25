#include "Game/TileMap.h"
#include "System/ILog.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>

namespace SimpleGame {

bool TileMap::LoadFromJson(const std::string &path)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR("TileMap: Failed to open map file {}", path);
            return false;
        }

        nlohmann::json j;
        file >> j;

        _width = j.value("width", 0);
        _height = j.value("height", 0);
        _tileWidth = j.value("tilewidth", 0);
        _tileHeight = j.value("tileheight", 0);

        auto layers = j.find("layers");
        if (layers != j.end() && layers->is_array())
        {
            for (const auto &l : *layers)
            {
                if (l.value("type", "") == "tilelayer")
                {
                    TileLayer layer;
                    layer.name = l.value("name", "");
                    layer.visible = l.value("visible", true);

                    auto data = l.find("data");
                    if (data != l.end() && data->is_array())
                    {
                        layer.data.reserve(data->size());
                        for (const auto &gid : *data)
                        {
                            layer.data.push_back(gid.get<uint32_t>());
                        }
                    }
                    _layers.push_back(std::move(layer));
                }
            }
        }

        // 충돌 레이어 캐싱 (통상 "Collision" 혹은 "collision" 명명)
        for (const auto &layer : _layers)
        {
            if (layer.name == "Collision" || layer.name == "collision")
            {
                _collisionLayer = &layer;
                break;
            }
        }

        if (!_collisionLayer)
        {
            LOG_WARN("TileMap: No 'Collision' layer found in map {}", path);
        }

        LOG_INFO(
            "TileMap: Loaded map {} ({}x{} tiles, {}x{} px/tile, {} layers)",
            path,
            _width,
            _height,
            _tileWidth,
            _tileHeight,
            _layers.size()
        );

        return true;
    } catch (const std::exception &e)
    {
        LOG_ERROR("TileMap: Exception while loading map {}: {}", path, e.what());
        return false;
    }
}

bool TileMap::IsWalkable(int tx, int ty) const
{
    // 경계 바깥은 이동 불가 (가상의 벽 처리)
    if (tx < 0 || tx >= _width || ty < 0 || ty >= _height)
    {
        return false;
    }

    if (!_collisionLayer)
    {
        return true; // 충돌 레이어가 없으면 전체 이동 가능
    }

    size_t index = static_cast<size_t>(ty * _width + tx);
    if (index >= _collisionLayer->data.size())
    {
        return false;
    }

    // Tiled의 flip bit 무시하고 GID 값만 확인
    uint32_t gid = _collisionLayer->data[index] & FLIP_MASK;
    return gid == 0; // 0은 빈 공간(이동 가능)
}

void TileMap::Slide(float &dx, float &dy, float normalX, float normalY) const
{
    // V_new = V - (V · N)N
    float dot = dx * normalX + dy * normalY;
    if (dot < 0)
    {
        dx -= dot * normalX;
        dy -= dot * normalY;
    }
}

SweepResult TileMap::SweepTest(float startX, float startY, float dx, float dy, float radius) const
{
    SweepResult result;
    result.hit = false;
    result.time = 1.0f;
    result.hitX = startX + dx;
    result.hitY = startY + dy;
    result.normalX = 0;
    result.normalY = 0;

    if (dx == 0.0f && dy == 0.0f)
        return result;

    float x0 = startX;
    float y0 = startY;
    float x1 = startX + dx;
    float y1 = startY + dy;

    // 객체의 이동 궤적을 포함하는 넉넉한 Bounding Box 영역 (Minkowski Radius 확장 포함)
    float minX = std::min(x0, x1) - radius;
    float maxX = std::max(x0, x1) + radius;
    float minY = std::min(y0, y1) - radius;
    float maxY = std::max(y0, y1) + radius;

    // 범위에 포함될 수 있는 타일 좌표 추출 범위 (안전하게 경계 확보)
    int minTx = std::max(0, GetTileX(minX));
    int maxTx = std::min(_width - 1, GetTileX(maxX));
    int minTy = std::max(0, GetTileY(minY));
    int maxTy = std::min(_height - 1, GetTileY(maxY));

    float tMin = 1.0f;
    bool collided = false;
    float nX = 0.0f, nY = 0.0f;

    // 해당 범위 내 타일들 중 Blocked 타일에 대해서만 AABB Swept 검사 수행
    for (int ty = minTy; ty <= maxTy; ++ty)
    {
        for (int tx = minTx; tx <= maxTx; ++tx)
        {
            if (!IsWalkable(tx, ty))
            {
                // 벽 타일의 AABB
                float tileLeft = static_cast<float>(tx * _tileWidth);
                float tileTop = static_cast<float>(ty * _tileHeight);
                float tileRight = tileLeft + static_cast<float>(_tileWidth);
                float tileBottom = tileTop + static_cast<float>(_tileHeight);

                // 원형 객체를 하나의 점으로써 취급하기 위해(Minkowski), 타일의 경계를 radius만큼 확장
                float eLeft = tileLeft - radius;
                float eRight = tileRight + radius;
                float eTop = tileTop - radius;
                float eBottom = tileBottom + radius;

                // Swept 검사를 위해 광선(Ray)이 확장된 타일 Bounding Box를 관통하는지 체크
                float invDx = (dx != 0.0f) ? 1.0f / dx : 0.0f;
                float invDy = (dy != 0.0f) ? 1.0f / dy : 0.0f;

                float tNearX = (eLeft - x0) * invDx;
                float tFarX = (eRight - x0) * invDx;
                if (dx == 0.0f)
                {
                    tNearX = std::numeric_limits<float>::lowest();
                    tFarX = std::numeric_limits<float>::max();
                }
                if (tNearX > tFarX)
                    std::swap(tNearX, tFarX);

                float tNearY = (eTop - y0) * invDy;
                float tFarY = (eBottom - y0) * invDy;
                if (dy == 0.0f)
                {
                    tNearY = std::numeric_limits<float>::lowest();
                    tFarY = std::numeric_limits<float>::max();
                }
                if (tNearY > tFarY)
                    std::swap(tNearY, tFarY);

                // 광선이 AABB의 범위를 빗나가는 경우 조기 종료
                if (dx == 0.0f && (x0 < eLeft || x0 > eRight))
                    continue;
                if (dy == 0.0f && (y0 < eTop || y0 > eBottom))
                    continue;

                float tEnter = std::max(tNearX, tNearY);
                float tExit = std::min(tFarX, tFarY);

                if (tEnter <= tExit && tEnter >= 0.0f && tEnter <= 1.0f)
                {
                    // 가장 먼저 충돌한 지점 기록
                    if (tEnter < tMin)
                    {
                        tMin = tEnter;
                        collided = true;

                        // 충돌 면의 노말 벡터 계산 (SweepTest의 필수 요소)
                        if (tNearX > tNearY)
                        {
                            nX = (dx > 0) ? -1.0f : 1.0f;
                            nY = 0.0f;
                        }
                        else
                        {
                            nX = 0.0f;
                            nY = (dy > 0) ? -1.0f : 1.0f;
                        }
                    }
                }
                // (Optional) 이미 진입해 있는 경우(객체가 벽과 겹쳐 태어난 경우 밀어내기 처리도 가능하나 일단 무시)
                else if (tEnter < 0.0f && tExit >= 0.0f)
                {
                    tMin = 0.0f; // 충돌 즉시 발생
                    collided = true;

                    // 이 때는 노말이 Bounding Box의 중심점으로부터 객체 중심점을 향해야 함
                    float cx = (eLeft + eRight) * 0.5f;
                    float cy = (eTop + eBottom) * 0.5f;
                    float px = x0 - cx;
                    float py = y0 - cy;
                    if (std::abs(px) > std::abs(py))
                    {
                        nX = (px > 0) ? 1.0f : -1.0f;
                        nY = 0.0f;
                    }
                    else
                    {
                        nX = 0.0f;
                        nY = (py > 0) ? 1.0f : -1.0f;
                    }
                }
            }
        }
    }

    if (collided)
    {
        result.hit = true;
        result.time = tMin;
        result.hitX = startX + dx * tMin;
        result.hitY = startY + dy * tMin;
        result.normalX = nX;
        result.normalY = nY;
    }

    return result;
}

} // namespace SimpleGame
