#include <complex>
#include <iostream>
#include "raylib.h"
#include "rlgl.h"
#include <thread>
#include <vector>
#include <string>
#include <random>
#include <atomic>

using std::to_string;
using std::cout;
using std::endl;
using std::vector;

constexpr int SCREEN_WIDTH = 1400;
constexpr int SCREEN_HEIGHT = 800;
constexpr int TILE_SIZE = 64;
constexpr int CHUNK_SIZE = 32;
constexpr int WORLD_SIZE = 16;
constexpr int TERRAIN_SIZE = WORLD_SIZE * CHUNK_SIZE * TILE_SIZE;
constexpr int ACTUAL_CHUNK_SIZE = CHUNK_SIZE * TILE_SIZE;
constexpr float CAMERA_HEIGHT = 500.0f;
constexpr float PLAYER_SPEED = 5.0f;

enum TileType { EMPTY, ORE, CONVEYOR, FACTORY };

struct Tile {
    TileType type;
    Color color;
};

struct Chunk {
    bool isInView;
    std::vector<Tile> tiles;
};

struct Player {
    Vector3 position;
    Color color;
};

Player player = {{1.0f, 1.0f, -20.0f}, RED}; // Slightly raised for depth
std::vector<std::vector<Chunk>> world(WORLD_SIZE, std::vector<Chunk>(CHUNK_SIZE, {false, std::vector<Tile>(CHUNK_SIZE, {EMPTY, LIGHTGRAY})}));
// world(WORLD_SIZE, std::vector<Tile>(WORLD_SIZE, {EMPTY, LIGHTGRAY}));
std::random_device rd;  // Obtain a random seed from the hardware
std::mt19937 gen(rd()); // Initialize Mersenne Twister engine
std::uniform_real_distribution<float> dist(0.0f, 1.0f); // Define range
std::atomic<bool> loadingComplete(false);

void sleep(const int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
template <typename T>
void print(T value, std::string name = "value") {
    std::cout << name << ": " << value << std::endl;
}
template <typename T>
void PrintVector(const T& vec, const std::string& name = "vector") {
    std::cout << name << ": (" << vec.x << ", " << vec.y;
    if (std::is_same<T, Vector3>::value || std::is_same<T, Vector4>::value) {
        std::cout << ", " << vec.z;
    }
    if (std::is_same<T, Vector4>::value) {
        std::cout << ", " << vec.w;
    }
    std::cout << ")" << std::endl;
}

float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}
float Normalize(float a, float b, float t) {
    return (t - a) / (b-a);
}
void PrintColorRGB(const Color color) {
    const std::string str = "r: " + to_string(color.r) + " g: " + to_string(color.g) + " b: " + to_string(color.b);
    print(str, "color: ");
}
float GetCameraX(float playerX, float radius, float angle) {
    return playerX + radius * std::cos(angle);
}
float GetCameraY(float playerY, float radius, float angle) {
    return playerY + radius * std::sin(angle);
}
Color ApplyNormalMap(Color baseColor, Color normalColor, Vector2 lightDir) {
    // Convert normal map values from [0,255] to [-1,1]
    Vector3 normal = {
        (normalColor.r / 255.0f) * 2.0f - 1.0f,  // X-axis
        (normalColor.g / 255.0f) * 2.0f - 1.0f,  // Y-axis
        normalColor.b / 255.0f                    // Z-axis (depth)
    };

    // Normalize the normal
    float length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length > 0.0f) {
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
    }

    // Calculate lighting intensity (dot product)
    float intensity = fmax(0.0f, normal.x * lightDir.x + normal.y * lightDir.y + normal.z * 1.0f);

    // Apply intensity to the base color
    return (Color){
        (unsigned char)(baseColor.r * intensity),
        (unsigned char)(baseColor.g * intensity),
        (unsigned char)(baseColor.b * intensity),
        baseColor.a
    };
}
float GetPerlinAverage(const Image &img) {
    int count = 0;
    float accumulation = 0;
    for (int x = 0; x < img.height; ++x) {
        for (int y = 0; y < img.width; ++y) {
            const float normalized = ColorNormalize(GetImageColor(img, x, y)).x;
            count++;
            accumulation +=normalized;
        }
    }
    return accumulation / static_cast<float>(count);
}

Camera3D camera = { 0 };
float rotationAngle = 0.0f;

float GetPlayerAimAngleDeg() {
    const float ycalc = (float)GetMouseY() - SCREEN_HEIGHT/2;
    const float xcalc = (float)GetMouseX() - SCREEN_WIDTH/2;
    const float angle = 180.0f/PI * std::atan2(ycalc,xcalc);
    return angle;
}
float GetPlayerAimAngleRad() {
    const float ycalc = (float)GetMouseY() - SCREEN_HEIGHT/2;
    const float xcalc = (float)GetMouseX() - SCREEN_WIDTH/2;
    const float angle = std::atan2(ycalc,xcalc);
    return angle;
}

void MoveCamera() {
    camera.target = player.position;
    camera.position = Vector3({
        GetCameraX(player.position.x, 200.0f, (3.0f / 2.0f) * PI),
        GetCameraY(player.position.y, 200.0f, (3.0f / 2.0f) * PI),
        player.position.z - CAMERA_HEIGHT
    });
}

float GetXMovementRate() {
    const float angle = GetPlayerAimAngleRad();
    if (angle < 0) return angle/PI + 0.5;
    return -1.0f * angle/PI + 0.5;
}
float GetYMovementRate() {
    const float angle = GetPlayerAimAngleRad();
    if (angle > PI/2.0f) return (-angle + PI)/PI;
    if (angle < -PI/2.0f) return -(angle + PI)/PI;
    return angle/PI;
}

void UpdatePlayer() {
    MoveCamera();
    if (IsKeyDown(KEY_W)) {
        player.position.y += PLAYER_SPEED;
        // player.position.x -= GetXMovementRate();
    }
    if (IsKeyDown(KEY_S)) {
        player.position.y -= PLAYER_SPEED;
    }
    if (IsKeyDown(KEY_A)) {
        player.position.x += PLAYER_SPEED;
    }
    if (IsKeyDown(KEY_D)) {
        player.position.x -= PLAYER_SPEED;
    }
    if (IsKeyDown(KEY_DOWN)) player.position.z -= PLAYER_SPEED;
    if (IsKeyDown(KEY_UP)) player.position.z += PLAYER_SPEED;
}

void DrawPlayer() {
    rlPushMatrix();
        rlTranslatef(player.position.x,player.position.y,player.position.z);
        rlRotatef(GetPlayerAimAngleDeg(), 0.0f, 0.0f, 1.0f);
        rlTranslatef(-player.position.x,-player.position.y,-player.position.z);

        DrawCube(player.position, 8.0f, 8.0f, 32.0f, player.color);
        DrawCube({player.position.x - 8.0f, player.position.y , player.position.z}, 8.0f, 8.0f, 32.0f, DARKGREEN);
        DrawCubeWires(player.position, 8.0f, 8.0f, 8.0f, BLACK);
    rlPopMatrix();
}

void HandleInput() {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Vector2 mousePos = GetMousePosition();
        // int tileX = mousePos.x / TILE_SIZE;
        // int tileY = mousePos.y / TILE_SIZE;
        //
        // if (tileX < WORLD_SIZE && tileY < WORLD_SIZE) {
        //     world[tileY][tileX].tiles = {ORE, DARKBLUE}; // Place an ore tile
        // }
    }
}

void HandleCamera() {

    // if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    //     rotationAngle += GetMouseDelta().x / 100;
    //     SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    //     // const float rate = Normalize(0, SCREEN_WIDTH, rotationAngle);
    //     // const float piInterpolation = Lerp(0, 2 * PI, rate);
    //     MoveCamera();
    // } else {
    //     SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    // }
}

void DrawDebugUI() {
    const std::string str = "Distance from camera target x: " + to_string(camera.position.x - camera.target.x);
    const std::string str2 = "Distance from camera target y: " + to_string(camera.position.y - camera.target.y);
    const float ycalc = (float)GetMouseY() - SCREEN_HEIGHT/2;
    const float xcalc = (float)GetMouseX() - SCREEN_WIDTH/2;
    const std::string str3 = "Angle of line: " + to_string(
        180.0f/PI *
        (std::atan2(
            ycalc
            ,
            xcalc)));
    const std::string str4 = "mouse X: " + to_string(xcalc);
    const std::string str5 = "mouse Y: " + to_string(ycalc);
    const std::string str6 = "player x: " + to_string(player.position.x);
    const std::string str7 = "player y: " + to_string(player.position.y);
    DrawText(str.c_str(), 10, 50, 20, BLACK);
    DrawText(str2.c_str(), 10, 100, 20, BLACK);
    DrawText(str3.c_str(), 10, 150, 20, BLACK);
    DrawText(str4.c_str(), 10, 200, 20, BLACK);
    DrawText(str5.c_str(), 10, 250, 20, BLACK);
    DrawText(str6.c_str(), 10, 300, 20, BLACK);
    DrawText(str7.c_str(), 10, 350, 20, BLACK);

    DrawLine(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, GetMouseX(), GetMouseY(), RED);
    DrawLine(SCREEN_WIDTH/2 + 1, SCREEN_HEIGHT/2 + 1, GetMouseX(), GetMouseY(), RED);

    DrawLine(SCREEN_WIDTH/2, 0, SCREEN_WIDTH/2, SCREEN_HEIGHT, BLACK);
    DrawLine(0, SCREEN_HEIGHT/2, SCREEN_WIDTH, SCREEN_HEIGHT/2, BLACK);
}

void DrawChunk(const Texture2D &tileTexture, const Chunk &chunk, int xWorld, int yWorld) {
    const int sideSize = sqrt(CHUNK_SIZE);
    for (int y = 0; y < sideSize; ++y) {
        for (int x = 0; x < sideSize; ++x) {
            //YOU NEED AN INITIAL CHUNK COORDINATE, THEN PLACE ALL TEXTURES RELATIVE TO THAT COORDINATE
            DrawTexture(tileTexture, x * TILE_SIZE, y * TILE_SIZE, chunk.tiles[y].color);
            // DrawRectangleLines(x * TILE_SIZE * xWorld, y * TILE_SIZE * yWorld, TILE_SIZE, TILE_SIZE, BLACK);
            // std::string str = "x " + to_string(xWorld) + " y " + to_string(yWorld);
            // DrawText(str.c_str(),  x * TILE_SIZE * xWorld, y * TILE_SIZE * yWorld, 20, WHITE);
        }
    }
}

void DrawWorld(Texture2D tileTexture) {
    const int sideSize = sqrt(WORLD_SIZE);
    for (int y = 0; y < sideSize; ++y) {
        for (int x = 0; x < sideSize; ++x) {
            DrawChunk(tileTexture, world[y][x], x, y);
            DrawCube({(float)x * CHUNK_SIZE, (float)y * CHUNK_SIZE, -4.0f}, 1.0f, 1.0f, 1.0f, RED);
        }
    }

    // for (int y = 0; y < WORLD_HEIGHT * TILE_SIZE; y+= TILE_SIZE) {
    //     for (int x = 0; x < WORLD_WIDTH * TILE_SIZE; x+= TILE_SIZE) {
    //         for (int z = 0; z < CHUNK_SIZE; ++z) {
    //         // DrawRectangle(x,y,TILE_SIZE, TILE_SIZE, world[y][x].color);
    //         }
    //     }
    // }
}

void DrawTerrainLayer(Texture2D noiseImage) {
    DrawTexture(noiseImage, 0, 0, WHITE);
}

void DrawChunkTextures(Texture2D texture, Chunk chunk, int x, int y) {
    for (int i = 0; i < CHUNK_SIZE; ++i) {
        for (int j = 0; j < CHUNK_SIZE; ++j) {
            DrawTexture(texture, x * TILE_SIZE * j, y * TILE_SIZE * i, WHITE);
        }
    }
}

void PaintNormalMapPixelToImage(Image &img, const Image &normalMap, const int x, const int y) {
    const Color pixel = ApplyNormalMap(GetImageColor(img, x,y),
            GetImageColor(normalMap, x >= normalMap.width ? x % normalMap.width : x, y >= normalMap.height ? y % normalMap.height : y),
            {-0.2, -0.2});
    ImageDrawPixel(&img, x, y, pixel);
}

void PaintNormalMapToImage(Image &img, const Image &normalMap, Vector2 lightDir) {
    for (int x = 0; x < img.width; ++x) {
        for (int y = 0; y < img.height; ++y) {
            const Color pixel = ApplyNormalMap(GetImageColor(img, x,y),
                GetImageColor(normalMap, x >= normalMap.width ? x % normalMap.width : x, y >= normalMap.height ? y % normalMap.height : y),
                {lightDir.x, lightDir.y});
            ImageDrawPixel(&img, x, y, pixel);
        }
    }

}

void PaintFiltersToImage(Image &img, const Image &normalMap) {
    for (int z = 0; z < ACTUAL_CHUNK_SIZE; ++z) {
        for (int w = 0; w < ACTUAL_CHUNK_SIZE; ++w) {
            const int wrapX = (z + ACTUAL_CHUNK_SIZE) % ACTUAL_CHUNK_SIZE;
            const int wrapY = (w + ACTUAL_CHUNK_SIZE) % ACTUAL_CHUNK_SIZE;

            const Color color = GetImageColor(img, wrapX, wrapY);
            const float normalized = ColorNormalize(color).x;
            // ImageDrawPixel(&img, z, w,
            //     // ColorLerp(
            //     ColorFromHSV(34, 0.6f, 0.72f));
            // ColorFromHSV(34, 0.6f, 0.52f),
            // boundaryDistance
            //     )
            // );
            if (normalized < 0.66f) {
                const float boundaryDistance = Normalize(0.24, 0.66, normalized);
                ImageDrawPixel(&img, z, w,
                ColorLerp(
                ColorFromHSV(34, 0.6f, 0.72f),
                    ColorFromHSV(16, 0.70f, 0.60f),
                boundaryDistance
                    )
                );
            }
            // else if (normalized < 0.66f) {
            //     const float boundaryDistance = Normalize(0.33, 0.66, normalized);
            //     ImageDrawPixel(&img, z, w,
            //     ColorLerp(ColorFromHSV(34, 0.6f, 0.52f),
            //     ColorFromHSV(16, 0.70f, 0.60f),
            //     boundaryDistance
            //         )
            //     );
            //
            // }
            else {
                ImageDrawPixel(&img, z, w,
                ColorFromHSV(16, 0.70f, 0.60f)
                );
            }
            PaintNormalMapPixelToImage(img, normalMap, z, w);
        }
    }
}

void DrawTerrainTextureLayer(vector<vector<Texture2D>> &chunkTextures, Image &normalMap) {
    int playerChunkX = static_cast<int>(player.position.x / ACTUAL_CHUNK_SIZE);
    int playerChunkY = static_cast<int>(player.position.y / ACTUAL_CHUNK_SIZE);

    int startX = std::max(0, playerChunkX - 1);
    int endX = std::min(WORLD_SIZE - 1, playerChunkX + 1);
    int startY = std::max(0, playerChunkY - 1);
    int endY = std::min(WORLD_SIZE - 1, playerChunkY + 1);

    for (int x = startX; x <= endX; ++x) {
        for (int y = startY; y <= endY; ++y) {
            if (chunkTextures[x][y].width == 0) {
                Image noisePart = GenImagePerlinNoise(ACTUAL_CHUNK_SIZE, ACTUAL_CHUNK_SIZE, x * ACTUAL_CHUNK_SIZE, y * ACTUAL_CHUNK_SIZE, 0.3f);
                PaintFiltersToImage(noisePart, normalMap);
                // std::thread t(LoadChunkTexture, std::ref(chunkTextures), x, y, std::cref(noisePart));
                chunkTextures[x][y] = LoadTextureFromImage(noisePart);
                // t.detach();
                UnloadImage(noisePart);
            }
            DrawTexture(chunkTextures[x][y], x * ACTUAL_CHUNK_SIZE, y * ACTUAL_CHUNK_SIZE, WHITE);
        }
    }
}

void DrawMapGrid() {
    for (int x = 0; x < CHUNK_SIZE + 1; ++x) {
            DrawLine3D({(float)x * TILE_SIZE, (float)0,-5.0f}, {(float)x * TILE_SIZE, (float)ACTUAL_CHUNK_SIZE, -5.0f}, BLACK);
            DrawLine3D({(float)x * TILE_SIZE + 1, (float)1,-5.0f}, {(float)x * TILE_SIZE + 1, (float)ACTUAL_CHUNK_SIZE + 1, -5.0f}, BLACK);
    }
    for (int y = 0; y < CHUNK_SIZE + 1; ++y) {
            DrawLine3D({(float)0, (float)y * TILE_SIZE,-5.0f}, {(float)ACTUAL_CHUNK_SIZE, (float)y * TILE_SIZE, -5.0f}, BLACK);
            DrawLine3D({(float)0, (float)y * TILE_SIZE + 1,-5.0f}, {(float)ACTUAL_CHUNK_SIZE + 1, (float)y * TILE_SIZE + 1, -5.0f}, BLACK);
    }
    const float groundHeight = 0;
    Ray mouseRay = GetScreenToWorldRay((Vector2){ (float)GetMouseX(), (float)GetMouseY() }, camera);

    float t = (groundHeight - mouseRay.position.z) / mouseRay.direction.z;
    Vector3 intersection = {
        mouseRay.position.x + t * mouseRay.direction.x,
        groundHeight,  // Ensure it's on the ground
        -mouseRay.position.y + t * -mouseRay.direction.y
    };

    int mouseTileX = (int)(intersection.x / TILE_SIZE);
    int mouseTileY = (int)(-intersection.z / TILE_SIZE); // Assuming Z is "forward"


    rlPushMatrix();
    rlTranslatef(mouseTileX * TILE_SIZE, mouseTileY * TILE_SIZE, -5.0f);
    DrawRectangle(0, 0, TILE_SIZE, TILE_SIZE, Fade(GREEN, 0.7f));
    rlPopMatrix();
}

void DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position, float width, float height, float length, Color color)
{
    float x = position.x;
    float y = position.y;
    float z = position.z;
    float texWidth = (float)texture.width;
    float texHeight = (float)texture.height;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

    // We calculate the normalized texture coordinates for the desired texture-source-rectangle
    // It means converting from (tex.width, tex.height) coordinates to [0.0f, 1.0f] equivalent
    rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // Front face
        rlNormal3f(0.0f, 0.0f, 1.0f);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z + length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z + length/2);

        // Back face
        rlNormal3f(0.0f, 0.0f, - 1.0f);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z - length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z - length/2);

        // Top face
        rlNormal3f(0.0f, 1.0f, 0.0f);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y + height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y + height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z - length/2);

        // Bottom face
        rlNormal3f(0.0f, - 1.0f, 0.0f);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y - height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y - height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z + length/2);

        // Right face
        rlNormal3f(1.0f, 0.0f, 0.0f);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z - length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z + length/2);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z + length/2);

        // Left face
        rlNormal3f( - 1.0f, 0.0f, 0.0f);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z - length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z + length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z - length/2);

    rlEnd();

    rlSetTexture(0);
}

void DrawCubeTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color)
{
    float x = position.x;
    float y = position.y;
    float z = position.z;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

    // Vertex data transformation can be defined with the commented lines,
    // but in this example we calculate the transformed vertex data directly when calling rlVertex3f()
    //rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> rotate -> translate)
        //rlTranslatef(2.0f, 0.0f, 0.0f);
        //rlRotatef(45, 0, 1, 0);
        //rlScalef(2.0f, 2.0f, 2.0f);

        rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);
            // Front Face
            rlNormal3f(0.0f, 0.0f, 1.0f);       // Normal Pointing Towards Viewer
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left Of The Texture and Quad
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left Of The Texture and Quad
            // Back Face
            rlNormal3f(0.0f, 0.0f, - 1.0f);     // Normal Pointing Away From Viewer
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Left Of The Texture and Quad
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Left Of The Texture and Quad
            // Top Face
            rlNormal3f(0.0f, 1.0f, 0.0f);       // Normal Pointing Up
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left Of The Texture and Quad
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - width/2, y + height/2, z + length/2);  // Bottom Left Of The Texture and Quad
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right Of The Texture and Quad
            // Bottom Face
            rlNormal3f(0.0f, - 1.0f, 0.0f);     // Normal Pointing Down
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x + width/2, y - height/2, z - length/2);  // Top Left Of The Texture and Quad
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Left Of The Texture and Quad
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Right Of The Texture and Quad
            // Right face
            rlNormal3f(1.0f, 0.0f, 0.0f);       // Normal Pointing Right
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Left Of The Texture and Quad
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Left Of The Texture and Quad
            // Left Face
            rlNormal3f( - 1.0f, 0.0f, 0.0f);    // Normal Pointing Left
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Left Of The Texture and Quad
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left Of The Texture and Quad
        rlEnd();
    //rlPopMatrix();

    rlSetTexture(0);
}

void runGameLoop() {
    camera.up = (Vector3){ 0.0f, 0.0f, -1.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_CUSTOM;

    vector<Image> images({
        GenImageChecked(TERRAIN_SIZE, TERRAIN_SIZE, CHUNK_SIZE * TILE_SIZE, CHUNK_SIZE * TILE_SIZE, LIGHTGRAY, SKYBLUE),
        LoadImage("sand-texture-small.png"),
        LoadImage("sand-grain.png"),
        LoadImage("terrain-normal2.png"),
        GenImageColor(TILE_SIZE, TILE_SIZE,
                ColorFromHSV(25, 0.49f, 0.44f)),
        GenImagePerlinNoise(ACTUAL_CHUNK_SIZE, ACTUAL_CHUNK_SIZE, 0, 0, 1.0f),
        LoadImage("terrain-normal2.png")
    });
    ImageResize(&images[6], TILE_SIZE, TILE_SIZE);
    PaintNormalMapToImage(images[4], images[6], {-0.9f,-0.9f});

    vector<vector<Texture2D>> chunkTextures(WORLD_SIZE, vector<Texture2D>(WORLD_SIZE));
    vector<Texture2D> textures({
        LoadTextureFromImage(images[0]),
        LoadTextureFromImage(images[3]),
        LoadTextureFromImage(images[4]),
    });


    vector<vector<Texture2D>> rockPlacementTextures(CHUNK_SIZE, vector<Texture2D>(CHUNK_SIZE));
    vector<vector<float>> chunkPerlinAverage(CHUNK_SIZE,  vector<float>(CHUNK_SIZE));
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            Image noisePart = GenImagePerlinNoise(TILE_SIZE, TILE_SIZE, x * TILE_SIZE, y * TILE_SIZE, 0.05f);
            chunkPerlinAverage[x][y] = GetPerlinAverage(noisePart);
            // rockPlacementTextures[x][y] = LoadTextureFromImage(noisePart);
            UnloadImage(noisePart);
        }
    }
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        UpdatePlayer();
        HandleInput();
        HandleCamera();

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(camera);
                DrawTerrainTextureLayer(chunkTextures, images[3]);
                DrawMapGrid();
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int y = 0; y < CHUNK_SIZE; ++y) {
                        // DrawTexture(rockPlacementTextures[x][y], x * TILE_SIZE, y * TILE_SIZE, WHITE);
                        if (chunkPerlinAverage[x][y] < 0.3f) {
                            // DrawCubeTexture(textures[2], {(float)(x+1) * TILE_SIZE - TILE_SIZE/2, (float)(y + 1) * TILE_SIZE - TILE_SIZE/2,-TILE_SIZE/2},
                            //     TILE_SIZE, TILE_SIZE, TILE_SIZE, WHITE);
                            DrawCubeWiresV({(float)(x+1) * TILE_SIZE - TILE_SIZE/2, (float)(y + 1) * TILE_SIZE - TILE_SIZE/2,-TILE_SIZE/2},
                                {TILE_SIZE, TILE_SIZE, TILE_SIZE}, BLACK);
                            DrawCubeTextureRec(textures[2], {(float)0, (float)0, TILE_SIZE, TILE_SIZE},
                                {(float)(x+1) * TILE_SIZE - TILE_SIZE/2, (float)(y + 1) * TILE_SIZE - TILE_SIZE/2,-TILE_SIZE/2},
                                TILE_SIZE, TILE_SIZE, TILE_SIZE, WHITE);
                            // DrawCube({(float)(x+1) * TILE_SIZE - TILE_SIZE/2, (float)(y + 1) * TILE_SIZE - TILE_SIZE/2,-TILE_SIZE/2}, TILE_SIZE, TILE_SIZE, TILE_SIZE, ColorFromHSV(34, 0.6f, 0.72f));
                        }
                    }
                }
                // DrawCube({TILE_SIZE/2,TILE_SIZE/2,-TILE_SIZE/2}, TILE_SIZE, TILE_SIZE, TILE_SIZE, BLUE);
                DrawPlayer();
            EndMode3D();
            // DrawDebugUI();
            DrawFPS(10, 10);
        EndDrawing();
    }
    CloseWindow();

    for (Image image: images) {
        UnloadImage(image);
    }
    for (Texture2D texture: textures) {
        UnloadTexture(texture);
    }

    for (int x = 0; x < WORLD_SIZE; ++x) {
        for (int y = 0; y < WORLD_SIZE; ++y) {
            UnloadTexture(chunkTextures[x][y]);
        }
    }

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            UnloadTexture(rockPlacementTextures[x][y]);
        }
    }

}


int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Neurovore");
    SetTargetFPS(60);
    runGameLoop();
    return 0;
}
