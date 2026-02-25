#pragma once

#include <cstdint>
#include <string>
#include <vector>


namespace SimpleGame {

struct TileLayer
{
    std::string name;
    std::vector<uint32_t> data;
    bool visible{true};
};

struct SweepResult
{
    bool hit{false};
    float time{1.0f};             // t value from 0.0 to 1.0 representing impact time
    float hitX{0.0f}, hitY{0.0f}; // Position of circle center at the time of collision
    float normalX{0.0f};          // Collision surface normal X
    float normalY{0.0f};          // Collision surface normal Y
};

class TileMap
{
public:
    TileMap() = default;
    ~TileMap() = default;

    // 객체 복사 금지 (대용량 메모리 복사 방지)
    TileMap(const TileMap &) = delete;
    TileMap &operator=(const TileMap &) = delete;
    TileMap(TileMap &&) noexcept = default;
    TileMap &operator=(TileMap &&) noexcept = default;

    // Tiled JSON 파싱 (nlohmann/json 사용)
    bool LoadFromJson(const std::string &path);

    // 월드 좌표를 격자 인덱스로 안전하게 변환 (내림 처리)
    inline int GetTileX(float worldX) const
    {
        return static_cast<int>(std::floor(worldX / _tileWidth));
    }
    inline int GetTileY(float worldY) const
    {
        return static_cast<int>(std::floor(worldY / _tileHeight));
    }

    // 타일 충돌 검사 (0: Walkable, 1: Blocked)
    bool IsWalkable(int tx, int ty) const;

    // 연속 충돌 검사 (Minkowski 확장 적용 기반 Swept AABB)
    SweepResult SweepTest(float startX, float startY, float dx, float dy, float radius) const;

    // 슬라이딩 벡터 계산 (V_new = V - (V * N)N)
    void Slide(float &dx, float &dy, float normalX, float normalY) const;

    int GetWidth() const
    {
        return _width;
    }
    int GetHeight() const
    {
        return _height;
    }
    int GetTileWidth() const
    {
        return _tileWidth;
    }
    int GetTileHeight() const
    {
        return _tileHeight;
    }

private:
    int _width{0};      // Map width in tiles
    int _height{0};     // Map height in tiles
    int _tileWidth{0};  // Tile width in pixels
    int _tileHeight{0}; // Tile height in pixels

    std::vector<TileLayer> _layers;
    const TileLayer *_collisionLayer{nullptr}; // 충돌 레이어 캐싱

    // Tiled flip bits 마스크 (gid의 하위 29비트만 사용)
    static constexpr uint32_t FLIP_MASK = 0x1FFFFFFF;
};

} // namespace SimpleGame
