#include "Game/TileMap.h"
#include <fstream>
#include <gtest/gtest.h>
#include <string>


using namespace SimpleGame;

class TileMapTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 1. 타일맵 테스트를 위한 임시 JSON 파일 생성
        const std::string jsonContent = R"({
            "width": 5,
            "height": 5,
            "tilewidth": 10,
            "tileheight": 10,
            "layers": [
                {
                    "name": "Collision",
                    "type": "tilelayer",
                    "visible": true,
                    "data": [
                        1, 1, 1, 1, 1,
                        1, 0, 0, 0, 1,
                        1, 0, 0, 0, 1,
                        1, 0, 0, 0, 1,
                        1, 1, 1, 1, 1
                    ]
                }
            ]
        })";

        std::ofstream out("test_map.json");
        out << jsonContent;
        out.close();
    }

    void TearDown() override
    {
        std::remove("test_map.json");
    }
};

TEST_F(TileMapTest, ParseAndWalkable)
{
    TileMap map;
    ASSERT_TRUE(map.LoadFromJson("test_map.json"));

    EXPECT_EQ(map.GetWidth(), 5);
    EXPECT_EQ(map.GetHeight(), 5);
    EXPECT_EQ(map.GetTileWidth(), 10);
    EXPECT_EQ(map.GetTileHeight(), 10);

    // 내부(가운데 3x3)는 빈 공간(Walkable)이어야 함
    EXPECT_TRUE(map.IsWalkable(1, 1));
    EXPECT_TRUE(map.IsWalkable(2, 2));
    EXPECT_TRUE(map.IsWalkable(3, 3));

    // 외곽은 Blocked이어야 함
    EXPECT_FALSE(map.IsWalkable(0, 0));
    EXPECT_FALSE(map.IsWalkable(4, 0));
    EXPECT_FALSE(map.IsWalkable(0, 4));
    EXPECT_FALSE(map.IsWalkable(4, 4));

    // 맵 밖의 좌표 요청 시 안전하게 false 반환해야 함
    EXPECT_FALSE(map.IsWalkable(-1, -1));
    EXPECT_FALSE(map.IsWalkable(5, 5));
}

TEST_F(TileMapTest, SweepTestAndSlide)
{
    TileMap map;
    ASSERT_TRUE(map.LoadFromJson("test_map.json"));

    // 현재 설계에 따르면 외곽 타일들은 벽(Blocked)입니다.
    // 타일 크기 10x10. 내부 사용 공간은 월드 좌표 (10,10) ~ (40,40)

    // 1. 벽으로 돌진하는 경우 테스트
    // 중앙(20, 20)에서 왼쪽 벽(0, 10 ~ 10, 20)을 향해 이동 (-10, 0)
    // 원의 반지름은 2 => Minkowski에 의해 x=10 위치가 벽이라면, 10+2=12 지점에서 막혀야 함
    float startX = 20.0f;
    float startY = 20.0f;
    float dx = -10.0f;
    float dy = 0.0f;
    float radius = 2.0f;

    auto result = map.SweepTest(startX, startY, dx, dy, radius);
    EXPECT_TRUE(result.hit);
    // (20, 20)에서 시작해 x=12까지만 갈 수 있으므로, 이동량 -10 중 -8만큼 이동 시 충돌해야 함
    // hitX = 12, time = 0.8 / normalX = 1
    EXPECT_FLOAT_EQ(result.hitX, 12.0f);
    EXPECT_FLOAT_EQ(result.normalX, 1.0f);
    EXPECT_FLOAT_EQ(result.normalY, 0.0f);

    // 슬라이딩 검사: 속도를 x=-10, y=5로 던질 경우, x축 진행 성분이 모두 막히고 y 이동만 허용
    float moveX = -10.0f;
    float moveY = 5.0f;
    map.Slide(moveX, moveY, result.normalX, result.normalY);
    EXPECT_FLOAT_EQ(moveX, 0.0f);
    EXPECT_FLOAT_EQ(moveY, 5.0f);
}
