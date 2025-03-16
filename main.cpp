#include <iostream>
#include "raylib.h"
#include "rlgl.h"
#include <thread>

using std::to_string;
using std::cout;
using std::endl;

constexpr int SCREEN_WIDTH = 1400;
constexpr int SCREEN_HEIGHT = 800;
constexpr int TILE_SIZE = 40;
constexpr int WORLD_WIDTH = 40;
constexpr int WORLD_HEIGHT = 40;

enum TileType { EMPTY, ORE, CONVEYOR, FACTORY };

struct Tile {
    TileType type;
    Color color;
};

struct Player {
    Vector3 position;
    Color color;
};

Player player = {{1.0f, 1.0f, 1.0f}, RED}; // Slightly raised for depth

std::vector<std::vector<Tile>> world(WORLD_HEIGHT, std::vector<Tile>(WORLD_WIDTH, {EMPTY, LIGHTGRAY}));

void sleep(const int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
template <typename T>
void print(T value, std::string name = "value") {
    std::cout << name << ": " << value << std::endl;
}
float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}
float Normalize(float a, float b, float t) {
    return (t - a) / (b-a);
}
float GetCameraX(float playerX, float radius, float angle) {
    return playerX + radius * std::cos(angle);
}
float GetCameraY(float playerY, float radius, float angle) {
    return playerY + radius * std::sin(angle);
}

Camera3D camera = { 0 };
float rotationAngle = 151.0f;

void MoveCamera() {
    camera.target = player.position;
    camera.position = Vector3({GetCameraX(player.position.x, 20.0f, (3.0f/2.0f) * std::acos(-1)),  GetCameraY(player.position.y, 20.0f, (3.0f/2.0f) * std::acos(-1)), player.position.z - 50});
}

void UpdatePlayer() {
    MoveCamera();
    if (IsKeyDown(KEY_W)) {
        player.position.y += 0.5f;
    }
    if (IsKeyDown(KEY_S)) {
        player.position.y -= 0.5f;
    }
    if (IsKeyDown(KEY_A)) {
        player.position.x += 0.5f;
    }
    if (IsKeyDown(KEY_D)) {
        player.position.x -= 0.5f;
    }
    if (IsKeyDown(KEY_DOWN)) player.position.z -= 0.5f;
    if (IsKeyDown(KEY_UP)) player.position.z += 0.5f;
}


void DrawWorld() {
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            Vector2 position = { (float)x, (float)y };
            if (x % 2 == 0 && y % 2 == 0) {
                DrawCube({position.x, position.y, 10.0f}, 1.0f, 1.0f, 1.0f, SKYBLUE); // 1x1x1 cube
            }
        }
    }
    DrawCube({20.0f, 0.0f, 2.0f}, 1.0f, 1.0f, 2.0f, GREEN);
    DrawCubeWires({20.0f, 0.0f, 2.0f}, 1.0f, 1.0f, 2.0f, BLACK);

    DrawCube({0.0f, 20.0f, 2.0f}, 1.0f, 1.0f, 2.0f, YELLOW);
    DrawCubeWires({0.0f, 20.0f, 2.0f}, 1.0f, 1.0f, 2.0f, BLACK);

    DrawCube({0.0f, 0.0f, 20.0f}, 1.0f, 1.0f, 2.0f, BLUE);
    DrawCubeWires({0.0f, 0.0f, 20.0f}, 1.0f, 1.0f, 2.0f, BLACK);

    // DrawGrid(50, 1.0f);
}

void DrawPlayer() {
    const float ycalc = (float)GetMouseY() - SCREEN_HEIGHT/2;
    const float xcalc = (float)GetMouseX() - SCREEN_WIDTH/2;
    const float angle = 180.0f/std::acos(-1) * std::atan2(ycalc,xcalc);
    rlPushMatrix();
        rlTranslatef(player.position.x,player.position.y,player.position.z);
        rlRotatef(angle, 0.0f, 0.0f, 1.0f);
        rlTranslatef(-player.position.x,-player.position.y,-player.position.z);

        DrawCube(player.position, 1.0f, 1.0f, 5.0f, player.color);
        DrawCube({player.position.x - 1.0f, player.position.y , player.position.z}, 1.0f, 1.0f, 1.0f, DARKGREEN);
        DrawCubeWires(player.position, 1.0f, 1.0f, 1.0f, BLACK);
    rlPopMatrix();
}

void HandleInput() {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePos = GetMousePosition();
        int tileX = mousePos.x / TILE_SIZE;
        int tileY = mousePos.y / TILE_SIZE;

        if (tileX < WORLD_WIDTH && tileY < WORLD_HEIGHT) {
            world[tileY][tileX] = {ORE, DARKBLUE}; // Place an ore tile
        }
    }
}

void HandleCamera() {

    // if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    //     rotationAngle += GetMouseDelta().x / 100;
    //     SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    //     // const float rate = Normalize(0, SCREEN_WIDTH, rotationAngle);
    //     // const float piInterpolation = Lerp(0, 2 * std::acos(-1), rate);
    //     MoveCamera();
    // } else {
    //     SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    // }
}

void runGameLoop() {
    camera.up = (Vector3){ 0.0f, 0.0f, -1.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_CUSTOM;

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        UpdatePlayer();
        HandleInput();
        HandleCamera();
        const std::string str = "Distance from camera target x: " + to_string(camera.position.x - camera.target.x);
        const std::string str2 = "Distance from camera target y: " + to_string(camera.position.y - camera.target.y);
        const float ycalc = (float)GetMouseY() - SCREEN_HEIGHT/2;
        const float xcalc = (float)GetMouseX() - SCREEN_WIDTH/2;
        const std::string str3 = "Angle of line: " + to_string(
            180.0f/std::acos(-1) *
            (std::atan2(
                ycalc
                ,
                xcalc)));
        const std::string str4 = "mouse X: " + to_string(xcalc);
        const std::string str5 = "mouse Y: " + to_string(ycalc);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawWorld();
                DrawPlayer();

                // DrawCube(cubePosition, 2.0f, 2.0f, 2.0f, RED);
                // DrawCubeWires(cubePosition, 2.0f, 2.0f, 2.0f, MAROON);
                //
                // DrawGrid(10, 1.0f);

            EndMode3D();
            DrawText(str.c_str(), 100, 50, 20, BLACK);
            DrawText(str2.c_str(), 100, 100, 20, BLACK);
            DrawText(str3.c_str(), 100, 150, 20, BLACK);
            DrawText(str4.c_str(), 100, 200, 20, BLACK);
            DrawText(str5.c_str(), 100, 250, 20, BLACK);

            DrawLine(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, GetMouseX(), GetMouseY(), RED);
            DrawLine(SCREEN_WIDTH/2 + 1, SCREEN_HEIGHT/2 + 1, GetMouseX(), GetMouseY(), RED);
            DrawLine(0,SCREEN_HEIGHT/2, SCREEN_WIDTH, SCREEN_HEIGHT/2, BLACK);
            DrawLine(SCREEN_WIDTH/2,0, SCREEN_WIDTH/2, SCREEN_HEIGHT, BLACK);

            DrawFPS(10, 10);
        EndDrawing();
    }
    CloseWindow();
}


int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Neurovore");
    SetTargetFPS(60);
    runGameLoop();
    return 0;
}
