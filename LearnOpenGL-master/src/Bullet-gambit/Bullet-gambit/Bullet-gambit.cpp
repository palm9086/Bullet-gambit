#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>
#include <learnopengl/shader.h>
#include <learnopengl/model.h>

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

// --- State and UI Structures ---
enum GameState {
	STATE_MENU,
	STATE_GAME,
	STATE_CREDITS,
	STATE_SUBMENU_START
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

// NEW TEXTURES
unsigned int backButtonTex = 0;
unsigned int descriptionButtonTex = 0;
unsigned int itemDescriptionPanelTex = 0;
unsigned int slot1Tex = 0, slot2Tex = 0, slot3Tex = 0, slot4Tex = 0;
// NEW: Restart Button Texture ID
unsigned int restartButtonTex = 0;

std::vector<MenuButton> activeButtons;
unsigned int quadVAO = 0;
Shader* menuShader = nullptr;
Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f, lastFrame = 0.0f;
float lastActionTime = 0.0f;
Model* gunModel = nullptr;
// --- UPDATED MODEL POINTERS ---
Model* itemModelRoll = nullptr;
Model* itemModelSkip = nullptr;
Model* itemModelMove = nullptr;
// --- END UPDATED MODEL POINTERS ---

GLFWwindow* g_window = nullptr;

// Game Data
bool player1Turn = true;
bool gameOver = false;
bool chamber[6];
int currentChamber = 0;
std::string gameMessage = "Player 1's turn";
float statusImageTime = 0.0f;
unsigned int currentStatusTex = 0;
bool turnIndicatorSkippedManually = false;

// NEW UI State
bool showItemDescription = false;
// Item System
enum ItemType { ITEM_NONE = 0, ITEM_ROLL = 1, ITEM_MOVE_BULLET = 2, ITEM_SKIP = 3 };
std::vector<ItemType> player1Items;
std::vector<ItemType> player2Items;

// --- Function Prototypes ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void processInput(GLFWwindow* window);
void renderQuad(float texVStart = 0.0f, float texVEnd = 1.0f);
unsigned int loadTexture(const char* path);
void setupMainMenu();
void setupStartSubMenu();
void setupGameButtons();
void handleGameAction(int action);
void startGameInit();
void updateHUD();
void giveRandomItem(bool forPlayer1);
void useItem(bool forPlayer1, int slot);
void getItemUVs(ItemType type, float& vStart, float& vEnd);
int randomInt(int min, int max) {
	return min + rand() % (max - min + 1);
}

// --- New Model Validation Helper ---
Model* loadAndValidateModel(const std::string& path, const std::string& name) {
	try {
		Model* model = new Model(FileSystem::getPath(path));
		std::cout << "SUCCESS: Loaded model '" << name << "' from " << path << std::endl;
		return model;
	}
	catch (const std::exception& e) {
		std::cerr << "FAILURE: Model '" << name << "' failed to load from " << path << std::endl;
		std::cerr << "         Error: " << e.what() << std::endl;
		std::cerr << "         ACTION: This model will not be rendered (Access Violation 0xc0000005 averted)."
			<< std::endl;
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


// --- Game Logic Functions ---
// MODIFIED: This function is now simplified to only ensure the base turn message is set, 
// as the game loop handles the full window title with item information conditionally.
void updateHUD()
{
	glfwSetWindowTitle(g_window, ("Bullet Gambit | " + gameMessage).c_str());
}

void giveRandomItem(bool forPlayer1)
{
	if (forPlayer1 && player1Items.size() >= 4) return;
	if (!forPlayer1 && player2Items.size() >= 4) return;
	int itemID = randomInt(1, 3);
	ItemType newItem = static_cast<ItemType>(itemID);

	if (forPlayer1) player1Items.push_back(newItem);
	else player2Items.push_back(newItem);
	std::cout << (forPlayer1 ? "Player 1" : "Player 2") << " got item: " << itemName(newItem) << std::endl;
}

void useItem(bool forPlayer1, int slot)
{
	auto& items = forPlayer1 ? player1Items : player2Items;
	if (slot < 0 || slot >= (int)items.size()) return;

	ItemType item = items[slot];
	items.erase(items.begin() + slot);

	lastActionTime = (float)glfwGetTime();
	switch (item)
	{
	case ITEM_ROLL:
		std::fill(std::begin(chamber), std::end(chamber), false);
		chamber[randomInt(0, 5)] = true;
		currentChamber = 0;
		std::cout << "Chamber rolled!" << std::endl;
		break;
	case ITEM_MOVE_BULLET:
		currentChamber = (currentChamber + 1) % 6;
		std::cout << "Bullet moved forward one chamber (now at index " << currentChamber << ")."
			<< std::endl;
		break;
	case ITEM_SKIP:
		player1Turn = !player1Turn;
		gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
		std::cout << (forPlayer1 ? "Player 1" : "Player 2") << " used SKIP. Turn immediately passes to the opponent."
			<< std::endl;
		break;
	default: break;
	}

	// Set turn indicator if SKIP item was used
	if (item == ITEM_SKIP) {
		currentStatusTex = player1Turn ?
			player1Tex : player2Tex;
		statusImageTime = (float)glfwGetTime();
	}

	updateHUD();
}

void startGameInit()
{
	std::fill(std::begin(chamber), std::end(chamber), false);
	chamber[randomInt(0, 5)] = true;
	currentChamber = 0;
	player1Turn = true;
	gameOver = false;
	player1Items.clear();
	player2Items.clear();
	lastActionTime = (float)glfwGetTime();
	turnIndicatorSkippedManually = false;
	showItemDescription = false;

	giveRandomItem(true);
	giveRandomItem(false);
	gameMessage = "Player 1's turn";
	currentStatusTex = player1Tex;
	statusImageTime = (float)glfwGetTime();
	updateHUD();
}

// MODIFIED: Rotation logic removed
void handleGameAction(int action)
{
	if (gameOver) return;

	lastActionTime = (float)glfwGetTime();
	statusImageTime = (float)glfwGetTime();
	turnIndicatorSkippedManually = false;
	bool fired = chamber[currentChamber];
	currentChamber = (currentChamber + 1) % 6;
	bool turnSwitched = false;
	// 1. Instant Rotation to the target for visual feedback of the shot (REMOVED)

	// 2. Resolve the shot
	if (action == 1) // Shoot Opponent (Foe button)
	{
		if (fired)
		{
			currentStatusTex = player1Turn ?
				player2GotTex : player1GotTex;
			gameMessage = player1Turn ? "P1 shot P2 - P1 Wins!" : "P2 shot P1 - P2 Wins!";
			gameOver = true;
		}
		else
		{
			currentStatusTex = clickTex;
			player1Turn = !player1Turn;
			gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
			turnSwitched = true;
		}
	}
	else if (action == 2) // Shoot Self (Safe button)
	{
		if (fired)
		{
			currentStatusTex = player1Turn ? player1GotTex : player2GotTex;
			gameMessage = player1Turn ? "P1 shot self - P2 Wins!" : "P2 shot self - P1 Wins!";
			gameOver = true;
		}
		else
		{
			currentStatusTex = clickTex;
			giveRandomItem(player1Turn);
			player1Turn = !player1Turn;
			gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
			turnSwitched = true;
		}
	}

	// 3. After resolution (if turn switched or game over), instantly reset the rotation to the Foe/Neutral state (0 degrees).

	updateHUD();
}

// --- OpenGL/Utility Functions ---
unsigned int loadTexture(const char* path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	int width, height, nrComponents;
	std::string fullPath = FileSystem::getPath(path);
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &nrComponents, 0);
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
		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load: " << fullPath << std::endl;
		unsigned char fallbackData[] = { 255, 255, 255, 255 };
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallbackData);
	}
	return textureID;
}

// Modified renderQuad to support sub-texture rendering (for the item slots)
void renderQuad(float texVStart, float texVEnd)
{
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			// Pos (x, y, z)          // TexCoords (u, v)
			-1.0f,	1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,	1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
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
#define BUTTON_SPACING (BUTTON_HEIGHT_NORM + 0.03f)
#define ITEM_SLOT_SIZE_NORM (120.0f / SCR_HEIGHT)

void setupMainMenu() {
	activeButtons.clear();

	// --- MODIFIED: Calculate centered positions ---
	float buttonW = BUTTON_WIDTH_NORM;
	float buttonH = BUTTON_HEIGHT_NORM;
	float spacing = BUTTON_SPACING;

	// Center X for all buttons
	float centerX = (1.0f - buttonW) / 2.0f;

	// Total height of the stack from Quit base to Start top
	// Stack: Quit (Y), Credit (Y + SPACING), Start (Y + 2*SPACING)
	// Total height is (Start_Y + H) - Quit_Y = 2 * SPACING + H
	float TSH = 2.0f * spacing + buttonH;

	// Base Y position (Quit button's Y) to vertically center the stack
	float startY = 0.3f - (TSH / 2.0f);
	// --- END MODIFIED ---

	// Quit Button (Bottom of the centered stack)
	activeButtons.push_back({ quitButtonTex, glm::vec2(centerX, startY), glm::vec2(buttonW, buttonH), (GameState)-1, false, 0, glm::vec3(0.8f, 0.2f, 0.2f) });
	// Credit Button (Middle of the centered stack)
	activeButtons.push_back({ creditButtonTex, glm::vec2(centerX, startY + spacing), glm::vec2(buttonW, buttonH), STATE_CREDITS, false, 0, glm::vec3(0.2f, 0.4f, 0.8f) });
	// Start Button (Top of the centered stack)
	activeButtons.push_back({ startButtonTex, glm::vec2(centerX, startY + 2.0f * spacing), glm::vec2(buttonW, buttonH), STATE_SUBMENU_START, false, 0, glm::vec3(0.2f, 0.8f, 0.2f) });
}

void setupStartSubMenu() {
	setupMainMenu();

	// --- MODIFIED: Recalculate centered start button position ---
	float buttonW = BUTTON_WIDTH_NORM;
	float spacing = BUTTON_SPACING;
	float buttonH = BUTTON_HEIGHT_NORM;

	float centerX = (1.0f - buttonW) / 2.0f;
	float TSH = 2.0f * spacing + buttonH;
	float startY = 0.3f - (TSH / 2.0f);

	float startButtonBaseY = startY + 2.0f * spacing; // New Centered Start Button Y
	float startButtonBaseX = centerX; // New Centered Start Button X
	// --- END MODIFIED ---

	// Sub-buttons placed relative to the new centered Start button
	float subButtonX = startButtonBaseX + buttonW + 0.02f; // Right of the Start button
	float subButtonWidth = buttonW * 0.6f;
	float subButtonHeight = buttonH * 0.6f;
	float subButtonVOffset = (buttonH - subButtonHeight) / 2.0f; // Vertical alignment offset

	activeButtons.push_back({ twoPlayerButtonTex, glm::vec2(subButtonX, startButtonBaseY + subButtonVOffset), glm::vec2(subButtonWidth, subButtonHeight), STATE_GAME, false, 0, glm::vec3(0.2f, 0.7f, 0.7f) });
	activeButtons.push_back({ botButtonTex, glm::vec2(subButtonX + subButtonWidth + 0.01f, startButtonBaseY + subButtonVOffset), glm::vec2(subButtonWidth, subButtonHeight), STATE_SUBMENU_START, false, 0, glm::vec3(0.5f, 0.5f, 0.5f) });
}

void setupGameButtons() {
	activeButtons.clear();
	float btnW = BUTTON_WIDTH_NORM * 1.2f;
	float btnH = BUTTON_HEIGHT_NORM * 1.2f;
	// 1. Increased Horizontal Spacing for Foe/Safe buttons (Now 0.50f)
	float H_SPACING = 0.50f;
	float TotalWidth = btnW * 2.0f + H_SPACING;

	// 1. Foe/Safe buttons (Center of the scene)
	float startX = (1.0f - TotalWidth) / 2.0f;
	float btnY = (1.0f - btnH) / 2.0f; // Center Y

	// MODIFIED: SWAPPED SAFE (Action 2) and FOE (Action 1) positions
	activeButtons.push_back({ safeButtonTex, glm::vec2(startX, btnY), glm::vec2(btnW, btnH), STATE_GAME, true, 2, glm::vec3(0.2f, 0.8f, 0.2f) });
	activeButtons.push_back({ foeButtonTex, glm::vec2(startX + btnW + H_SPACING, btnY), glm::vec2(btnW, btnH), STATE_GAME, true, 1, glm::vec3(0.8f, 0.2f, 0.2f) });
	// 2. New UI Buttons (Top Left/Right)
	activeButtons.push_back({ backButtonTex, glm::vec2(MARGIN_X, 1.0f - MARGIN_Y - BUTTON_HEIGHT_NORM * 0.5f), glm::vec2(BUTTON_WIDTH_NORM * 0.5f, BUTTON_HEIGHT_NORM * 0.5f), STATE_MENU, false, 3, glm::vec3(1.0f, 1.0f, 1.0f) });
	activeButtons.push_back({ descriptionButtonTex, glm::vec2(1.0f - MARGIN_X - BUTTON_WIDTH_NORM * 0.5f, 1.0f - MARGIN_Y - BUTTON_HEIGHT_NORM * 0.5f), glm::vec2(BUTTON_WIDTH_NORM * 0.5f, BUTTON_HEIGHT_NORM * 0.5f), STATE_GAME, false, 4, glm::vec3(1.0f, 1.0f, 1.0f) });
	// 3. Item Slot Buttons (Bottom of the scene)
	// Horizontal spacing between items is ITEM_SLOT_SIZE_NORM * 1.2f.
	float ITEM_SPACING = ITEM_SLOT_SIZE_NORM * 1.2f;
	float itemXBase = (1.0f - ITEM_SLOT_SIZE_NORM * 4.0f) / 2.0f - ITEM_SLOT_SIZE_NORM * 0.2f;
	float itemY = MARGIN_Y; // Bottom
	for (int i = 0; i < 4; ++i) {
		activeButtons.push_back({
			(i == 0 ? slot1Tex : (i == 1 ? slot2Tex : (i == 2 ? slot3Tex : slot4Tex))),
			glm::vec2(itemXBase + (float)i * ITEM_SPACING, itemY), // Use ITEM_SPACING here
			glm::vec2(ITEM_SLOT_SIZE_NORM, ITEM_SLOT_SIZE_NORM),
			STATE_GAME,
			false,
			10 + i,
			glm::vec3(1.0f, 1.0f, 1.0f),
			true
			});
	}
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
	// 1. Load Main Gun Model
	gunModel = loadAndValidateModel("resources/objects/gun/fullgun.dae", "Gun Model");
	// 2. Load Roll Item Model (Restored to Bread)
	itemModelRoll = loadAndValidateModel("resources/objects/bread/bread.dae", "Roll Item Model (Bread)");
	// 3. Load Skip Item Model (Restored to Uno/Untitled.dae, assuming it is available now)
	itemModelSkip = loadAndValidateModel("resources/objects/uno/Untitled.dae", "Skip Item Model (Uno Card)");
	// 4. Load Move Item Model (Restored to Watch)
	itemModelMove = loadAndValidateModel("resources/objects/watch/Untitled.dae", "Move Item Model (Watch)");


	// Load Textures
	menuBackgroundTex = loadTexture("resources/textures/menu/menu.png");
	startButtonTex = loadTexture("resources/textures/menu/Start button.png");
	creditButtonTex = loadTexture("resources/textures/menu/Credit button.png");
	quitButtonTex = loadTexture("resources/textures/menu/Quit button.png");
	twoPlayerButtonTex = loadTexture("resources/textures/menu/2 player button.png");
	botButtonTex = loadTexture("resources/textures/menu/bot button.png");
	safeButtonTex = loadTexture("resources/textures/menu/safe button.png");
	foeButtonTex = loadTexture("resources/textures/menu/foe button.png");

	player1Tex = loadTexture("resources/textures/menu/Player1.png");
	player2Tex = loadTexture("resources/textures/menu/Player2.png");
	player1GotTex = loadTexture("resources/textures/menu/Player1got.png");
	player2GotTex = loadTexture("resources/textures/menu/Player2got.png");
	player1WonTex = loadTexture("resources/textures/menu/Player1Won.png");
	player2WonTex = loadTexture("resources/textures/menu/Player2Won.png");
	clickTex = loadTexture("resources/textures/menu/Click.png");

	backButtonTex = loadTexture("resources/textures/menu/Back.png");
	descriptionButtonTex = loadTexture("resources/textures/menu/Description.png");
	itemDescriptionPanelTex = loadTexture("resources/textures/menu/item description.png");
	slot1Tex = loadTexture("resources/textures/menu/slot1.png");
	slot2Tex = loadTexture("resources/textures/menu/slot2.png");
	slot3Tex = loadTexture("resources/textures/menu/slot3.png");
	slot4Tex = loadTexture("resources/textures/menu/slot4.png");

	// NEW: Load Restart Button Texture
	restartButtonTex = loadTexture("resources/textures/menu/Restart.png");


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
		if (currentGameState == STATE_MENU || currentGameState == STATE_SUBMENU_START)
		{
			// Menu Rendering
			glDisable(GL_DEPTH_TEST);
			if (menuShader) menuShader->use();
			if (menuShader) menuShader->setFloat("alpha", 1.0f);
			if (menuShader) menuShader->setFloat("vStart", 0.0f); // Default to full texture
			if (menuShader) menuShader->setFloat("vEnd", 1.0f);   // Default to full texture

			glBindTexture(GL_TEXTURE_2D, menuBackgroundTex);
			if (menuShader) menuShader->setVec2("offset", glm::vec2(0.0f, 0.0f));
			if (menuShader) menuShader->setVec2("scale", glm::vec2(1.0f, 1.0f));
			if (menuShader) menuShader->setVec3("color", glm::vec3(0.0f, 0.0f, 0.1f));
			renderQuad();
			for (const auto& button : activeButtons)
			{
				if (menuShader) menuShader->setVec2("offset", button.position);
				if (menuShader) menuShader->setVec2("scale", button.size);
				glBindTexture(GL_TEXTURE_2D, button.textureID);
				if (menuShader) menuShader->setVec3("color", button.color);
				renderQuad();
			}
		}
		else if (currentGameState == STATE_CREDITS)
		{
			glDisable(GL_DEPTH_TEST);
			glClearColor(0.0f, 0.0f, 0.3f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		else if (currentGameState == STATE_GAME)
		{
			bool inCooldown = ((float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY);

			// 3D Rendering (Gun Model)
			glEnable(GL_DEPTH_TEST);
			// CRITICAL CHECK: Ensure shader is valid before using it
			if (ourShader.ID == 0) {
				// Skip 3D rendering if the main shader is invalid
				std::cerr << "Warning: Main shader is invalid. Skipping 3D rendering."
					<< std::endl;
			}
			else {

				ourShader.use();
				glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
				glm::mat4 view = camera.GetViewMatrix();
				ourShader.setMat4("projection", projection);
				ourShader.setMat4("view", view);

				if (gunModel)
				{
					// compute target world point from mouse ray intersecting z=0 plane
					glm::vec3 rayOrigin = camera.Position;
					// normalized device coords
					float mx = lastX / (float)SCR_WIDTH;
					float my = lastY / (float)SCR_HEIGHT;
					float ndcX = mx * 2.0f - 1.0f;
					float ndcY = my * 2.0f - 1.0f;

					glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
					glm::mat4 view = camera.GetViewMatrix();
					glm::mat4 invProj = glm::inverse(projection);
					glm::mat4 invView = glm::inverse(view);

					// Ray in clip space
					glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
					// Eye space
					glm::vec4 rayEye = invProj * rayClip;
					rayEye.z = -1.0f; rayEye.w = 0.0f;
					// World space direction
					glm::vec4 rayWorld4 = invView * rayEye;
					glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld4));

					glm::vec3 targetWorld = glm::vec3(0.0f);
					if (fabs(rayDir.z) > 1e-6f) {
						float t = -rayOrigin.z / rayDir.z; // intersect with z=0 plane
						if (t > 0.0f) targetWorld = rayOrigin + rayDir * t;
						else targetWorld = rayOrigin + rayDir * 5.0f; // fallback
					}
					else {
						// ray parallel to plane - fallback to point far along ray
						targetWorld = rayOrigin + rayDir * 5.0f;
					}

					// Build orientation so that model's local +X axis points toward targetWorld
					glm::vec3 gunPos = glm::vec3(0.0f);
					glm::vec3 dir = glm::normalize(targetWorld - gunPos);
					// avoid degenerate
					if (glm::length(dir) < 1e-6f) dir = glm::vec3(1.0f, 0.0f, 0.0f);

					glm::vec3 up(0.0f, 1.0f, 0.0f);
					// MODIFIED: xAxis now equals dir as requested
					glm::vec3 xAxis = dir; // left side follows target
					glm::vec3 zAxis = glm::normalize(glm::cross(xAxis, up));
					glm::vec3 yAxis = glm::normalize(glm::cross(-zAxis, xAxis));
					// Preserve prior vertical inversion if applied

					glm::mat4 orient(1.0f);
					orient[0] = glm::vec4(xAxis, 0.0f);
					orient[1] = glm::vec4(yAxis, 0.0f);
					orient[2] = glm::vec4(zAxis, 0.0f);
					// Apply base model rotation that was previously used (camera orientation)
					glm::mat4 baseRot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

					// Compose final model matrix: translate -> apply flip -> orient -> baseRot -> scale
					glm::mat4 model = glm::mat4(1.0f);
					model = glm::translate(model, gunPos);

					// Apply 180-degree rotation around World Y axis to flip the gun horizontally BEFORE orientation
					model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

					model = model * orient * baseRot;
					model = glm::scale(model, glm::vec3(0.5f));
					ourShader.setMat4("model", model);
					gunModel->Draw(ourShader);
				}

				// --- ITEM MODEL RENDERING WITH CUSTOM SCALING AND TRANSLATION ---
				auto& currentItems = player1Turn ? player1Items : player2Items;

				// Fix: Re-calculate Item Slot Positions variables (ITEM_SPACING, itemXBase) here for scope
				float ITEM_SPACING = ITEM_SLOT_SIZE_NORM * 1.2f;
				float itemXBase = (1.0f - ITEM_SLOT_SIZE_NORM * 4.0f) / 2.0f - ITEM_SLOT_SIZE_NORM * 0.2f;

				// World coordinate mapping (approximate)
				float worldX_scale = 10.0f;
				float worldY_scale = worldX_scale * ((float)SCR_HEIGHT / (float)SCR_WIDTH);

				for (int i = 0; i < (int)currentItems.size(); ++i) {
					ItemType type = currentItems[i];
					Model* modelToDraw = nullptr;

					// Assign model pointer based on type.
					if (type == ITEM_ROLL) modelToDraw = itemModelRoll;
					else if (type == ITEM_SKIP) modelToDraw = itemModelSkip;
					else if (type == ITEM_MOVE_BULLET) modelToDraw = itemModelMove;

					if (modelToDraw) {
						// SAFE CHECK: Only draw if the model pointer is valid (not nullptr).
						// Calculate normalized center X of the button
						float normX_center = itemXBase + (float)i * ITEM_SPACING + ITEM_SLOT_SIZE_NORM / 2.0f;

						// MODIFIED: Calculate normalized Y position for the model (Increased from 2.5f to 3.5f)
						float normY_top_center = MARGIN_Y + ITEM_SLOT_SIZE_NORM * 3.5f;

						// Convert normalized screen position to 3D world space using the new top-middle Y
						float worldX = (normX_center - 0.5f) * worldX_scale;
						float worldY = (normY_top_center - 0.5f) * worldY_scale;
						float worldZ = -0.5f;
						glm::mat4 model = glm::mat4(1.0f);

						model = glm::translate(model, glm::vec3(worldX, worldY, worldZ));
						// Specific scaling and rotation adjustments for each model
						float scale = 1.0f;
						if (type == ITEM_ROLL) {
							// Roll Object: scaled for visibility
							scale = 0.04f;
							// orient upright
							model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
							// Keep roll's rotation but spin around Z axis (bread special case)
							model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
						}
						else if (type == ITEM_SKIP) {
							// Skip Object: small scale and spin in Y axis
							scale = 0.005f;
							// User Request: "filp it in x axis for 90"
							model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
							model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
						}
						else if (type == ITEM_MOVE_BULLET) {
							// Move Object: scaled 2x larger than previous 2.0f -> 4.0f
							scale = 4.0f;
							// User Request: "filp it in x axis for 90"
							model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
							// Add spin around the Y-axis
							model = glm::rotate(model, (float)glfwGetTime() * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
						}

						model = glm::scale(model, glm::vec3(scale));
						ourShader.setMat4("model", model);
						modelToDraw->Draw(ourShader);
					}
				}

				// --- END ITEM MODEL RENDERING ---
			}

			// 2D UI Rendering
			glDisable(GL_DEPTH_TEST);
			if (menuShader) menuShader->use();
			float elapsed = (float)glfwGetTime() - statusImageTime;
			float alpha = 1.0f;

			// Determine if action buttons should be shown.
			bool showActionUI = true;
			// Hide UI if it's currently showing a "Got" or "Won" image.
			showActionUI = currentStatusTex != player1GotTex && currentStatusTex != player2GotTex && currentStatusTex != player1WonTex && currentStatusTex != player2WonTex;

			// Special case: If turn indicator or click feedback is on, hide the action/utility buttons
			if (isPlayerTurnIndicator(currentStatusTex) || currentStatusTex == clickTex) {
				showActionUI = false;
			}


			// 1. Handle Game Over Transition: Shot -> Winner
			if (gameOver) {
				// The explicit 5.0s delay is bypassed by the click logic below
				if (elapsed > GAME_OVER_TRANSITION_DELAY && (currentStatusTex == player1GotTex || currentStatusTex == player2GotTex)) {
					currentStatusTex = (currentStatusTex == player1GotTex) ?
						player2WonTex : player1WonTex;
				}
			}
			// 2. Handle Non-Game-Over Status Effects
			else if (currentStatusTex != 0) {
				// Player Turn Indicator: Hide on click OR after 2.0s
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

			// 3. Manual Skip Handling
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
						if (i < (int)items.size())
							itemStatus += "[" + std::to_string(i + 1) + ":" + itemName(items[i]) + "] ";
						else
							itemStatus += "[" + std::to_string(i + 1) + ":Empty] ";
					}
					baseTitle += " | " + itemStatus;
					baseTitle += "| Action: Click Safe/Foe Buttons | 1-4: Use Item | R: Restart";
				}
				else {
					// Show minimal message when status image is covering
					baseTitle += " | Waiting for action (Click or wait to continue)...";
				}
			}
			glfwSetWindowTitle(g_window, baseTitle.c_str());
			// === END UPDATED: Window Title Logic ===

			// Draw Status Image
			if (currentStatusTex != 0) {
				if (currentStatusTex == clickTex && elapsed > 0.0f) {
					alpha = 1.0f - std::min(1.0f, elapsed / STATUS_IMAGE_DURATION);
				}
				if (menuShader) menuShader->setVec2("offset", glm::vec2(0.0f, 0.0f));
				if (menuShader) menuShader->setVec2("scale", glm::vec2(1.0f, 1.0f));
				glBindTexture(GL_TEXTURE_2D, currentStatusTex);
				if (menuShader) menuShader->setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
				if (menuShader) menuShader->setFloat("alpha", alpha);
				renderQuad();
			}

			// Draw all 2D UI elements
			if (menuShader) menuShader->setFloat("alpha", 1.0f);
			for (const auto& button : activeButtons) {
				bool showButton = false;
				glm::vec3 finalColor = button.color;

				// Determine visibility based on game state
				bool isWonScene = (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex);

				// Show the button if it's part of the action phase AND showActionUI is true
				if (button.isGameAction || button.actionCode == 3 || button.actionCode == 4) { // Action (Safe/Foe), Back, Description
					showButton = showActionUI;

					// NEW: Back button exception for Game Over 'Won' state (actionCode 3)
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
					// Only the back button is shown in credits, which is handled by actionCode 3
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
					renderQuad();
				}
			}

			// --- MODIFIED: Draw Restart Button in Won Scene (Position lowered, Size smaller) ---
			if (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex) {
				// Define button properties for rendering
				MenuButton restartBtn;
				restartBtn.textureID = restartButtonTex;
				// Size reduced to 1.0f
				restartBtn.size = glm::vec2(BUTTON_WIDTH_NORM * 1.0f, BUTTON_HEIGHT_NORM * 1.0f);
				// Position is MARGIN_Y, which is the bottom edge
				restartBtn.position = glm::vec2((1.0f - restartBtn.size.x) / 2.0f, MARGIN_Y);
				restartBtn.color = glm::vec3(1.0f, 1.0f, 1.0f);

				// Render the restart button
				if (menuShader) menuShader->setVec2("offset", restartBtn.position);
				if (menuShader) menuShader->setVec2("scale", restartBtn.size);
				glBindTexture(GL_TEXTURE_2D, restartButtonTex);
				if (menuShader) menuShader->setVec3("color", restartBtn.color);
				renderQuad();
			}

			// Draw Item Description Panel (if active)
			if (showItemDescription) {
				// Convert 600x640 pixels to normalized screen space
				float panelW_Norm = 600.0f / SCR_WIDTH;
				float panelH_Norm = 640.0f / SCR_HEIGHT;
				// Center the panel
				float panelX = (1.0f - panelW_Norm) / 2.0f;
				float panelY = (1.0f - panelH_Norm) / 2.0f;
				if (menuShader) menuShader->setVec2("offset", glm::vec2(panelX, panelY));
				if (menuShader) menuShader->setVec2("scale", glm::vec2(panelW_Norm, panelH_Norm));
				glBindTexture(GL_TEXTURE_2D, itemDescriptionPanelTex);
				if (menuShader) menuShader->setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
				renderQuad();
			}

			glEnable(GL_DEPTH_TEST);
		}

		glfwSwapBuffers(g_window);
		glfwPollEvents();
	}

	// --- FIXED MODEL DELETION: Use explicit braces and set to nullptr ---
	if (gunModel) { delete gunModel; gunModel = nullptr; }
	if (itemModelRoll) { delete itemModelRoll; itemModelRoll = nullptr; }
	if (itemModelSkip) { delete itemModelSkip; itemModelSkip = nullptr; }
	if (itemModelMove) { delete itemModelMove; itemModelMove = nullptr; }
	// --- END FIXED MODEL DELETION ---

	glfwTerminate();
	return 0;
}

// ====================================================
// === Callbacks ===
// ====================================================
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

	// Item usage keybinds 1, 2, 3, 4
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
			if (itemSlot < (int)items.size())
			{
				// Check for turn indicator before using item
				if (isPlayerTurnIndicator(currentStatusTex) && ((float)glfwGetTime() - statusImageTime < TURN_SKIP_DURATION)) {
					// Do not allow item use if turn indicator is up, until the indicator times out (or is clicked/skipped)
					return;
				}
				// Also prevent item use if any other status image is active
				if (currentStatusTex != 0 && currentStatusTex != clickTex) {
					return;
				}
				useItem(player1Turn, itemSlot);
			}
		}
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		// Normalize to [0, 1] for button comparison
		float normX = (float)xpos / SCR_WIDTH;
		float normY = 1.0f - ((float)ypos / SCR_HEIGHT); // Y is flipped in OpenGL

		bool buttonClicked = false;

		// === NEW: Restart Button Priority Check (Higher Priority) ===
		if (currentGameState == STATE_GAME && (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex))
		{
			// Define button properties (must match rendering in main loop)
			MenuButton restartBtn;
			// Size reduced to 1.0f
			restartBtn.size = glm::vec2(BUTTON_WIDTH_NORM * 1.0f, BUTTON_HEIGHT_NORM * 1.0f);
			// Position is MARGIN_Y, which is the bottom edge
			restartBtn.position = glm::vec2((1.0f - restartBtn.size.x) / 2.0f, MARGIN_Y);

			float minX = restartBtn.position.x;
			float maxX = restartBtn.position.x + restartBtn.size.x;
			float minY = restartBtn.position.y;
			float maxY = restartBtn.position.y + restartBtn.size.y;

			if (normX >= minX && normX <= maxX && normY >= minY && normY <= maxY)
			{
				buttonClicked = true;
				// Restart the game
				setupGameButtons();
				startGameInit();
				return;
			}
		}
		// === END Restart Button Priority Check ===

		for (const auto& btn : activeButtons)
		{
			float minX = btn.position.x;
			float maxX = btn.position.x + btn.size.x;
			float minY = btn.position.y;
			float maxY = btn.position.y + btn.size.y;

			if (normX >= minX && normX <= maxX && normY >= minY && normY <= maxY)
			{
				buttonClicked = true;

				// --- NEW: Visibility Check for Intractability (STATE_GAME only matters) ---
				if (currentGameState == STATE_GAME) {
					// Replicate the showActionUI logic from the rendering loop
					bool showActionUI = true;
					showActionUI = currentStatusTex != player1GotTex && currentStatusTex != player2GotTex && currentStatusTex != player1WonTex && currentStatusTex != player2WonTex;
					if (isPlayerTurnIndicator(currentStatusTex) || currentStatusTex == clickTex) {
						showActionUI = false;
					}

					bool isVisible = false;
					bool isWonScene = (currentStatusTex == player1WonTex || currentStatusTex == player2WonTex);

					// Foe/Safe (isGameAction), Back (3), Description (4)
					if (btn.isGameAction || btn.actionCode == 3 || btn.actionCode == 4) {
						isVisible = showActionUI;

						// NEW: Back button exception for Game Over 'Won' state (actionCode 3)
						if (btn.actionCode == 3 && isWonScene) {
							isVisible = true;
						}

						// Special case for 'Description' button: always show it if not game over and not in a turn indicator state
						if (btn.actionCode == 4 && !gameOver && !isPlayerTurnIndicator(currentStatusTex) && currentStatusTex != clickTex) {
							isVisible = true;
						}
					}
					// Item Slots
					else if (btn.isItemSlot) {
						isVisible = showActionUI;
					}

					// Skip processing if the button is logically hidden (uninteractable)
					if (!isVisible) {
						continue;
					}
				}
				// --- END NEW: Visibility Check ---

				// Handle Menu Transitions (Non-Action/Utility Buttons)
				if (!btn.isGameAction && !btn.isItemSlot && btn.actionCode < 3) {
					if (btn.nextState == (GameState)-1) {
						glfwSetWindowShouldClose(window, true);
					}
					else {
						currentGameState = btn.nextState;
						if (currentGameState == STATE_SUBMENU_START) {
							setupStartSubMenu();
						}
						else if (currentGameState == STATE_GAME) {
							setupGameButtons();
							startGameInit();
						}
						else if (currentGameState == STATE_MENU) {
							setupMainMenu();
						}
						else if (currentGameState == STATE_CREDITS) {
							activeButtons.clear(); // Clear buttons for the blank credits screen
							// Add back button for credits screen
							activeButtons.push_back({ backButtonTex, glm::vec2(MARGIN_X, MARGIN_Y), glm::vec2(BUTTON_WIDTH_NORM * 0.5f, BUTTON_HEIGHT_NORM * 0.5f), STATE_MENU, false, 3, glm::vec3(1.0f, 1.0f, 1.0f) });
						}
					}
					return;
				}

				// Check for visibility logic for actions, utility, and items (now implicitly checked by the block above, but keeping for checks)
				bool canAct = !(isPlayerTurnIndicator(currentStatusTex) && currentGameState == STATE_GAME);

				// Handle Game Actions (Foe/Safe)
				if (btn.isGameAction) {
					if (currentGameState == STATE_GAME && ((float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY || gameOver || !canAct)) {
						return;
					}
					if (!gameOver) handleGameAction(btn.actionCode);
					return;
				}

				// Handle Item Slot Buttons (Action codes 10-13)
				if (btn.isItemSlot && currentGameState == STATE_GAME) {
					// Check visibility constraints again
					if (gameOver || (float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY || !canAct || showItemDescription) {
						return;
					}
					int slotIndex = btn.actionCode - 10;
					useItem(player1Turn, slotIndex);
					return;
				}

				// Handle Utility Buttons (Action codes 3 and 4)
				if (btn.actionCode == 3) { // Back button
					if (currentGameState == STATE_GAME) {
						// If the Description Panel is open, close it instead of going back
						if (showItemDescription) {
							showItemDescription = false;
							setupGameButtons(); // Re-setup to show action buttons
						}
						else {
							setupMainMenu();
							currentGameState = STATE_MENU;
						}
					}
					else if (currentGameState == STATE_CREDITS) {
						setupMainMenu();
						currentGameState = STATE_MENU;
					}
					return;
				}
				else if (btn.actionCode == 4) { // Description button
					if (currentGameState == STATE_GAME && !gameOver && !isPlayerTurnIndicator(currentStatusTex) && currentStatusTex != clickTex) {
						showItemDescription = !showItemDescription;
					}
					return;
				}
			}
		}

		// Handle Clicks outside of buttons: Skip status screens
		if (!buttonClicked && currentGameState == STATE_GAME)
		{
			float elapsed = (float)glfwGetTime() - statusImageTime;
			// Allow skipping of turn indicator after MIN_TURN_DELAY (0.5s)
			if (isPlayerTurnIndicator(currentStatusTex) && elapsed > MIN_TURN_DELAY)
			{
				currentStatusTex = 0;
				statusImageTime = 0.0f;
				turnIndicatorSkippedManually = true;
			}
			// MODIFIED: Only allow skipping of 'Got Shot' status to jump to 'Victory' scene.
			else if (currentStatusTex == player1GotTex || currentStatusTex == player2GotTex)
			{
				// If currently on a 'Got' screen, transition immediately to the 'Won' screen.
				if (currentStatusTex == player1GotTex) {
					currentStatusTex = player2WonTex; // P1 Got -> P2 Won
				}
				else { // Must be player2GotTex
					currentStatusTex = player1WonTex; // P2 Got -> P1 Won
				}
				statusImageTime = 0.0f;
				turnIndicatorSkippedManually = true;
				return;
			}
		}
	}
}

void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos)
{
	if (currentGameState == STATE_GAME)
	{
		if (firstMouse)
		{
			lastX = (float)xpos;
			lastY = (float)ypos;
			firstMouse = false;
		}

		lastX = (float)xpos;
		lastY = (float)ypos;
	}
}