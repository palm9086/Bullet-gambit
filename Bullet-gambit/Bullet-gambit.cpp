#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <learnopengl/camera.h>
#include <learnopengl/shader.h>
#include <learnopengl/model_animation.h> // animation-capable model helper
#include <learnopengl/animation.h>
#include <learnopengl/animator.h>
#include <unordered_map>
#include <array>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ctime>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

// --- State and UI Structures ---
enum GameState {
	STATE_MENU,
	STATE_GAME,
	STATE_CREDITS,
	STATE_SUBMENU_START,
	STATE_CHAR_SELECT
};
GameState currentGameState = STATE_MENU;

struct MenuButton {
	unsigned int textureID;
	glm::vec2 position;
	glm::vec2 size;
	GameState nextState;
	bool isGameAction = false;
	int actionCode = 0;
	glm::vec3 color;
	bool isItemSlot = false;
};

// --- Global Variables ---
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;
const float MIN_TURN_DELAY = 0.5f;
const float STATUS_IMAGE_DURATION = 0.5f;
const float TURN_SKIP_DURATION = 2.0f;
const float GAME_OVER_TRANSITION_DELAY = 5.0f;

// Textures
unsigned int menuBackgroundTex = 0;
unsigned int startButtonTex = 0, creditButtonTex = 0, quitButtonTex = 0;
unsigned int twoPlayerButtonTex = 0, botButtonTex = 0;
unsigned int foeButtonTex = 0, safeButtonTex = 0;
unsigned int player1Tex = 0, player2Tex = 0;
unsigned int player1GotTex = 0, player2GotTex = 0;
unsigned int player1WonTex = 0, player2WonTex = 0;
unsigned int clickTex = 0;
unsigned int backButtonTex = 0;
unsigned int descriptionButtonTex = 0;
unsigned int itemDescriptionPanelTex = 0;
unsigned int slot1Tex = 0, slot2Tex = 0, slot3Tex = 0, slot4Tex = 0;
unsigned int restartButtonTex =0;

// Player turn indicator textures (non-interactable UI element)
unsigned int turnP1Tex =0;
unsigned int turnP2Tex =0;

// Credit textures and timing for credits sequence
unsigned int creditTextures[3] = {0,0,0 };
float creditStartTime =0.0f;
const float CREDIT_IMAGE_DURATION =5.0f; // seconds per image
const float CREDIT_FADE_DURATION =1.0f; // crossfade duration (seconds)

// Player/NecoArc models removed to avoid loading/rendering issues.
// Palette textures remain available for UI (set to0 if not used).
unsigned int paletteTextures[4] = {0,0,0,0 };
int selectedPaletteP1 = 0; // default palette0
int selectedPaletteP2 = 0; // default palette0

// Keep model pointers declared but models are disabled elsewhere
Model* playerModelMain = nullptr;
Model* playerSitting = nullptr;
Model* playerWave = nullptr;
Model* playerDying = nullptr;
Model* playerModelP1 = nullptr;
Model* playerModelP2 = nullptr;

// Store texture pixel sizes to preserve aspect ratios
std::unordered_map<unsigned int, glm::vec2> textureSizes;

// Small optimization: single global constant for bone matrix uploads
const int MAX_BONES = 100; // increased to support models with many bones/meshes

std::vector<MenuButton> activeButtons;
unsigned int quadVAO = 0;
Shader* menuShader = nullptr;
Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float lastActionTime = 0.0f;
Model* gunModel = nullptr;
// --- UPDATED MODEL POINTERS ---
Model* itemModelRoll = nullptr;
Model* itemModelSkip = nullptr;
Model* itemModelMove = nullptr;
// Add table and chair model pointers (do not remove existing pointers)
Model* tableModel = nullptr;
Model* chairModel = nullptr;
// --- END UPDATED MODEL POINTERS ---

// === Configurable model position/scale offsets (tweak here) ===
glm::vec3 g_gunPos = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 g_playerLeftPos = glm::vec3(-1.6f, -1.0f, -0.5f);
glm::vec3 g_playerRightPos = glm::vec3(1.6f, -1.0f, -0.5f);
glm::vec3 g_playerLeftPosWin = glm::vec3(-2.2f, -1.4f, -0.5f);
glm::vec3 g_playerRightPosWin = glm::vec3(2.2f, -1.4f, -0.5f);

glm::vec3 g_tablePos = glm::vec3(0.0f, -0.9f, -0.2f);
glm::vec3 g_chairLeftPos = glm::vec3(-2.0f, 0.2f, -0.25f);
glm::vec3 g_chairRightPos = glm::vec3(2.0f, 0.2f, -0.25f);

float g_gunScale = 0.5f;
float g_playerScale = 0.25f;
float g_tableScale = 0.35f;
float g_chairScale = 0.25f;

// Item placement globals
float g_worldXScale = 10.0f; // item slots -> world X multiplier
float g_worldYScale = 5.0f;  // item slots -> world Y multiplier (new)
float g_itemXOffset = 0.0f;  // global X offset applied to all items (move left/right)
float g_itemZ = 1.5f;        // global Z position for items
float g_itemY_roll = -1.05f;
float g_itemY_skip = -1.00f;
float g_itemY_move = -0.75f;
// Vertical offset to raise all item models above their default placement
float g_itemVerticalOffset =0.80f; // increased to move items up; positive = up
// Per-slot horizontal offsets (world units)
float g_itemSlotLargeOffset =1.6f; // slot1/right and slot4/left
float g_itemSlotSmallOffset =0.5f; // slot2/right slightly and slot3/left slightly
// === End configurable globals ===

GLFWwindow* g_window = nullptr;

// Game Data
bool player1Turn = true;
bool gameOver = false;
bool chamber[6];
int currentChamber = 0;
std::string gameMessage = "Player1's turn";
float statusImageTime = 0.0f;
unsigned int currentStatusTex = 0;
bool turnIndicatorSkippedManually = false;

// NEW UI State
bool showItemDescription = false;
// Item System
enum ItemType { ITEM_NONE = 0, ITEM_ROLL = 1, ITEM_MOVE_BULLET = 2, ITEM_SKIP = 3 };
// Fixed4-slot storage so each slot maps1:1 to a UI button
std::array<ItemType,4> player1Items;
std::array<ItemType,4> player2Items;

// New selection state tracking for character select
int charSelectingPlayer = 1; //1 = player1 choosing,2 = player2 choosing
int selectedPalette = -1;

// New animation objects for player models
Animation* sitAnimation = nullptr;
Animation* danceAnimation = nullptr;
Animation* deathAnimation = nullptr;
Animator* animatorP1 = nullptr;
Animator* animatorP2 = nullptr;
int lastWinner = 0; //0 = none,1 = player1,2 = player2

// --- Function Prototypes ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void processInput(GLFWwindow* window);
void renderQuad(float texVStart = 0.0f, float texVEnd = 1.0f);
unsigned int loadTexture(const char* path, bool flip = false);
void setupMainMenu();
void setupStartSubMenu();
void setupGameButtons();
void handleGameAction(int action);
void startGameInit();
void updateHUD();
void giveRandomItem(bool forPlayer1);
void useItem(bool forPlayer1, int slot);
void getItemUVs(ItemType type, float& vStart, float& vEnd);
Model* loadAndValidateModel(const std::string& path, const std::string& name);
static Model* loadModelWithFlip(const std::string& path, const std::string& name);
int randomInt(int min, int max) {
	return min + rand() % (max - min + 1);
}

// --- New helper to load Model with vertical flip enabled for textures used by models ---
static Model* loadModelWithFlip(const std::string& path, const std::string& name)
{
	// Enable vertical flip for model textures (some model UVs expect this)
	stbi_set_flip_vertically_on_load(true);
	Model* m = loadAndValidateModel(path, name);
	// Restore default (no flip) for UI textures
	stbi_set_flip_vertically_on_load(false);
	return m;
}

// --- New Model Validation Helper ---
Model* loadAndValidateModel(const std::string& path, const std::string& name) {
	try {
		// REMOVED FileSystem::getPath(path) - now uses the path directly
		Model* model = new Model(path);
		std::cout << "SUCCESS: Loaded model '" << name << "' from " << path << std::endl;
		return model;
	}
	catch (const std::exception& e) {
		std::cerr << "FAILURE: Model '" << name << "' failed to load from " << path << std::endl;
		std::cerr << " Error: " << e.what() << std::endl;
		std::cerr << " ACTION: This model will not be rendered." << std::endl;
		return nullptr;
	}
}
// --- End New Model Validation Helper ---


// --- Helper for turn indicator status ---
bool isPlayerTurnIndicator(unsigned int texID) {
	return texID == player1Tex ||
		texID == player2Tex;
}

// ADDED: Helper for game over status images (Got or Won)
bool isGameOverStatusIndicator(unsigned int texID) {
	return texID == player1GotTex ||
		texID == player2GotTex ||
		texID == player1WonTex ||
		texID == player2WonTex;
}

// --- Item Helpers ---
std::string itemName(ItemType type)
{
	switch (type)
	{
	case ITEM_ROLL: return "Roll";
	case ITEM_MOVE_BULLET: return "Move";
	case ITEM_SKIP: return "Skip";
	default: return "Empty";
	}
}

void getItemUVs(ItemType type, float& vStart, float& vEnd)
{
	// UV ranges (Bottom to Top): Skip (0.0-0.25), Roll (0.25-0.50), Move (0.50-0.75), Empty (0.75-1.0)
	switch (type)
	{
	case ITEM_NONE:
		vStart = 0.75f;
		vEnd = 1.0f; break;
	case ITEM_MOVE_BULLET:
		vStart = 0.50f; vEnd = 0.75f; break;
	case ITEM_ROLL:
		vStart = 0.25f; vEnd = 0.50f; break;
	case ITEM_SKIP:
	default:
		vStart = 0.0f; vEnd = 0.25f; break;
	}
}



 // Callback implementations
void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
{
	// Update the OpenGL viewport when the window size changes
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* /*window*/, double xpos, double ypos)
{
	// Track mouse only while in-game to aim the gun
	if (currentGameState == STATE_GAME) {
		if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
		lastX = (float)xpos; lastY = (float)ypos;
	}
}

void processInput(GLFWwindow* window)
{
	// ESC input handling
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
	// R input handling (Restart)
	if (currentGameState == STATE_GAME && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
	{
		startGameInit();
		return;
	}

	// Item usage keybinds1,2,3,4
	if (currentGameState == STATE_GAME && ((float)glfwGetTime() - lastActionTime >= MIN_TURN_DELAY) && !gameOver)
	{
		int itemSlot = -1;
		if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) itemSlot = 0;
		else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) itemSlot = 1;
		else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) itemSlot = 2;
		else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) itemSlot = 3;

		if (itemSlot != -1)
		{
			auto& items = player1Turn ? player1Items : player2Items;
			// Check slot contains an item
			if (itemSlot >=0 && itemSlot <4 && items[itemSlot] != ITEM_NONE)
			{
				// Check for turn indicator before using item
				if (isPlayerTurnIndicator(currentStatusTex) && ((float)glfwGetTime() - statusImageTime < TURN_SKIP_DURATION)) {
					// Do not allow item use if turn indicator is up
					return;
				}
				// Also prevent item use if any other status image is active
				if (currentStatusTex !=0 && currentStatusTex != clickTex) {
					return;
				}
				useItem(player1Turn, itemSlot);
			}
		}
	}
}

// --- Game Logic Functions ---
// MODIFIED: This function is now simplified to only ensure the base turn message is set, 
// as the game loop handles the full window title with item information conditionally.
void updateHUD()
{
	glfwSetWindowTitle(g_window, ("Bullet Gambit | " + gameMessage).c_str());
}

void giveRandomItem(bool forPlayer1)
{
	// Choose array reference
	auto& items = forPlayer1 ? player1Items : player2Items;

	// Compact items to the left so slot0 is always filled first
	{
		int dst =0;
		for (int i =0; i <4; ++i) {
			if (items[i] != ITEM_NONE) {
				if (i != dst) {
					items[dst] = items[i];
					items[i] = ITEM_NONE;
				}
				dst++;
			}
		}
	}

	// Find first empty slot after compaction
	int firstEmpty = -1;
	for (int i =0; i <4; ++i) {
		if (items[i] == ITEM_NONE) { firstEmpty = i; break; }
	}
	if (firstEmpty == -1) return; // inventory full

	int itemID = randomInt(1,3);
	ItemType newItem = static_cast<ItemType>(itemID);
	items[firstEmpty] = newItem;
	std::cout << (forPlayer1 ? "Player1" : "Player2") << " got item: " << itemName(newItem) << " (slot " << (firstEmpty +1) << ")" << std::endl;
}

void useItem(bool forPlayer1, int slot)
{
	auto& items = forPlayer1 ? player1Items : player2Items;
	if (slot <0 || slot >=4) return;
	ItemType item = items[slot];
	if (item == ITEM_NONE) return;
	// consume the item in-place
	items[slot] = ITEM_NONE;

	lastActionTime = (float)glfwGetTime();
	switch (item)
	{
	case ITEM_ROLL:
		std::fill(std::begin(chamber), std::end(chamber), false);
		chamber[randomInt(0,5)] = true;
		currentChamber =0;
		std::cout << "Chamber rolled!" << std::endl;
		break;
	case ITEM_MOVE_BULLET:
		currentChamber = (currentChamber +1) %6;
		std::cout << "Bullet moved forward one chamber (now at index " << currentChamber << ")." << std::endl;
		break;
	case ITEM_SKIP:
		player1Turn = !player1Turn;
		gameMessage = player1Turn ? "Player1's turn" : "Player2's turn";
		std::cout << (forPlayer1 ? "Player1" : "Player2") << " used SKIP. Turn immediately passes to the opponent." << std::endl;
		break;
	default: break;
	}

	// Set turn indicator if SKIP item was used
	if (item == ITEM_SKIP) {
		currentStatusTex = player1Turn ? player1Tex : player2Tex;
		statusImageTime = (float)glfwGetTime();
	}

	// Update game buttons to reflect new active player
	setupGameButtons();

	updateHUD();
}

void startGameInit()
{
	std::fill(std::begin(chamber), std::end(chamber), false);
	chamber[randomInt(0, 5)] = true;
	currentChamber = 0;
	player1Turn = true;
	gameOver = false;
	lastWinner = 0;
	// reset fixed4-slot inventories
	player1Items.fill(ITEM_NONE);
	player2Items.fill(ITEM_NONE);
	lastActionTime = (float)glfwGetTime();
	turnIndicatorSkippedManually = false;
	showItemDescription = false;

	// Ensure players always start in the sitting animation (fresh game or restart).
	// Force sitAnimation on both animators, reset time, do not blend or preserve previous animations.
	if (animatorP1 && sitAnimation) {
		animatorP1->m_CurrentAnimation = sitAnimation;
		animatorP1->m_CurrentTime = 0.0f;
		animatorP1->m_CurrentAnimation2 = nullptr;
		animatorP1->m_CurrentTime2 = 0.0f;
		animatorP1->m_blendAmount = 0.0f;
	}
	if (animatorP2 && sitAnimation) {
		animatorP2->m_CurrentAnimation = sitAnimation;
		animatorP2->m_CurrentTime = 0.0f;
		animatorP2->m_CurrentAnimation2 = nullptr;
		animatorP2->m_CurrentTime2 = 0.0f;
		animatorP2->m_blendAmount = 0.0f;
	}

	giveRandomItem(true);
	giveRandomItem(false);
	gameMessage = "Player1's turn";
	currentStatusTex = player1Tex;
	statusImageTime = (float)glfwGetTime();
	updateHUD();

	// Ensure UI buttons reflect the freshly reset turn state
	setupGameButtons();
}

// MODIFIED: Rotation logic removed
void handleGameAction(int action)
{
	if (gameOver) return;

	lastActionTime = (float)glfwGetTime();
	statusImageTime = (float)glfwGetTime();
	turnIndicatorSkippedManually = false;
	bool fired = chamber[currentChamber];
	currentChamber = (currentChamber +1) %6;
    bool turnSwitched = false;

    //2. Resolve the shot
    if (action ==1) // Shoot Opponent (Foe button)
    {
        if (fired)
        {
            currentStatusTex = player1Turn ? player2GotTex : player1GotTex;
            gameMessage = player1Turn ? "P1 shot P2 - P1 Wins!" : "P2 shot P1 - P2 Wins!";
            gameOver = true;
            // Determine winner: if it was player1's turn when shot succeeded, player1 is the winner
            lastWinner = player1Turn ? 1 : 2;

            // Assign winner/loser animations: winner dances, loser dies.
            if (lastWinner == 1) {
                if (animatorP1 && danceAnimation) {
                    animatorP1->m_CurrentAnimation = danceAnimation;
                    animatorP1->m_CurrentTime = 0.0f;
                    animatorP1->m_CurrentAnimation2 = nullptr;
                    animatorP1->m_CurrentTime2 = 0.0f;
                    animatorP1->m_blendAmount = 0.0f;
                }
                if (animatorP2 && deathAnimation) {
                    animatorP2->m_CurrentAnimation = deathAnimation;
                    animatorP2->m_CurrentTime = 0.0f;
                    animatorP2->m_CurrentAnimation2 = nullptr;
                    animatorP2->m_CurrentTime2 = 0.0f;
                    animatorP2->m_blendAmount = 0.0f;
                }
            }
            else if (lastWinner == 2) {
                if (animatorP2 && danceAnimation) {
                    animatorP2->m_CurrentAnimation = danceAnimation;
                    animatorP2->m_CurrentTime = 0.0f;
                    animatorP2->m_CurrentAnimation2 = nullptr;
                    animatorP2->m_CurrentTime2 = 0.0f;
                    animatorP2->m_blendAmount = 0.0f;
                }
                if (animatorP1 && deathAnimation) {
                    animatorP1->m_CurrentAnimation = deathAnimation;
                    animatorP1->m_CurrentTime = 0.0f;
                    animatorP1->m_CurrentAnimation2 = nullptr;
                    animatorP1->m_CurrentTime2 = 0.0f;
                    animatorP1->m_blendAmount = 0.0f;
                }
            }
        }
        else
        {
            currentStatusTex = clickTex;
            player1Turn = !player1Turn;
            gameMessage = player1Turn ? "Player1's turn" : "Player2's turn";
            turnSwitched = true;
            // Update buttons to reflect the toggled turn
            setupGameButtons();
        }
    }
    else if (action ==2) // Shoot Self (Safe button)
    {
        if (fired)
        {
            currentStatusTex = player1Turn ? player1GotTex : player2GotTex;
            gameMessage = player1Turn ? "P1 shot self - P2 Wins!" : "P2 shot self - P1 Wins!";
            gameOver = true;
            // If player shot self, the opponent wins
            lastWinner = player1Turn ? 2 : 1;

            // Assign winner/loser animations: winner dances, loser dies.
            if (lastWinner == 1) {
                if (animatorP1 && danceAnimation) {
                    animatorP1->m_CurrentAnimation = danceAnimation;
                    animatorP1->m_CurrentTime = 0.0f;
                    animatorP1->m_CurrentAnimation2 = nullptr;
                    animatorP1->m_CurrentTime2 = 0.0f;
                    animatorP1->m_blendAmount = 0.0f;
                }
                if (animatorP2 && deathAnimation) {
                    animatorP2->m_CurrentAnimation = deathAnimation;
                    animatorP2->m_CurrentTime = 0.0f;
                    animatorP2->m_CurrentAnimation2 = nullptr;
                    animatorP2->m_CurrentTime2 = 0.0f;
                    animatorP2->m_blendAmount = 0.0f;
                }
            }
            else if (lastWinner == 2) {
                if (animatorP2 && danceAnimation) {
                    animatorP2->m_CurrentAnimation = danceAnimation;
                    animatorP2->m_CurrentTime = 0.0f;
                    animatorP2->m_CurrentAnimation2 = nullptr;
                    animatorP2->m_CurrentTime2 = 0.0f;
                    animatorP2->m_blendAmount = 0.0f;
                }
                if (animatorP1 && deathAnimation) {
                    animatorP1->m_CurrentAnimation = deathAnimation;
                    animatorP1->m_CurrentTime = 0.0f;
                    animatorP1->m_CurrentAnimation2 = nullptr;
                    animatorP1->m_CurrentTime2 = 0.0f;
                    animatorP1->m_blendAmount = 0.0f;
                }
            }
        }
        else
        {
            currentStatusTex = clickTex;
            giveRandomItem(player1Turn);
            player1Turn = !player1Turn;
            gameMessage = player1Turn ? "Player1's turn" : "Player2's turn";
            turnSwitched = true;
            // Update buttons to reflect the toggled turn
            setupGameButtons();
        }
    }

	updateHUD();
}

// OpenGL/Utility Functions ---
unsigned int loadTexture(const char* path, bool flip)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	int width, height, nrComponents;

	// FIX: Use the 'path' directly instead of FileSystem::getPath(path)
	std::string fullPath = std::string(path);

	stbi_set_flip_vertically_on_load(flip);
	unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &nrComponents, 0);
	stbi_set_flip_vertically_on_load(false);

	if (data)
	{
		GLenum format = (nrComponents == 1) ? GL_RED : (nrComponents == 4) ? GL_RGBA : GL_RGB;
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		textureSizes[textureID] = glm::vec2((float)width, (float)height);
		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at local path: " << fullPath << std::endl;
		unsigned char fallbackData[] = { 255,255,255,255 };
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallbackData);
		textureSizes[textureID] = glm::vec2(1.0f, 1.0f);
	}
	return textureID;
}

// Modified renderQuad to support sub-texture rendering (for the item slots)
void renderQuad(float texVStart, float texVEnd)
{
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			// Pos (x, y, z) // TexCoords (u, v)
			-1.0f,	1.0f,0.0f,0.0f,1.0f,
			-1.0f, -1.0f,0.0f,0.0f,0.0f,
			1.0f,	1.0f,0.0f,1.0f,1.0f,
			1.0f, -1.0f,0.0f,1.0f,0.0f,
		};
		unsigned int quadVBO;
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);

		// FIX: Corrected typo from quadVAAO to quadVAO
		glBindVertexArray(quadVAO);

		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}

	// Set the uniforms for the texture coordinates in the menu shader
	if (menuShader) {
		menuShader->use();
		menuShader->setFloat("vStart", texVStart);
		menuShader->setFloat("vEnd", texVEnd);
	}

	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

// --- Menu Setup Functions ---
#define BUTTON_WIDTH_NORM (300.0f / SCR_WIDTH)
#define BUTTON_HEIGHT_NORM (100.0f / SCR_HEIGHT)
#define MARGIN_X (0.05f)
#define MARGIN_Y (0.05f)
#define BUTTON_SPACING (BUTTON_HEIGHT_NORM +0.01f)
#define ITEM_SLOT_SIZE_NORM (120.0f / SCR_HEIGHT)

// Helper: return normalized size (width,height) for a texture ID using stored pixel sizes
static glm::vec2 getNormalizedSize(unsigned int texID)
{
	auto it = textureSizes.find(texID);
	if (it != textureSizes.end()) {
		float w = it->second.x / (float)SCR_WIDTH;
		float h = it->second.y / (float)SCR_HEIGHT;
		return glm::vec2(w, h);
	}
	// fallback
	return glm::vec2(BUTTON_WIDTH_NORM, BUTTON_HEIGHT_NORM);
}

void setupMainMenu() {
	activeButtons.clear();

	// Get normalized sizes for each menu texture (use actual image sizes)
	glm::vec2 quitSize = getNormalizedSize(quitButtonTex)*2.0f;
	glm::vec2 creditSize = getNormalizedSize(creditButtonTex)*2.0f;
	glm::vec2 startSize = getNormalizedSize(startButtonTex)*2.0f;

	// spacing between stacked buttons (normalized)
	float gap = 0.05f;

	// Compute total stack height (quit at bottom -> credit -> start at top)
	float totalHeight = quitSize.y + gap + creditSize.y + gap + startSize.y;

	// Base Y position to vertically center the stack (slightly above center) -- keep previous offset of0.25f
	float baseY = 0.3f - (totalHeight / 2.0f);

	// Center X per button based on its own width
	float quitX = (1.0f - quitSize.x) / 2.0f;
	float creditX = (1.0f - creditSize.x) / 2.0f;
	float startX = (1.0f - startSize.x) / 2.0f;

	// Positions
	float quitY = baseY;
	float creditY = quitY + quitSize.y + gap;
	float startY = creditY + creditSize.y + gap;

	// Push buttons using texture-based sizes
	activeButtons.push_back({ quitButtonTex, glm::vec2(quitX, quitY), quitSize, (GameState)-1, false,0, glm::vec3(0.8f,0.2f,0.2f) });
	activeButtons.push_back({ creditButtonTex, glm::vec2(creditX, creditY), creditSize, STATE_CREDITS, false,0, glm::vec3(0.2f,0.4f,0.8f) });
	activeButtons.push_back({ startButtonTex, glm::vec2(startX, startY), startSize, STATE_SUBMENU_START, false,0, glm::vec3(0.2f,0.8f,0.2f) });
}

void setupStartSubMenu() {
	// Build main menu first (start button will have correct size/position)
	setupMainMenu();

	// Find the start button we just added to position sub-buttons relative to it
	glm::vec2 startPos(0.0f);
	glm::vec2 startSize(BUTTON_WIDTH_NORM, BUTTON_HEIGHT_NORM);
	for (const auto& b : activeButtons) {
		if (b.textureID == startButtonTex) {
			startPos = b.position;
			startSize = b.size;
			break;
		}
	}

	// Get sizes for sub-buttons from their textures
	glm::vec2 twoSize = getNormalizedSize(twoPlayerButtonTex)*2.0f;
	glm::vec2 botSize = getNormalizedSize(botButtonTex) * 2.0f;

	// Place sub-buttons to the right of start button with small gap, align vertically centered to start
	float gap = 0.02f;
	float subX = startPos.x + startSize.x + gap;
	float twoY = startPos.y + (startSize.y - twoSize.y) / 2.0f;
	float botY = startPos.y + (startSize.y - botSize.y) / 2.0f;

	activeButtons.push_back({ twoPlayerButtonTex, glm::vec2(subX, twoY), twoSize, STATE_CHAR_SELECT, false,0, glm::vec3(0.2f,0.7f,0.7f) });
	activeButtons.push_back({ botButtonTex, glm::vec2(subX + twoSize.x + 0.01f, botY), botSize, STATE_SUBMENU_START, false,0, glm::vec3(0.5f,0.5f,0.5f) });
}

void setupGameButtons() {
	activeButtons.clear();

	// Local modifiable variable to prevent "expression must be a modifiable lvalue" error
	float currentMarginY =0.05f;
	float currentMarginX =0.05f;

	//1. Action Buttons (Left/Right)
	unsigned int leftTex = (player1Turn ? safeButtonTex : foeButtonTex);
	unsigned int rightTex = (player1Turn ? foeButtonTex : safeButtonTex);
	glm::vec2 leftSize = getNormalizedSize(leftTex) *2.0f;
	glm::vec2 rightSize = getNormalizedSize(rightTex) *2.0f;

	float H_SPACING =0.30f;
	float totalWidth = leftSize.x + H_SPACING + rightSize.x;
	float startX = (1.0f - totalWidth) /2.0f;
	float maxH = std::max(leftSize.y, rightSize.y);
	float btnY = (1.0f - maxH) /2.0f;

	activeButtons.push_back({ leftTex, glm::vec2(startX, btnY + (maxH - leftSize.y) /2.0f), leftSize, STATE_GAME, true, (leftTex == safeButtonTex ?2 :1), glm::vec3(1.0f) });
	activeButtons.push_back({ rightTex, glm::vec2(startX + leftSize.x + H_SPACING, btnY + (maxH - rightSize.y) /2.0f), rightSize, STATE_GAME, true, (rightTex == safeButtonTex ?2 :1), glm::vec3(1.0f) });

	// 2. Navigation Buttons
	glm::vec2 backSize = getNormalizedSize(backButtonTex) * 2.0f;
	activeButtons.push_back({ backButtonTex, glm::vec2(currentMarginX, 1.0f - currentMarginY - backSize.y), backSize, STATE_MENU, false, 3, glm::vec3(1.0f) });

	// DESCRIPTION BUTTON (Top-right) - restore visibility and action
	glm::vec2 descSize = getNormalizedSize(descriptionButtonTex) * 2.0f;
	glm::vec2 descPos = glm::vec2(1.0f - currentMarginX - descSize.x, 1.0f - currentMarginY - descSize.y);
	activeButtons.push_back({ descriptionButtonTex, descPos, descSize, STATE_GAME, false, 4, glm::vec3(1.0f) });

	// 3. Item Slots with Symmetrical Offsets
	float ITEM_SLOT_H =0.133f; // Equivalent to120.0f /900.0f
	float ITEM_SPACING = ITEM_SLOT_H *0.4f;

	// Symmetrical distance variables
	float largeMove =0.03f; // Outer slots (1 &4)
	float smallMove =0.01f; // Inner slots (2 &3)

	std::array<glm::vec2,4> slotSizes;
	float totalSlotsWidth =0.0f;
	for (int i =0; i <4; ++i) {
		slotSizes[i] = glm::vec2(ITEM_SLOT_H *1.6f, ITEM_SLOT_H);
		totalSlotsWidth += slotSizes[i].x;
	}
	totalSlotsWidth += ITEM_SPACING *3.0f;

	float cursorX = 0.5f - (totalSlotsWidth *0.5f);
	for (int i =0; i <4; ++i) {
		unsigned int slotTex = (i ==0 ? slot1Tex : (i ==1 ? slot2Tex : (i ==2 ? slot3Tex : slot4Tex)));

		// Apply symmetrical horizontal adjustments
		float adjustedX = cursorX;
		if (i ==0) adjustedX += largeMove; // Slot1: move Right
		if (i ==1) adjustedX += smallMove; // Slot2: move Right a little
		if (i ==2) adjustedX -= smallMove; // Slot3: move Left a little
		if (i ==3) adjustedX -= largeMove; // Slot4: move Left

		activeButtons.push_back({ slotTex, glm::vec2(adjustedX, currentMarginY), slotSizes[i], STATE_GAME, false,10 + i, glm::vec3(1.0f), true });
		cursorX += slotSizes[i].x + ITEM_SPACING;
	}
}


void setupCharacterSelect()
{
	activeButtons.clear();
	// New layout: two rows - top = Player1, bottom = Player2
	glm::vec2 palSize = glm::vec2(0.15f, 0.15f); // slightly smaller to fit lower on screen
	float gap = 0.02f;
	float baseX = (1.0f - (palSize.x * 4.0f + gap * 3.0f)) / 2.0f;
	// Place rows near the bottom so they align with the play/back buttons
	float bottomY = MARGIN_Y; // bottom row (Player2)
	float topY = bottomY + palSize.y + 0.02f; // top row (Player1)

	// Player1 (top row) action codes20..23
	for (int i = 0; i < 4; ++i) {
		activeButtons.push_back({ paletteTextures[i], glm::vec2(baseX + i * (palSize.x + gap), topY), palSize, STATE_CHAR_SELECT, false,20 + i, glm::vec3(1.0f,1.0f,1.0f) });
	}
	// Player2 (bottom row) action codes30..33
	for (int i = 0; i < 4; ++i) {
		activeButtons.push_back({ paletteTextures[i], glm::vec2(baseX + i * (palSize.x + gap), bottomY), palSize, STATE_CHAR_SELECT, false,30 + i, glm::vec3(1.0f,1.0f,1.0f) });
	}

	// Back button (bottom-left)
	glm::vec2 backSize = getNormalizedSize(backButtonTex) * 2.0f;
	activeButtons.push_back({ backButtonTex, glm::vec2(MARGIN_X-0.01f, MARGIN_Y), backSize, STATE_MENU, false,3, glm::vec3(1.0f,1.0f,1.0f) });

	// Next / Play button on bottom-right (reuse startButtonTex as requested), actionCode5
	glm::vec2 nextSize = getNormalizedSize(startButtonTex) * 2.0f;
	activeButtons.push_back({ startButtonTex, glm::vec2(1.0f - MARGIN_X - nextSize.x + 0.01f, MARGIN_Y), nextSize, STATE_GAME, false,5, glm::vec3(1.0f,1.0f,1.0f) });
}

// ====================================================
// === MAIN FUNCTION ===
// ====================================================
int main()
{
	srand(static_cast<unsigned>(time(0)));

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	g_window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Bullet Gambit", NULL, NULL);
	glfwMakeContextCurrent(g_window);
	glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);
	glfwSetCursorPosCallback(g_window, mouse_callback);
	glfwSetMouseButtonCallback(g_window, mouse_button_callback);
	glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Shader ourShader("Bullet-gambit.vs", "Bullet-gambit.fs");
	Shader newMenuShader("menu_2d.vs", "menu_2d.fs");
	menuShader = &newMenuShader;
	//1. Load Main Gun Model (use flipped textures)
	gunModel = loadModelWithFlip("resources/objects/gun/fullgun.dae", "Gun Model");
	//2. Load Roll Item Model (Restored to Bread)
	itemModelRoll = loadModelWithFlip("resources/objects/bread/bread.dae", "Roll Item Model (Bread)");
	//3. Load Skip Item Model (Restored to Uno/Untitled.dae, assuming it is available now)
	itemModelSkip = loadModelWithFlip("resources/objects/uno/Untitled.dae", "Skip Item Model (Uno Card)");
	//4. Load Move Item Model (Restored to Watch)
	itemModelMove = loadModelWithFlip("resources/objects/watch/Untitled.dae", "Move Item Model (Watch)");
	// Load Table and Chair models (non-destructive, after item models)
	tableModel = loadModelWithFlip("resources/objects/tableset/table.dae", "Table Model");
	chairModel = loadModelWithFlip("resources/objects/tableset/chair.dae", "Chair Model");

	// Load palette textures and player models
	// Load palettes (optional) and two instances of the NecoArc model for left/right
	paletteTextures[0] = loadTexture("resources/objects/necoarc/Neco_Arc_Palette1.png", true);
	paletteTextures[1] = loadTexture("resources/objects/necoarc/Neco_Arc_Palette2.png", true);
	paletteTextures[2] = loadTexture("resources/objects/necoarc/Neco_Arc_Palette3.png", true);
	paletteTextures[3] = loadTexture("resources/objects/necoarc/Neco_Arc_Palette4.png", true);

	playerModelP1 = loadModelWithFlip("resources/objects/necoarc/untitled.dae", "NecoArc P1");
	playerModelP2 = loadModelWithFlip("resources/objects/necoarc/untitled.dae", "NecoArc P2");

	// Load animations (use same model reference for bone mappings)
	if (playerModelP1) {
		sitAnimation = new Animation("resources/objects/necoarc/Sitting Talking.dae", playerModelP1);
		danceAnimation = new Animation("resources/objects/necoarc/Slide Hip Hop Dance.dae", playerModelP1);
		deathAnimation = new Animation("resources/objects/necoarc/Death From Front Headshot.dae", playerModelP1);
		// Create per-player animators and start with sitting animation
		animatorP1 = new Animator(sitAnimation);
		animatorP2 = new Animator(sitAnimation);
	}

	// Debug: report model load details for Neco (use P1 as representative)
	if (playerModelP1) {
		std::cout << "Neco model (P1) loaded. Mesh count: " << playerModelP1->meshes.size()
			<< ", loaded texture count: " << playerModelP1->textures_loaded.size() << std::endl;
		for (size_t ti = 0; ti < playerModelP1->textures_loaded.size(); ++ti) {
			std::cout << " Texture[" << ti << "] path: " << playerModelP1->textures_loaded[ti].path << std::endl;
		}
	}
	else {
		std::cerr << "Neco model (P1) failed to load (playerModelP1 is nullptr)." << std::endl;
	}

	// Load Textures
	menuBackgroundTex = loadTexture("resources/textures/menu/menu.png", true); // flip background
	startButtonTex = loadTexture("resources/textures/menu/Start button.png", true);
	creditButtonTex = loadTexture("resources/textures/menu/Credit button.png", true);
	quitButtonTex = loadTexture("resources/textures/menu/Quit button.png", true);
	twoPlayerButtonTex = loadTexture("resources/textures/menu/2 player button.png", true);
	botButtonTex = loadTexture("resources/textures/menu/bot button.png", true);
	safeButtonTex = loadTexture("resources/textures/menu/safe button.png", true);
	foeButtonTex = loadTexture("resources/textures/menu/foe button.png", true);

	player1Tex = loadTexture("resources/textures/menu/Player1.png", true);
	player2Tex = loadTexture("resources/textures/menu/Player2.png", true);
	player1GotTex = loadTexture("resources/textures/menu/Player1got.png", true);
	player2GotTex = loadTexture("resources/textures/menu/Player2got.png", true);
	player1WonTex = loadTexture("resources/textures/menu/Player1Won.png", true);
	player2WonTex = loadTexture("resources/textures/menu/Player2Won.png", true);
	clickTex = loadTexture("resources/textures/menu/Click.png", true);

	backButtonTex = loadTexture("resources/textures/menu/Back.png", true);
	descriptionButtonTex = loadTexture("resources/textures/menu/Description.png", true);
	itemDescriptionPanelTex = loadTexture("resources/textures/menu/item description.png", true);
	slot1Tex = loadTexture("resources/textures/menu/slot1.png", true);
	slot2Tex = loadTexture("resources/textures/menu/slot2.png", true);
	slot3Tex = loadTexture("resources/textures/menu/slot3.png", true);
	slot4Tex = loadTexture("resources/textures/menu/slot4.png", true);

	// NEW: Load Restart Button Texture
	restartButtonTex = loadTexture("resources/textures/menu/Restart.png", true);

	// Load turn indicator textures (top-center non-interactable)
	turnP1Tex = loadTexture("resources/textures/menu/p1.png", true);
	turnP2Tex = loadTexture("resources/textures/menu/p2.png", true);

	// Load credit textures (3 images)
	creditTextures[0] = loadTexture("resources/textures/credit/1.png", true);
	creditTextures[1] = loadTexture("resources/textures/credit/2.png", true);
	creditTextures[2] = loadTexture("resources/textures/credit/3.png", true);


	setupMainMenu();

	// --- Game Loop ---
	while (!glfwWindowShouldClose(g_window))
	{
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		processInput(g_window);

		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (currentGameState == STATE_MENU || currentGameState == STATE_SUBMENU_START || currentGameState == STATE_CHAR_SELECT)
		{
			// Menu/character select logic preserved; actual UI rendering moved to the dedicated UI pass below.
			// No3D rendering required here.
		}
		else if (currentGameState == STATE_CREDITS)
		{
			// Keep credits clear behavior (solid color) here; actual UI rendering for credit buttons moved to UI pass.
			glDisable(GL_DEPTH_TEST);
			glClearColor(0.0f,0.0f,0.3f,1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			// Ensure we always return to the main menu after all credit images have been shown.
			// Defensive: set creditStartTime if it wasn't set by the transition code.
			if (creditStartTime <=0.0f) creditStartTime = (float)glfwGetTime();
			float totalElapsedCredits = (float)glfwGetTime() - creditStartTime;
			const float totalCreditsDuration = CREDIT_IMAGE_DURATION *3.0f; //3 images
			if (totalElapsedCredits >= totalCreditsDuration) {
				setupMainMenu();
				currentGameState = STATE_MENU;
				creditStartTime = 0.0f;
			}
		}
		else if (currentGameState == STATE_GAME)
		{
			bool inCooldown = ((float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY);

			// --- Draw Won overlay as a background BEFORE3D rendering so player models appear in front ---
			if (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex) {
				// Draw full-screen background image behind3D models
				glDisable(GL_DEPTH_TEST);
				// Prevent the overlay from writing to the depth buffer so subsequent3D draws are not occluded
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				if (menuShader) menuShader->use();
				if (menuShader) menuShader->setFloat("alpha",0.65f);
				if (menuShader) menuShader->setVec2("offset", glm::vec2(0.0f,0.0f));
				if (menuShader) menuShader->setVec2("scale", glm::vec2(1.0f,1.0f));
				glBindTexture(GL_TEXTURE_2D, currentStatusTex);
				if (menuShader) menuShader->setVec3("color", glm::vec3(1.0f,1.0f,1.0f));
				renderQuad(0.0f,1.0f);
				// Restore depth write and depth testing for3D rendering
				glDepthMask(GL_TRUE);
				glEnable(GL_DEPTH_TEST);
			}

			// Determine if we're in the Won scene once and reuse the flag
			bool isWonScene = (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex);

			//3D Rendering (Gun Model)
			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);
			// CRITICAL CHECK: Ensure shader is valid before using it
			if (ourShader.ID ==0) {
				// Skip3D rendering if the main shader is invalid
				std::cerr << "Warning: Main shader is invalid. Skipping3D rendering."
					<< std::endl;
			}
			else {
				ourShader.use();
				// Prevent accidental palette use for non-player meshes
				ourShader.setBool("usePalette", false);
				// Ensure fragments with very low alpha are preserved for models
				ourShader.setFloat("alphaCutoff",0.0f);
				glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT,0.1f,1000.0f);
				glm::mat4 view = camera.GetViewMatrix();
				ourShader.setMat4("projection", projection);
				ourShader.setMat4("view", view);

				// Only draw the gun/model items/table/chairs when NOT in the Won scene
				if (gunModel && !isWonScene)
				{
					// compute target world point from mouse ray intersecting z=0 plane
					glm::vec3 rayOrigin = camera.Position;
					// normalized device coords
					float mx = lastX / (float)SCR_WIDTH;
					float my = lastY / (float)SCR_HEIGHT;
					float ndcX = mx *2.0f -1.0f;
					float ndcY = my *2.0f -1.0f;

					glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT,0.1f,1000.0f);
					glm::mat4 view = camera.GetViewMatrix();
					glm::mat4 invProj = glm::inverse(projection);
					glm::mat4 invView = glm::inverse(view);

					// Ray in clip space
					glm::vec4 rayClip(ndcX, ndcY, -1.0f,1.0f);
					// Eye space
					glm::vec4 rayEye = invProj * rayClip;
					rayEye.z = -1.0f; rayEye.w =0.0f;
					// World space direction
					glm::vec4 rayWorld4 = invView * rayEye;
					glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld4));

					glm::vec3 targetWorld = glm::vec3(0.0f);
					if (fabs(rayDir.z) >1e-6f) {
						float t = -rayOrigin.z / rayDir.z; // intersect with z=0 plane
						if (t >0.0f) targetWorld = rayOrigin + rayDir * t;
						else targetWorld = rayOrigin + rayDir *5.0f; // fallback
					}
					else {
						// ray parallel to plane - fallback to point far along ray
						targetWorld = rayOrigin + rayDir *5.0f;
					}

					// Build orientation so that model's local +X axis points toward targetWorld
					glm::vec3 gunPos = glm::vec3(0.0f);
					glm::vec3 dir = glm::normalize(targetWorld - gunPos);
					// avoid degenerate
					if (glm::length(dir) <1e-6f) dir = glm::vec3(1.0f,0.0f,0.0f);

					glm::vec3 up(0.0f,1.0f,0.0f);
					// MODIFIED: xAxis now equals dir as requested
					glm::vec3 xAxis = dir; // left side follows target
					glm::vec3 zAxis = glm::normalize(glm::cross(xAxis, up));
					glm::vec3 yAxis = glm::normalize(glm::cross(-zAxis, xAxis));
					// Preserve prior vertical inversion if applied

					glm::mat4 orient(1.0f);
					orient[0] = glm::vec4(xAxis,0.0f);
					orient[1] = glm::vec4(yAxis,0.0f);
					orient[2] = glm::vec4(zAxis,0.0f);
					// Apply base model rotation that was previously used (camera orientation)
					glm::mat4 baseRot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f,0.0f,0.0f));

					// Compose final model matrix: translate -> apply flip -> orient -> baseRot -> scale
					glm::mat4 model = glm::mat4(1.0f);
					model = glm::translate(model, gunPos);

					// Apply180-degree rotation around World Y axis to flip the gun horizontally BEFORE orientation
					model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f,1.0f,0.0f));

					model = model * orient * baseRot;
					model = glm::scale(model, glm::vec3(0.5f));
					ourShader.setMat4("model", model);
					gunModel->Draw(ourShader);
				}

				// Draw two NecoArc models next to the gun (left and right)
				// Ensure we update animators and upload bone matrices per-player before drawing
				if (playerModelP1) {
					// Disable face culling while drawing player models to avoid half-mesh clipping on improperly wound meshes
					GLboolean cullEnabledBeforeModels = glIsEnabled(GL_CULL_FACE);
					if (cullEnabledBeforeModels) glDisable(GL_CULL_FACE);

					// Update animator for P1
					if (animatorP1) animatorP1->UpdateAnimation(deltaTime);
					// Upload bone matrices for P1 (use animator if present)
					if (animatorP1) {
						auto mats = animatorP1->GetFinalBoneMatrices();
						for (size_t bi =0; bi < mats.size() && bi < MAX_BONES; ++bi) {
							std::string name = "finalBonesMatrices[" + std::to_string(bi) + "]";
							ourShader.setMat4(name.c_str(), mats[bi]);
						}
						// fill rest with identity
						for (size_t bi = mats.size(); bi < MAX_BONES; ++bi) {
							std::string name = "finalBonesMatrices[" + std::to_string(bi) + "]";
							ourShader.setMat4(name.c_str(), glm::mat4(1.0f));
						}
					}
					else {
						for ( int bi =0; bi < MAX_BONES; bi++) {
							std::string name = "finalBonesMatrices[" + std::to_string(bi) + "]";
							ourShader.setMat4(name.c_str(), glm::mat4(1.0f));
						}
					}

					// Determine transform offsets depending on whether we are in the Won scene
					float leftX = isWonScene ? -2.2f : -1.6f;
					float rightX = isWonScene ?2.2f :1.6f;
					float sitY = isWonScene ? -1.4f : -1.0f;

					glm::mat4 mLeft = glm::mat4(1.0f);
					// position left player to sit on left chair (normal sitting or win scene adjustments)
					mLeft = glm::translate(mLeft, glm::vec3(leftX, sitY, -0.5f));
					mLeft = glm::rotate(mLeft, glm::radians(-90.0f), glm::vec3(0.0f,1.0f,0.0f));
					// Flip model horizontally so the character faces inward toward center
					mLeft = glm::rotate(mLeft, glm::radians(180.0f), glm::vec3(0.0f,1.0f,0.0f));
					mLeft = glm::scale(mLeft, glm::vec3(g_playerScale));
					ourShader.setMat4("model", mLeft);

					// Bind palette for P1 if available
					int palIndexP1 = (selectedPaletteP1 >=0 && selectedPaletteP1 <4) ? selectedPaletteP1 :0;
					if (paletteTextures[palIndexP1] !=0) {
						ourShader.setBool("usePalette", true);
						ourShader.setBool("flipPaletteHoriz", true);
						const int PALETTE_TEX_UNIT =15;
						glActiveTexture(GL_TEXTURE0 + PALETTE_TEX_UNIT);
						glBindTexture(GL_TEXTURE_2D, paletteTextures[palIndexP1]);
						ourShader.setInt("paletteTex", PALETTE_TEX_UNIT);
					}
					// Draw full model (visible in Won scene too)
					playerModelP1->Draw(ourShader);
					// unbind
					if (paletteTextures[palIndexP1] !=0) {
						glActiveTexture(GL_TEXTURE0 +15);
						glBindTexture(GL_TEXTURE_2D,0);
						glActiveTexture(GL_TEXTURE0);
						ourShader.setBool("usePalette", false);
					}

					// Restore face culling state after drawing models
					if (cullEnabledBeforeModels) glEnable(GL_CULL_FACE);
				}

				if (playerModelP2) {
					// Disable face culling while drawing player models to avoid half-mesh clipping
					GLboolean cullEnabledBeforeModelsP2 = glIsEnabled(GL_CULL_FACE);
					if (cullEnabledBeforeModelsP2) glDisable(GL_CULL_FACE);

					if (animatorP2) animatorP2->UpdateAnimation(deltaTime);
					// Upload bone matrices for P2
					if (animatorP2) {
						auto mats = animatorP2->GetFinalBoneMatrices();
						for (size_t bi =0; bi < mats.size() && bi < MAX_BONES; ++bi) {
							std::string name = "finalBonesMatrices[" + std::to_string(bi) + "]";
							ourShader.setMat4(name.c_str(), mats[bi]);
						}
						for (size_t bi = mats.size(); bi < MAX_BONES; ++bi) {
							std::string name = "finalBonesMatrices[" + std::to_string(bi) + "]";
							ourShader.setMat4(name.c_str(), glm::mat4(1.0f));
						}
					}
					else {
						for ( int bi =0; bi < MAX_BONES; ++bi) {
							std::string name = "finalBonesMatrices[" + std::to_string(bi) + "]";
							ourShader.setMat4(name.c_str(), glm::mat4(1.0f));
						}
					}

					float leftX = isWonScene ? -2.2f : -1.6f;
					float rightX = isWonScene ?2.2f :1.6f;
					float sitY = isWonScene ? -1.4f : -1.0f;

					glm::mat4 mRight = glm::mat4(1.0f);
					// position right player to sit on right chair
					mRight = glm::translate(mRight, glm::vec3(rightX, sitY, -0.5f));
					mRight = glm::rotate(mRight, glm::radians(90.0f), glm::vec3(0.0f,1.0f,0.0f));
					// Flip model horizontally so the character faces inward toward center
					mRight = glm::rotate(mRight, glm::radians(180.0f), glm::vec3(0.0f,1.0f,0.0f));
					mRight = glm::scale(mRight, glm::vec3(g_playerScale));
					ourShader.setMat4("model", mRight);

					int palIndexP2 = (selectedPaletteP2 >=0 && selectedPaletteP2 <4) ? selectedPaletteP2 :0;
					if (paletteTextures[palIndexP2] !=0) {
						ourShader.setBool("usePalette", true);
						ourShader.setBool("flipPaletteHoriz", true);
						const int PALETTE_TEX_UNIT =15;
						glActiveTexture(GL_TEXTURE0 + PALETTE_TEX_UNIT);
						glBindTexture(GL_TEXTURE_2D, paletteTextures[palIndexP2]);
						ourShader.setInt("paletteTex", PALETTE_TEX_UNIT);
					}
					// Draw full model (visible in Won scene too)
					playerModelP2->Draw(ourShader);
					if (paletteTextures[palIndexP2] !=0) {
						glActiveTexture(GL_TEXTURE0 +15);
						glBindTexture(GL_TEXTURE_2D,0);
						glActiveTexture(GL_TEXTURE0);
						ourShader.setBool("usePalette", false);
					}

					// Restore face culling state after drawing models
					if (cullEnabledBeforeModelsP2) glEnable(GL_CULL_FACE);
				}

				// Render table and two chairs (if loaded) placed under the gun
				if (!isWonScene) {
					if (tableModel) {
						glm::mat4 modelTable = glm::mat4(1.0f);
						// Position table slightly higher so it sits closer to UI buttons
						modelTable = glm::translate(modelTable, g_tablePos);
						// Rotate -90 degrees around X axis as requested
						modelTable = glm::rotate(modelTable, glm::radians(-90.0f), glm::vec3(1.0f,0.0f,0.0f));
						// Make table smaller so it doesn't dominate the scene
						modelTable = glm::scale(modelTable, glm::vec3(g_tableScale));
						ourShader.setMat4("model", modelTable);
						tableModel->Draw(ourShader);
					}
					if (chairModel) {
						// Left chair: moved further apart (more to center), moved up slightly, and scaled
						glm::mat4 modelChairLeft = glm::mat4(1.0f);
						modelChairLeft = glm::translate(modelChairLeft, g_chairLeftPos); // further apart on X, moved up on Y, slightly forward on Z
						modelChairLeft = glm::rotate(modelChairLeft, glm::radians(-90.0f), glm::vec3(0.0f,1.0f,0.0f));
						modelChairLeft = glm::scale(modelChairLeft, glm::vec3(g_chairScale)); // keep size similar
						ourShader.setMat4("model", modelChairLeft);
						chairModel->Draw(ourShader);

						// Right chair: moved further apart, moved up slightly, and scaled
						glm::mat4 modelChairRight = glm::mat4(1.0f);
						modelChairRight = glm::translate(modelChairRight, g_chairRightPos); // further apart on X, moved up on Y, slightly forward on Z
						modelChairRight = glm::rotate(modelChairRight, glm::radians(90.0f), glm::vec3(0.0f,1.0f,0.0f));
						modelChairRight = glm::scale(modelChairRight, glm::vec3(g_chairScale)); // keep size similar
						ourShader.setMat4("model", modelChairRight);
						chairModel->Draw(ourShader);
					}
				}

				// --- ITEM MODEL RENDERING WITH CUSTOM SCALING AND TRANSLATION ---
				if (!isWonScene) {
					auto& currentItems = player1Turn ? player1Items : player2Items;

					// Place item models above each slot and align horizontally to the slot's center and vertically to the slot's top
					for (int slotIndex =0; slotIndex <4; ++slotIndex) {
						if (slotIndex >= (int)currentItems.size()) continue;
						ItemType type = currentItems[slotIndex];
						Model* modelToDraw = nullptr;
						if (type == ITEM_ROLL) modelToDraw = itemModelRoll;
						else if (type == ITEM_SKIP) modelToDraw = itemModelSkip;
						else if (type == ITEM_MOVE_BULLET) modelToDraw = itemModelMove;
						if (!modelToDraw) continue;

						// Determine UI slot top-middle (use button position/size after any adjustments/cuts)
						float uiCenterX =0.5f; // fallback
						float uiTopY = MARGIN_Y + ITEM_SLOT_SIZE_NORM; // fallback top
						for (const auto& b : activeButtons) {
							if (b.isItemSlot && b.actionCode ==10 + slotIndex) {
								uiCenterX = b.position.x + b.size.x *0.5f; // top middle X
								uiTopY = b.position.y + b.size.y; // top Y of the button
								break;
							}
						}

						// Convert UI coords to world space
						float worldX = (uiCenterX -0.5f) * g_worldXScale;
						float uiTopWorld = (uiTopY -0.5f) * g_worldYScale;

						// Apply per-slot horizontal adjustments so models sit visually over their slot areas
						float slotHorizontalOffset =0.0f;
						if (slotIndex ==0) slotHorizontalOffset = g_itemSlotLargeOffset; // Slot1 -> move right
						else if (slotIndex ==1) slotHorizontalOffset = g_itemSlotSmallOffset; // Slot2 -> move right slightly
						else if (slotIndex ==2) slotHorizontalOffset = -g_itemSlotSmallOffset; // Slot3 -> move left slightly
						else if (slotIndex ==3) slotHorizontalOffset = -g_itemSlotLargeOffset; // Slot4 -> move left

						worldX += slotHorizontalOffset;

						// Keep the same per-item scale and baseModelHalf values (do not change these)
						float scale =1.0f;
						float baseModelHalf =0.5f;
						if (type == ITEM_ROLL) { scale =0.04f; baseModelHalf =0.6f; }
						else if (type == ITEM_SKIP) { scale =0.004f; baseModelHalf =0.25f; }
						else if (type == ITEM_MOVE_BULLET) { scale =2.0f; baseModelHalf =0.15f; }

						// Compute world Y so the model's bottom sits just above the slot top (top-middle placement)
						float modelHalfWorld = baseModelHalf * scale;
						const float margin =0.02f; // small vertical gap
						float worldY = uiTopWorld + modelHalfWorld + margin;
						if (type == ITEM_MOVE_BULLET) worldY -=0.20f; // small visual tweak retained for move item

						glm::mat4 model = glm::mat4(1.0f);
						model = glm::translate(model, glm::vec3(worldX + g_itemXOffset, worldY + g_itemVerticalOffset, g_itemZ));

						// Per-model rotations preserved
						if (type == ITEM_ROLL) {
							model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f,0.0f,0.0f));
							model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0f,0.0f,1.0f));
						}
						else if (type == ITEM_SKIP) {
							model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0f,1.0f,0.0f));
						}
						else if (type == ITEM_MOVE_BULLET) {
							model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0f,1.0f,0.0f));
						}

						model = glm::scale(model, glm::vec3(scale));
						ourShader.setMat4("model", model);
						modelToDraw->Draw(ourShader);
					}
				}
				// --- END ITEM MODEL RENDERING ---
			}

			// Ensure UI uses the default texture unit (player palette binding uses unit15)
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,0);
			glDisable(GL_DEPTH_TEST);
			if (menuShader) menuShader->use();
			float elapsed = (float)glfwGetTime() - statusImageTime;
			float alpha = 1.0f;

			// If the Won overlay is active, render it semi-transparently so3D models remain visible
			if (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex) {
				alpha = 0.65f; // translucent overlay
			}

			// Determine if action buttons should be shown.
			bool showActionUI = true;
			// Hide UI if it's currently showing a "Got" or "Won" image.
			showActionUI = currentStatusTex != player1GotTex && currentStatusTex != player2GotTex && currentStatusTex != player1WonTex && currentStatusTex != player2WonTex;

			// Special case: If turn indicator or click feedback is on, hide the action/utility buttons
			if (isPlayerTurnIndicator(currentStatusTex) || currentStatusTex == clickTex) {
				showActionUI = false;
			}


			//1. Handle Game Over Transition: Shot -> Winner
			if (gameOver) {
				// The explicit5.0s delay is bypassed by the click logic below
				if (elapsed > GAME_OVER_TRANSITION_DELAY && (currentStatusTex == player1GotTex || currentStatusTex == player2GotTex)) {
					currentStatusTex = (currentStatusTex == player1GotTex) ? player2WonTex : player1WonTex;
				}
			}
			//2. Handle Non-Game-Over Status Effects
			else if (currentStatusTex != 0) {
				// Player Turn Indicator: Hide on click OR after2.0s
				if (isPlayerTurnIndicator(currentStatusTex)) {
					if (elapsed > TURN_SKIP_DURATION) {
						currentStatusTex = 0;
						showActionUI = true; // Show actions when indicator disappears
					}
				}
				// Click feedback: Hide after duration (0.5s)
				else if (currentStatusTex == clickTex) {
					if (elapsed > STATUS_IMAGE_DURATION) {
						currentStatusTex = 0;
						currentStatusTex = player1Turn ? player1Tex : player2Tex;
						statusImageTime = (float)glfwGetTime();
						showActionUI = false;
					}
					else {
						showActionUI = false;
					}
				}
			}

			//3. Manual Skip Handling
			if (currentStatusTex == 0 && !gameOver) {
				if (turnIndicatorSkippedManually) {
					turnIndicatorSkippedManually = false;
					showActionUI = true;
				}
			}

			// === UPDATED: Real-time Window Title Logic ===
			std::string baseTitle = "Bullet Gambit | " + gameMessage;
			if (!gameOver) {
				if (showActionUI || showItemDescription) { // Show item list if action is possible or description is open
					// Append item list and action guide when player can act
					std::string itemStatus = player1Turn ? "P1 Items: " : "P2 Items: ";
					auto& items = player1Turn ? player1Items : player2Items;
					for (int i = 0; i < 4; ++i) {
						if (items[i] != ITEM_NONE)
						 itemStatus += "[" + std::to_string(i + 1) + ":" + itemName(items[i]) + "] ";
						else
						 itemStatus += "[" + std::to_string(i + 1) + ":Empty] ";
					}
					baseTitle += " | " + itemStatus;
					baseTitle += "| Action: Click Safe/Foe Buttons |1-4: Use Item | R: Restart";
				}
				else {
					// Show minimal message when status image is covering
					baseTitle += " | Waiting for action (Click or wait to continue)...";
				}
			}
			glfwSetWindowTitle(g_window, baseTitle.c_str());
			// === END UPDATED: Window Title Logic ===

			// Draw Status Image
			if (currentStatusTex !=0 && !(currentStatusTex == player1WonTex || currentStatusTex == player2WonTex)) {
				if ( currentStatusTex == clickTex && elapsed >0.0f) {
					alpha =1.0f - std::min(1.0f, elapsed / STATUS_IMAGE_DURATION);
				}
				if (menuShader) menuShader->setVec2("offset", glm::vec2(0.0f,0.0f));
				if (menuShader) menuShader->setVec2("scale", glm::vec2(1.0f,1.0f));
				glBindTexture(GL_TEXTURE_2D, currentStatusTex);
				if (menuShader) menuShader->setVec3("color", glm::vec3(1.0f,1.0f,1.0f));
				if (menuShader) menuShader->setFloat("alpha", alpha);
				renderQuad(0.0f,1.0f);
			}

			// Draw all2D UI elements
			if (menuShader) menuShader->setFloat("alpha",1.0f);

			// Non-interactable player turn indicator (top-middle between Back and Description)
			// Visibility: match the Description button visibility (use the same logic as the Description button)
			{
				bool descriptionVisible = false;
				// determine local showActionUI as used in this UI pass
				bool localShowActionUI = currentStatusTex != player1GotTex && currentStatusTex != player2GotTex && currentStatusTex != player1WonTex && currentStatusTex != player2WonTex;
				if (isPlayerTurnIndicator(currentStatusTex) || currentStatusTex == clickTex) localShowActionUI = false;
				for (const auto &b : activeButtons) {
					if (b.actionCode ==4) {
						bool descShow = localShowActionUI;
						if (!gameOver && !isPlayerTurnIndicator(currentStatusTex) && currentStatusTex != clickTex) descShow = true;
						descriptionVisible = descShow;
						break;
					}
				}
				if (menuShader && descriptionVisible) {
					glm::vec2 backPos(0.0f); glm::vec2 backSize(0.0f);
					glm::vec2 descPos(1.0f); glm::vec2 descSize(0.0f);
					for (const auto &b : activeButtons) {
						if (b.actionCode ==3) { backPos = b.position; backSize = b.size; }
						if (b.actionCode ==4) { descPos = b.position; descSize = b.size; }
					}
					unsigned int turnTex = player1Turn ? turnP1Tex : turnP2Tex;
					if (turnTex !=0) {
						glm::vec2 turnSize = descSize;
						if (turnSize.x <=0.0f || turnSize.y <=0.0f) turnSize = getNormalizedSize(turnTex);
						float leftEdge = backPos.x + backSize.x;
						float rightEdge = descPos.x;
						float centerX = (leftEdge + rightEdge) *0.5f;
						float posX = centerX - turnSize.x *0.5f;
						float posY =1.0f - MARGIN_Y - turnSize.y;
						menuShader->setVec2("offset", glm::vec2(posX, posY));
						menuShader->setVec2("scale", turnSize);
						glBindTexture(GL_TEXTURE_2D, turnTex);
						menuShader->setVec3("color", glm::vec3(1.0f));
						renderQuad(0.0f,1.0f);
					}
				}
			}

			for (const auto& button : activeButtons) {
				bool showButton = false;
				glm::vec3 finalColor = button.color;

				// Determine visibility based on game state
				bool isWonScene = (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex);

				// Show the button if it's part of the action phase AND showActionUI is true
				if (button.isGameAction || button.actionCode == 3 || button.actionCode == 4) { // Action (Safe/Foe), Back, Description
					showButton = showActionUI;

					// NEW: Back button exception for Game Over 'Won' state (actionCode3)
					if (button.actionCode == 3 && isWonScene) {
						showButton = true;
					}

					// Special case for 'Description' button: always show it if not game over and not in a turn indicator state
					if (button.actionCode == 4 && !gameOver && !isPlayerTurnIndicator(currentStatusTex) && currentStatusTex != clickTex) {
						showButton = true;
					}
				}
				else if (button.isItemSlot) {
					// MODIFIED: Item slots are now hidden when Safe/Foe are hidden (controlled by showActionUI)
					showButton = showActionUI && currentGameState == STATE_GAME;

					// Dim item slots if in cooldown OR description is shown
					if (showButton && (((float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY) || showItemDescription)) {
						finalColor = finalColor * 0.5f + glm::vec3(0.1f, 0.1f, 0.1f);
					}
				}
				// Other buttons (Menu/Submenu)
				else if (currentGameState == STATE_MENU || currentGameState == STATE_SUBMENU_START) {
					showButton = true;
				}
				else if (currentGameState == STATE_CREDITS) {
					// Only the back button is shown in credits, which is handled by actionCode3
					if (button.actionCode == 3) showButton = true;
				}

				if (!showButton) continue;

				if (menuShader) menuShader->setVec2("offset", button.position);
				if (menuShader) menuShader->setVec2("scale", button.size);
				glBindTexture(GL_TEXTURE_2D, button.textureID);
				if (menuShader) menuShader->setVec3("color", finalColor);

				// Handle Item Slot Textures and Cooldowns
				if (button.isItemSlot && currentGameState == STATE_GAME) {
					int slotIndex = button.actionCode - 10;
					auto& items = player1Turn ? player1Items : player2Items;
					ItemType itemType = ITEM_NONE;
					if (slotIndex >= 0 && slotIndex < (int)items.size()) {
						itemType = items[slotIndex];
					}
					else {
						// Empty slots are dimmed
						finalColor = finalColor * 0.5f + glm::vec3(0.3f, 0.3f, 0.3f) * 0.5f;
					}
					if (menuShader) menuShader->setVec3("color", finalColor);
					float vStart, vEnd;
					getItemUVs(itemType, vStart, vEnd);
					renderQuad(vStart, vEnd);
				}
				else {
					// Render normal buttons (Safe, Foe, Back, Description, Menu buttons)
					renderQuad(0.0f, 1.0f);
				}
			}

			// --- MODIFIED: Draw Restart Button in Won Scene (Position lowered, Size smaller) ---
			if (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex) {
				// Define button properties for rendering
				MenuButton restartBtn;
				restartBtn.textureID = restartButtonTex;
				// Size reduced to1.0f
				restartBtn.size = glm::vec2(BUTTON_WIDTH_NORM * 1.0f, BUTTON_HEIGHT_NORM * 1.0f);
				// Position is MARGIN_Y, which is the bottom edge
				restartBtn.position = glm::vec2((1.0f - restartBtn.size.x) / 2.0f, MARGIN_Y);
				restartBtn.color = glm::vec3(1.0f, 1.0f, 1.0f);

				// Render the restart button
				if (menuShader) menuShader->setVec2("offset", restartBtn.position);
				if (menuShader) menuShader->setVec2("scale", restartBtn.size);
				glBindTexture(GL_TEXTURE_2D, restartButtonTex);
				if (menuShader) menuShader->setVec3("color", restartBtn.color);
				renderQuad(0.0f, 1.0f);
			}

			// Draw Item Description Panel (if active)
			if (showItemDescription) {
				// Convert600x640 pixels to normalized screen space
				float panelW_Norm = 600.0f / SCR_WIDTH;
				float panelH_Norm = 640.0f / SCR_HEIGHT;
				// Center the panel
				float panelX = (1.0f - panelW_Norm) / 2.0f;
				float panelY = (1.0f - panelH_Norm) / 2.0f;
				if (menuShader) menuShader->setVec2("offset", glm::vec2(panelX, panelY));
				if (menuShader) menuShader->setVec2("scale", glm::vec2(panelW_Norm, panelH_Norm));
				glBindTexture(GL_TEXTURE_2D, itemDescriptionPanelTex);
				if (menuShader) menuShader->setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
				renderQuad(0.0f, 1.0f);
			}
		}

		// ===============================
		// UI PASS (DEPTH OFF, SAFE)
		// All UI rendering (menu, HUD, buttons, status overlays) must happen once here
		// ===============================
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Menu / Character Select rendering
		if (currentGameState == STATE_MENU || currentGameState == STATE_SUBMENU_START || currentGameState == STATE_CHAR_SELECT)
		{
			if (menuShader) menuShader->use();
			if (menuShader) menuShader->setFloat("alpha",1.0f);
			if (menuShader) menuShader->setFloat("vStart",0.0f);
			if (menuShader) menuShader->setFloat("vEnd",1.0f);

			glBindTexture(GL_TEXTURE_2D, menuBackgroundTex);
			if (menuShader) menuShader->setVec2("offset", glm::vec2(0.0f,0.0f));
			if (menuShader) menuShader->setVec2("scale", glm::vec2(1.0f,1.0f));
			if (menuShader) menuShader->setVec3("color", glm::vec3(0.0f,0.0f,0.1f));
			renderQuad(0.0f,1.0f);

			for (const auto& button : activeButtons)
			{
				if (menuShader) menuShader->setVec2("offset", button.position);
				if (menuShader) menuShader->setVec2("scale", button.size);
				glBindTexture(GL_TEXTURE_2D, button.textureID);
				if (currentGameState == STATE_CHAR_SELECT) {
					std::string selTitle = "Character Select | P1:" + std::string((selectedPaletteP1 == -1) ? "None" : std::to_string(selectedPaletteP1))
					 + " P2:" + std::string((selectedPaletteP2 == -1) ? "None" : std::to_string(selectedPaletteP2));
					glfwSetWindowTitle(g_window, selTitle.c_str());

					if (button.actionCode >=20 && button.actionCode <24) {
						int idx = button.actionCode -20;
						glm::vec3 color = button.color;
						if (selectedPaletteP1 == idx) color = glm::vec3(0.4f,1.0f,0.4f);
						if (selectedPaletteP2 == idx) color = glm::vec3(0.4f,0.4f,1.0f);
						if (menuShader) menuShader->setVec3("color", color);
						renderQuad(0.0f,1.0f);
						continue;
					}
					if (button.actionCode >=30 && button.actionCode <34) {
						int idx = button.actionCode -30;
						glm::vec3 color = button.color;
						if (selectedPaletteP2 == idx) color = glm::vec3(0.4f,0.4f,1.0f);
						if (selectedPaletteP1 == idx) color = glm::vec3(0.4f,1.0f,0.4f);
						if (menuShader) menuShader->setVec3("color", color);
						renderQuad(0.0f,1.0f);
						continue;
					}
				}
				if (menuShader) menuShader->setVec3("color", button.color);
				renderQuad(0.0f,1.0f);
			}
		}
		else if (currentGameState == STATE_CREDITS)
		{
			// Render credits: fullscreen crossfade between three images
			if (menuShader) menuShader->use();

			float totalElapsed = (float)glfwGetTime() - creditStartTime;
			const float totalDuration = CREDIT_IMAGE_DURATION *3.0f; //3 images
			if (totalElapsed >= totalDuration) {
				// finished credits, go back to menu
				setupMainMenu();
				currentGameState = STATE_MENU;
			}
			else {
				int idx = (int)(totalElapsed / CREDIT_IMAGE_DURATION);
				if (idx <0) idx =0; if (idx >2) idx =2;
				float inSegment = totalElapsed - idx * CREDIT_IMAGE_DURATION; // time into current segment
				float alphaCurr =1.0f;
				float alphaNext =0.0f;
				if (inSegment > (CREDIT_IMAGE_DURATION - CREDIT_FADE_DURATION)) {
					float t = (inSegment - (CREDIT_IMAGE_DURATION - CREDIT_FADE_DURATION)) / CREDIT_FADE_DURATION;
					alphaCurr =1.0f - t;
					alphaNext = t;
				}

				// Draw current image
				if (creditTextures[idx] !=0) {
					if (menuShader) menuShader->setFloat("alpha", alphaCurr);
					if (menuShader) menuShader->setVec2("offset", glm::vec2(0.0f,0.0f));
					if (menuShader) menuShader->setVec2("scale", glm::vec2(1.0f,1.0f));
					glBindTexture(GL_TEXTURE_2D, creditTextures[idx]);
					if (menuShader) menuShader->setVec3("color", glm::vec3(1.0f));
					renderQuad(0.0f,1.0f);
				}

				// Draw next image fading in - only if there is a next image (don't wrap from last to first)
				int nextIdx = idx +1;
				if (nextIdx <3 && alphaNext >0.0f && creditTextures[nextIdx] !=0) {
					if (menuShader) menuShader->setFloat("alpha", alphaNext);
					if (menuShader) menuShader->setVec2("offset", glm::vec2(0.0f,0.0f));
					if (menuShader) menuShader->setVec2("scale", glm::vec2(1.0f,1.0f));
					glBindTexture(GL_TEXTURE_2D, creditTextures[nextIdx]);
					if (menuShader) menuShader->setVec3("color", glm::vec3(1.0f));
					renderQuad(0.0f,1.0f);
				}

				if (menuShader) menuShader->setFloat("alpha",1.0f);
			}
		}
		else if (currentGameState == STATE_GAME)
		{
			// Game UI rendering (status overlays, buttons, item panels)
			// Ensure UI uses the default texture unit (player palette binding uses unit15)
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_DEPTH_TEST);
			if (menuShader) menuShader->use();
			float elapsed = (float)glfwGetTime() - statusImageTime;
			float alpha = 1.0f;

			if (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex) {
				alpha = 0.65f;
			}

			// Determine if action buttons should be shown.
			bool showActionUI = true;
			// Hide UI if it's currently showing a "Got" or "Won" image.
			showActionUI = currentStatusTex != player1GotTex && currentStatusTex != player2GotTex && currentStatusTex != player1WonTex && currentStatusTex != player2WonTex;

			// Special case: If turn indicator or click feedback is on, hide the action/utility buttons
			if (isPlayerTurnIndicator(currentStatusTex) || currentStatusTex == clickTex) {
				showActionUI = false;
			}


			//1. Handle Game Over Transition: Shot -> Winner
			if (gameOver) {
				// The explicit5.0s delay is bypassed by the click logic below
				if (elapsed > GAME_OVER_TRANSITION_DELAY && (currentStatusTex == player1GotTex || currentStatusTex == player2GotTex)) {
					currentStatusTex = (currentStatusTex == player1GotTex) ? player2WonTex : player1WonTex;
				}
			}
			//2. Handle Non-Game-Over Status Effects
			else if (currentStatusTex != 0) {
				// Player Turn Indicator: Hide on click OR after2.0s
				if (isPlayerTurnIndicator(currentStatusTex)) {
					if (elapsed > TURN_SKIP_DURATION) {
						currentStatusTex = 0;
						showActionUI = true; // Show actions when indicator disappears
					}
				}
				// Click feedback: Hide after duration (0.5s)
				else if (currentStatusTex == clickTex) {
					if (elapsed > STATUS_IMAGE_DURATION) {
						currentStatusTex = 0;
						currentStatusTex = player1Turn ? player1Tex : player2Tex;
						statusImageTime = (float)glfwGetTime();
						showActionUI = false;
					}
					else {
						showActionUI = false;
					}
				}
			}

			//3. Manual Skip Handling
			if (currentStatusTex == 0 && !gameOver) {
				if (turnIndicatorSkippedManually) {
					turnIndicatorSkippedManually = false;
					showActionUI = true;
				}
			}

			// === UPDATED: Real-time Window Title Logic ===
			std::string baseTitle = "Bullet Gambit | " + gameMessage;
			if (!gameOver) {
				if (showActionUI || showItemDescription) { // Show item list if action is possible or description is open
					// Append item list and action guide when player can act
					std::string itemStatus = player1Turn ? "P1 Items: " : "P2 Items: ";
					auto& items = player1Turn ? player1Items : player2Items;
					for (int i = 0; i < 4; ++i) {
						if (items[i] != ITEM_NONE)
						 itemStatus += "[" + std::to_string(i + 1) + ":" + itemName(items[i]) + "] ";
						else
						 itemStatus += "[" + std::to_string(i + 1) + ":Empty] ";
					}
					baseTitle += " | " + itemStatus;
					baseTitle += "| Action: Click Safe/Foe Buttons |1-4: Use Item | R: Restart";
				}
				else {
					// Show minimal message when status image is covering
					baseTitle += " | Waiting for action (Click or wait to continue)...";
				}
			}
			glfwSetWindowTitle(g_window, baseTitle.c_str());
			// === END UPDATED: Window Title Logic ===

			// Draw Status Image
			if (currentStatusTex !=0 && !(currentStatusTex == player1WonTex || currentStatusTex == player2WonTex)) {
				if ( currentStatusTex == clickTex && elapsed >0.0f) {
					alpha =1.0f - std::min(1.0f, elapsed / STATUS_IMAGE_DURATION);
				}
				if (menuShader) menuShader->setVec2("offset", glm::vec2(0.0f,0.0f));
				if (menuShader) menuShader->setVec2("scale", glm::vec2(1.0f,1.0f));
				glBindTexture(GL_TEXTURE_2D, currentStatusTex);
				if (menuShader) menuShader->setVec3("color", glm::vec3(1.0f,1.0f,1.0f));
				if (menuShader) menuShader->setFloat("alpha", alpha);
				renderQuad(0.0f,1.0f);
			}

			// Draw all2D UI elements
			if (menuShader) menuShader->setFloat("alpha",1.0f);

			// Non-interactable player turn indicator (top-middle between Back and Description)
			// Visibility: match the Description button visibility (use the same logic as the Description button)
			{
				bool descriptionVisible = false;
				// determine local showActionUI as used in this UI pass
				bool localShowActionUI = currentStatusTex != player1GotTex && currentStatusTex != player2GotTex && currentStatusTex != player1WonTex && currentStatusTex != player2WonTex;
				if (isPlayerTurnIndicator(currentStatusTex) || currentStatusTex == clickTex) localShowActionUI = false;
				for (const auto &b : activeButtons) {
					if (b.actionCode ==4) {
						bool descShow = localShowActionUI;
						if (!gameOver && !isPlayerTurnIndicator(currentStatusTex) && currentStatusTex != clickTex) descShow = true;
						descriptionVisible = descShow;
						break;
					}
				}
				if (menuShader && descriptionVisible) {
					glm::vec2 backPos(0.0f); glm::vec2 backSize(0.0f);
					glm::vec2 descPos(1.0f); glm::vec2 descSize(0.0f);
					for (const auto &b : activeButtons) {
						if (b.actionCode ==3) { backPos = b.position; backSize = b.size; }
						if (b.actionCode ==4) { descPos = b.position; descSize = b.size; }
					}
					unsigned int turnTex = player1Turn ? turnP1Tex : turnP2Tex;
					if (turnTex !=0) {
						glm::vec2 turnSize = descSize;
						if (turnSize.x <=0.0f || turnSize.y <=0.0f) turnSize = getNormalizedSize(turnTex);
						float leftEdge = backPos.x + backSize.x;
						float rightEdge = descPos.x;
						float centerX = (leftEdge + rightEdge) *0.5f;
						float posX = centerX - turnSize.x *0.5f;
						float posY =1.0f - MARGIN_Y - turnSize.y;
						menuShader->setVec2("offset", glm::vec2(posX, posY));
						menuShader->setVec2("scale", turnSize);
						glBindTexture(GL_TEXTURE_2D, turnTex);
						menuShader->setVec3("color", glm::vec3(1.0f));
						renderQuad(0.0f,1.0f);
					}
				}
			}

			for (const auto& button : activeButtons) {
				bool showButton = false;
				glm::vec3 finalColor = button.color;

				// Determine visibility based on game state
				bool isWonScene = (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex);

				// Show the button if it's part of the action phase AND showActionUI is true
				if (button.isGameAction || button.actionCode == 3 || button.actionCode == 4) { // Action (Safe/Foe), Back, Description
					showButton = showActionUI;

					// NEW: Back button exception for Game Over 'Won' state (actionCode3)
					if (button.actionCode == 3 && isWonScene) {
						showButton = true;
					}

					// Special case for 'Description' button: always show it if not game over and not in a turn indicator state
					if (button.actionCode == 4 && !gameOver && !isPlayerTurnIndicator(currentStatusTex) && currentStatusTex != clickTex) {
						showButton = true;
					}
				}
				else if (button.isItemSlot) {
					// MODIFIED: Item slots are now hidden when Safe/Foe are hidden (controlled by showActionUI)
					showButton = showActionUI && currentGameState == STATE_GAME;

					// Dim item slots if in cooldown OR description is shown
					if (showButton && (((float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY) || showItemDescription)) {
						finalColor = finalColor * 0.5f + glm::vec3(0.1f, 0.1f, 0.1f);
					}
				}
				// Other buttons (Menu/Submenu)
				else if (currentGameState == STATE_MENU || currentGameState == STATE_SUBMENU_START) {
					showButton = true;
				}
				else if (currentGameState == STATE_CREDITS) {
					// Only the back button is shown in credits, which is handled by actionCode3
					if (button.actionCode == 3) showButton = true;
				}

				if (!showButton) continue;

				if (menuShader) menuShader->setVec2("offset", button.position);
				if (menuShader) menuShader->setVec2("scale", button.size);
				glBindTexture(GL_TEXTURE_2D, button.textureID);
				if (menuShader) menuShader->setVec3("color", finalColor);

				// Handle Item Slot Textures and Cooldowns
				if (button.isItemSlot && currentGameState == STATE_GAME) {
					int slotIndex = button.actionCode - 10;
					auto& items = player1Turn ? player1Items : player2Items;
					ItemType itemType = ITEM_NONE;
					if (slotIndex >= 0 && slotIndex < (int)items.size()) {
						itemType = items[slotIndex];
					}
					else {
						// Empty slots are dimmed
						finalColor = finalColor * 0.5f + glm::vec3(0.3f, 0.3f, 0.3f) * 0.5f;
					}
					if (menuShader) menuShader->setVec3("color", finalColor);
					float vStart, vEnd;
					getItemUVs(itemType, vStart, vEnd);
					renderQuad(vStart, vEnd);
				}
				else {
					// Render normal buttons (Safe, Foe, Back, Description, Menu buttons)
					renderQuad(0.0f, 1.0f);
				}
			}

			// --- MODIFIED: Draw Restart Button in Won Scene (Position lowered, Size smaller) ---
			if (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex) {
				// Define button properties for rendering
				MenuButton restartBtn;
				restartBtn.textureID = restartButtonTex;
				// Size reduced to1.0f
				restartBtn.size = glm::vec2(BUTTON_WIDTH_NORM * 1.0f, BUTTON_HEIGHT_NORM * 1.0f);
				// Position is MARGIN_Y, which is the bottom edge
				restartBtn.position = glm::vec2((1.0f - restartBtn.size.x) / 2.0f, MARGIN_Y);
				restartBtn.color = glm::vec3(1.0f, 1.0f, 1.0f);

				// Render the restart button
				if (menuShader) menuShader->setVec2("offset", restartBtn.position);
				if (menuShader) menuShader->setVec2("scale", restartBtn.size);
				glBindTexture(GL_TEXTURE_2D, restartButtonTex);
				if (menuShader) menuShader->setVec3("color", restartBtn.color);
				renderQuad(0.0f, 1.0f);
			}

			// Draw Item Description Panel (if active)
			if (showItemDescription) {
				// Convert600x640 pixels to normalized screen space
				float panelW_Norm = 600.0f / SCR_WIDTH;
				float panelH_Norm = 640.0f / SCR_HEIGHT;
				// Center the panel
				float panelX = (1.0f - panelW_Norm) / 2.0f;
				float panelY = (1.0f - panelH_Norm) / 2.0f;
				if (menuShader) menuShader->setVec2("offset", glm::vec2(panelX, panelY));
				if (menuShader) menuShader->setVec2("scale", glm::vec2(panelW_Norm, panelH_Norm));
				glBindTexture(GL_TEXTURE_2D, itemDescriptionPanelTex);
				if (menuShader) menuShader->setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
				renderQuad(0.0f, 1.0f);
			}
		}

		// Restore depth write to avoid affecting subsequent frames or other code
		glDepthMask(GL_TRUE);

		// --- END of frame rendering inside main loop
		// Add GLFW event polling and buffer swap so the window remains responsive
		glfwSwapBuffers(g_window);
		glfwPollEvents();
	}

	// --- FIXED MODEL DELETION: Use explicit braces and set to nullptr ---
	if (gunModel) { delete gunModel; gunModel = nullptr; }
	if (itemModelRoll) { delete itemModelRoll; itemModelRoll = nullptr; }
	if (itemModelSkip) { delete itemModelSkip; itemModelSkip = nullptr; }
	if (itemModelMove) { delete itemModelMove; itemModelMove = nullptr; }
	if (tableModel) { delete tableModel; tableModel = nullptr; }
	if (chairModel) { delete chairModel; chairModel = nullptr; }
	// Delete NecoArc models if loaded
	if (playerModelP1) { delete playerModelP1; playerModelP1 = nullptr; }
	if (playerModelP2) { delete playerModelP2; playerModelP2 = nullptr; }
	// --- END FIXED MODEL DELETION ---

	// Shutdown: delete animations/animators
	if (sitAnimation) { delete sitAnimation; sitAnimation = nullptr; }
	if (danceAnimation) { delete danceAnimation; danceAnimation = nullptr; }
	if (deathAnimation) { delete deathAnimation; deathAnimation = nullptr; }
	if (animatorP1) { delete animatorP1; animatorP1 = nullptr; }
	if (animatorP2) { delete animatorP2; animatorP2 = nullptr; }

	glfwTerminate();
	return 0;
}

// ====================================================
// === Callbacks ===
// ====================================================
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	// Normalize to [0,1]
	float normX = (float)xpos / SCR_WIDTH;
	float normY = 1.0f - ((float)ypos / SCR_HEIGHT);

	bool buttonClicked = false;

	// Restart button priority (when Won screen is shown)
	if (currentGameState == STATE_GAME && (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex)) {
		MenuButton restartBtn;
		restartBtn.size = glm::vec2(BUTTON_WIDTH_NORM * 1.0f, BUTTON_HEIGHT_NORM * 1.0f);
		restartBtn.position = glm::vec2((1.0f - restartBtn.size.x) / 2.0f, MARGIN_Y);
		float minX = restartBtn.position.x;
		float maxX = restartBtn.position.x + restartBtn.size.x;
		float minY = restartBtn.position.y;
		float maxY = restartBtn.position.y + restartBtn.size.y;
		if (normX >= minX && normX <= maxX && normY >= minY && normY <= maxY) {
			buttonClicked = true;
			// Initialize game state first which will rebuild the buttons
			startGameInit();
			return;
		}
	}

	// Check active buttons
	for (const auto& btn : activeButtons) {
		float minX = btn.position.x;
		float maxX = btn.position.x + btn.size.x;
		float minY = btn.position.y;
		float maxY = btn.position.y + btn.size.y;

		if (normX >= minX && normX <= maxX && normY >= minY && normY <= maxY) {
			buttonClicked = true;

			// Character select palette buttons (actionCode20..23 and30..33)
			if (currentGameState == STATE_CHAR_SELECT) {
				if (btn.actionCode >= 20 && btn.actionCode < 24) {
					selectedPaletteP1 = btn.actionCode - 20;
					std::cout << "Player1 selected palette " << selectedPaletteP1 << std::endl;
					return;
				}
				if (btn.actionCode >= 30 && btn.actionCode < 34) {
					selectedPaletteP2 = btn.actionCode - 30;
					std::cout << "Player2 selected palette " << selectedPaletteP2 << std::endl;
					return;
				}
				if (btn.actionCode == 5) {
					if (selectedPaletteP1 != -1 && selectedPaletteP2 != -1) {
						currentGameState = STATE_GAME;
						// startGameInit will set player1Turn = true and call setupGameButtons()
						startGameInit();
					}
					else {
						std::cout << "Both players must select a palette before proceeding." << std::endl;
					}
					return;
				}
				// Back button in character select should return to main menu (actionCode ==3)
				if (btn.actionCode == 3) {
					setupMainMenu();
					currentGameState = STATE_MENU;
					return;
				}
			}

			// Visibility check for game-state
			if (currentGameState == STATE_GAME) {
				bool showActionUI = currentStatusTex != player1GotTex && currentStatusTex != player2GotTex && currentStatusTex != player1WonTex && currentStatusTex != player2WonTex;
				if (isPlayerTurnIndicator(currentStatusTex) && currentGameState == STATE_GAME) return;
				bool isVisible = false;
				bool isWonScene = (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex);

				if (btn.isGameAction || btn.actionCode == 3 || btn.actionCode == 4) {
					isVisible = showActionUI;
					if (btn.actionCode == 3 && isWonScene) isVisible = true;
					if (btn.actionCode == 4 && !gameOver && !isPlayerTurnIndicator(currentStatusTex) && currentStatusTex != clickTex) isVisible = true;
				}
				else if (btn.isItemSlot) {
					isVisible = showActionUI && currentGameState == STATE_GAME;
				}

				if (!isVisible) continue;
			}

			// Menu transitions
			if (!btn.isGameAction && !btn.isItemSlot && btn.actionCode < 3) {
				if (btn.nextState == (GameState)-1) {
					glfwSetWindowShouldClose(window, true);
				}
				else {
					currentGameState = btn.nextState;
					if (currentGameState == STATE_SUBMENU_START) setupStartSubMenu();
					else if (currentGameState == STATE_GAME) { startGameInit(); }
					else if (currentGameState == STATE_MENU) setupMainMenu();
					else if (currentGameState == STATE_CREDITS) { activeButtons.clear(); activeButtons.push_back({ backButtonTex, glm::vec2(MARGIN_X, MARGIN_Y), glm::vec2(BUTTON_WIDTH_NORM *0.5f, BUTTON_HEIGHT_NORM *0.5f), STATE_MENU, false,3, glm::vec3(1.0f) }); }
					else if (currentGameState == STATE_CHAR_SELECT) { charSelectingPlayer = 1; selectedPaletteP1 = -1; selectedPaletteP2 = -1; setupCharacterSelect(); }
				}
				// Start credit timer when entering credits
				creditStartTime = (float)glfwGetTime();
				return;
			}

			// Actions
			bool canAct = !(isPlayerTurnIndicator(currentStatusTex) && currentGameState == STATE_GAME);
			if (btn.isGameAction) {
				if (currentGameState == STATE_GAME && ((float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY || gameOver || !canAct)) return;
				if (!gameOver) handleGameAction(btn.actionCode);
				return;
			}
			if (btn.isItemSlot && currentGameState == STATE_GAME) {
				if (gameOver || (float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY || !canAct || showItemDescription) return;
				int slotIndex = btn.actionCode - 10; useItem(player1Turn, slotIndex); return;
			}
			if (btn.actionCode == 3) {
				if (currentGameState == STATE_GAME) {
					if (showItemDescription) { showItemDescription = false; setupGameButtons(); }
					else { setupMainMenu(); currentGameState = STATE_MENU; }
				}
				else if (currentGameState == STATE_CREDITS) { setupMainMenu(); currentGameState = STATE_MENU; }
				return;
			}
			if (btn.actionCode == 4) {
				if (currentGameState == STATE_GAME && !gameOver && !isPlayerTurnIndicator(currentStatusTex) && currentStatusTex != clickTex) showItemDescription = !showItemDescription;
				return;
			}
		}
	}

	// Clicks outside buttons
	if (!buttonClicked && currentGameState == STATE_GAME) {
		float elapsed = (float)glfwGetTime() - statusImageTime;
		if (isPlayerTurnIndicator(currentStatusTex) && elapsed > MIN_TURN_DELAY) {
			currentStatusTex = 0; statusImageTime = 0.0f; turnIndicatorSkippedManually = true;
		}
		else if (currentStatusTex == player1GotTex || currentStatusTex == player2GotTex) {
			currentStatusTex = (currentStatusTex == player1GotTex) ? player2WonTex : player1WonTex;
			statusImageTime = 0.0f; turnIndicatorSkippedManually = true; return;
		}
	}
}
