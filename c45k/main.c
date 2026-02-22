#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>
#include "ibkr_c_api.h"
#include <cjson/cJSON.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#define MAX_ITEMS 50
#define MAX_ACCOUNT_VALUES 128
#define MAX_POSITIONS 256

typedef struct ScanRow {
	int rank;
	char symbol[32];
	char secType[16];
	char currency[8];
} ScanRow;

typedef struct AccountValueRow {
	char key[64];
	char value[32];
	char currency[8];
	char accountName[32];
} AccountValueRow;

typedef struct PositionRow {
	char account[32];
	char symbol[32];
	char secType[16];
	double position;
	double marketPrice;
	double marketValue;
	double averageCost;
	double unrealizedPNL;
	double realizedPNL;
} PositionRow;

static ScanRow g_scan_rows[MAX_ITEMS];
static int g_scan_count = 0;
static int g_scan_done = 0;
static int g_scan_error = 0;

static IbkrHandle g_ibkr = NULL;

static AccountValueRow g_account_values[MAX_ACCOUNT_VALUES];
static int g_account_value_count = 0;
static PositionRow g_positions[MAX_POSITIONS];
static int g_position_count = 0;
static int g_account_ready = 0;

static void clear_scan_rows(void) {
	g_scan_count = 0;
	g_scan_done = 0;
	g_scan_error = 0;
	for (int i = 0; i < MAX_ITEMS; ++i) {
		g_scan_rows[i].rank = 0;
		g_scan_rows[i].symbol[0] = '\0';
		g_scan_rows[i].secType[0] = '\0';
		g_scan_rows[i].currency[0] = '\0';
	}
}

static void clear_account_data(void) {
	g_account_value_count = 0;
	g_position_count = 0;
	g_account_ready = 0;
	for (int i = 0; i < MAX_ACCOUNT_VALUES; ++i) {
		g_account_values[i].key[0] = '\0';
		g_account_values[i].value[0] = '\0';
		g_account_values[i].currency[0] = '\0';
		g_account_values[i].accountName[0] = '\0';
	}
	for (int i = 0; i < MAX_POSITIONS; ++i) {
		g_positions[i].account[0] = '\0';
		g_positions[i].symbol[0] = '\0';
		g_positions[i].secType[0] = '\0';
		g_positions[i].position = 0.0;
		g_positions[i].marketPrice = 0.0;
		g_positions[i].marketValue = 0.0;
		g_positions[i].averageCost = 0.0;
		g_positions[i].unrealizedPNL = 0.0;
		g_positions[i].realizedPNL = 0.0;
	}
}

static int find_account_value_index(const char* key) {
	for (int i = 0; i < g_account_value_count; ++i) {
		if (strcmp(g_account_values[i].key, key) == 0) return i;
	}
	return -1;
}

static int find_position_index(const char* account, const char* symbol, const char* secType) {
	for (int i = 0; i < g_position_count; ++i) {
		if (strcmp(g_positions[i].account, account) == 0 &&
			strcmp(g_positions[i].symbol, symbol) == 0 &&
			strcmp(g_positions[i].secType, secType) == 0) {
			return i;
		}
	}
	return -1;
}

static const AccountValueRow* find_account_value(const char* key) {
	int idx = find_account_value_index(key);
	return (idx >= 0) ? &g_account_values[idx] : NULL;
}

static void start_ibkr(const char* accountCode) {
	clear_scan_rows();
	clear_account_data();

	printf("Connecting to TWS at 127.0.0.1:7495 ...\n");
	g_ibkr = ibkr_create("127.0.0.1", 7495, 0);
	ibkr_connect(g_ibkr);

	printf("Requesting scanner (TOP_PERC_GAIN, price > $5) ...\n");
	ibkr_start_scanner(g_ibkr, 1, "TOP_PERC_GAIN", 5.0);

	printf("Requesting account data ...\n");
	ibkr_request_account_data(g_ibkr, accountCode);
}

static void stop_ibkr(void) {
	if (!g_ibkr) return;
	ibkr_disconnect(g_ibkr);
	SLEEP_MS(500);
	ibkr_destroy(g_ibkr);
	g_ibkr = NULL;
}

static void poll_scanner_updates(void) {
	if (!g_ibkr || g_scan_done) return;

	CScannerItem items[MAX_ITEMS];
	int result = ibkr_poll_scanner(g_ibkr, items, MAX_ITEMS);
	if (result < 0) return;

	if (result == 0) {
		g_scan_error = 1;
		g_scan_done = 1;
		return;
	}

	g_scan_count = result > MAX_ITEMS ? MAX_ITEMS : result;
	for (int i = 0; i < g_scan_count; i++) {
		g_scan_rows[i].rank = items[i].rank;
		snprintf(g_scan_rows[i].symbol, sizeof(g_scan_rows[i].symbol), "%s", items[i].symbol);
		snprintf(g_scan_rows[i].secType, sizeof(g_scan_rows[i].secType), "%s", items[i].secType);
		snprintf(g_scan_rows[i].currency, sizeof(g_scan_rows[i].currency), "%s", items[i].currency);
	}
	g_scan_done = 1;
}

static void poll_account_updates(void) {
	if (!g_ibkr) return;

	CAccountValue values[32];
	CPosition positions[64];
	int value_count = 0;
	int position_count = 0;

	if (ibkr_poll_account_data(g_ibkr, values, 32, positions, 64,
						  &value_count, &position_count) < 0) {
		return;
	}

	if (value_count > 0 || position_count > 0) {
		g_account_ready = 1;
	}

	for (int i = 0; i < value_count; ++i) {
		int idx = find_account_value_index(values[i].key);
		if (idx < 0 && g_account_value_count < MAX_ACCOUNT_VALUES) {
			idx = g_account_value_count++;
		}
		if (idx >= 0) {
			snprintf(g_account_values[idx].key, sizeof(g_account_values[idx].key), "%s", values[i].key);
			snprintf(g_account_values[idx].value, sizeof(g_account_values[idx].value), "%s", values[i].value);
			snprintf(g_account_values[idx].currency, sizeof(g_account_values[idx].currency), "%s", values[i].currency);
			snprintf(g_account_values[idx].accountName, sizeof(g_account_values[idx].accountName), "%s", values[i].accountName);
		}
	}

	for (int i = 0; i < position_count; ++i) {
		int idx = find_position_index(positions[i].account, positions[i].symbol, positions[i].secType);
		if (idx < 0 && g_position_count < MAX_POSITIONS) {
			idx = g_position_count++;
		}
		if (idx >= 0) {
			snprintf(g_positions[idx].account, sizeof(g_positions[idx].account), "%s", positions[i].account);
			snprintf(g_positions[idx].symbol, sizeof(g_positions[idx].symbol), "%s", positions[i].symbol);
			snprintf(g_positions[idx].secType, sizeof(g_positions[idx].secType), "%s", positions[i].secType);
			g_positions[idx].position = positions[i].position;
			g_positions[idx].marketPrice = positions[i].marketPrice;
			g_positions[idx].marketValue = positions[i].marketValue;
			g_positions[idx].averageCost = positions[i].averageCost;
			g_positions[idx].unrealizedPNL = positions[i].unrealizedPNL;
			g_positions[idx].realizedPNL = positions[i].realizedPNL;
		}
	}
}

static void render_scanner_results_ui(void) {
	if (g_scan_done == 0) {
		igText("Scanner status: running...");
		return;
	}
	if (g_scan_error != 0) {
		igText("Scanner status: no results received.");
		return;
	}

	igText("Scanner status: %d items", g_scan_count);
	if (igBeginTable("scanner_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, (ImVec2){0, 0}, 0)) {
		igTableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 60.0f, 0);
		igTableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
		igTableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f, 2);
		igTableSetupColumn("Currency", ImGuiTableColumnFlags_WidthFixed, 80.0f, 3);
		igTableHeadersRow();
		for (int i = 0; i < g_scan_count; ++i) {
			igTableNextRow(0, 0.0f);
			igTableNextColumn();
			igText("%d", g_scan_rows[i].rank);
			igTableNextColumn();
			igText("%s", g_scan_rows[i].symbol);
			igTableNextColumn();
			igText("%s", g_scan_rows[i].secType);
			igTableNextColumn();
			igText("%s", g_scan_rows[i].currency);
		}
		igEndTable();
	}
}

static void render_account_summary_ui(void) {
	if (!g_account_ready) {
		igText("Account status: waiting for updates...");
		return;
	}

	const AccountValueRow* net = find_account_value("NetLiquidation");
	if (net) igText("Net Liquidation: %s %s", net->value, net->currency);
	const AccountValueRow* avail = find_account_value("AvailableFunds");
	if (avail) igText("Available Funds: %s %s", avail->value, avail->currency);
	const AccountValueRow* buy = find_account_value("BuyingPower");
	if (buy) igText("Buying Power: %s %s", buy->value, buy->currency);

	igSeparator();
	if (igBeginTable("account_values_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, (ImVec2){0, 0}, 0)) {
		igTableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
		igTableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 120.0f, 1);
		igTableSetupColumn("Currency", ImGuiTableColumnFlags_WidthFixed, 80.0f, 2);
		igTableHeadersRow();
		for (int i = 0; i < g_account_value_count; ++i) {
			igTableNextRow(0, 0.0f);
			igTableNextColumn();
			igText("%s", g_account_values[i].key);
			igTableNextColumn();
			igText("%s", g_account_values[i].value);
			igTableNextColumn();
			igText("%s", g_account_values[i].currency);
		}
		igEndTable();
	}
}

static void render_positions_ui(void) {
	if (!g_account_ready) {
		igText("Positions: waiting for updates...");
		return;
	}

	igText("Current Positions (%d)", g_position_count);
	if (igBeginTable("positions_table", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, (ImVec2){0, 0}, 0)) {
		igTableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 80.0f, 0);
		igTableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 80.0f, 1);
		igTableSetupColumn("Avg Cost", ImGuiTableColumnFlags_WidthFixed, 80.0f, 2);
		igTableSetupColumn("Mkt Price", ImGuiTableColumnFlags_WidthFixed, 80.0f, 3);
		igTableSetupColumn("Mkt Value", ImGuiTableColumnFlags_WidthFixed, 100.0f, 4);
		igTableSetupColumn("Unreal P&L", ImGuiTableColumnFlags_WidthFixed, 100.0f, 5);
		igTableSetupColumn("Real P&L", ImGuiTableColumnFlags_WidthFixed, 100.0f, 6);
		igTableHeadersRow();

		for (int i = 0; i < g_position_count; ++i) {
			igTableNextRow(0, 0.0f);
			igTableNextColumn();
			igText("%s", g_positions[i].symbol);
			igTableNextColumn();
			igText("%.2f", g_positions[i].position);
			igTableNextColumn();
			igText("%.2f", g_positions[i].averageCost);
			igTableNextColumn();
			igText("%.2f", g_positions[i].marketPrice);
			igTableNextColumn();
			igText("%.2f", g_positions[i].marketValue);
			igTableNextColumn();
			igText("%.2f", g_positions[i].unrealizedPNL);
			igTableNextColumn();
			igText("%.2f", g_positions[i].realizedPNL);
		}
		igEndTable();
	}
}

const unsigned int SCR_WIDTH = 2560;
const unsigned int SCR_HEIGHT = 1280;
int main(void) {
	const char* accountCode = getenv("IBKR_ACCOUNT");
	start_ibkr(accountCode ? accountCode : "");


	// Setup Platform/Renderer backends
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Chart GLFW", NULL, NULL);
	if (window == NULL)
	{
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	//glfwSetScrollCallback(window, Renderer::ScrollCallback);

	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		printf("failed to initialize GLAD\n");
		return -1;
	}

	ImGuiContext* ctx = igCreateContext(NULL);
	ImGuiIO* io = igGetIO_Nil();
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	igStyleColorsDark(igGetStyle());

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		poll_scanner_updates();
		poll_account_updates();

		// Clear the screen with a dark background color
		glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		igNewFrame();

		igBegin("Scanner Results", NULL, 0);
		render_scanner_results_ui();
		igEnd();

		igBegin("Account Summary", NULL, 0);
		render_account_summary_ui();
		igEnd();

		igBegin("Positions", NULL, 0);
		render_positions_ui();
		igEnd();

		// igShowDemoWindow(NULL);
		igRender();
		ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());

		// Swap buffers to display the rendered frame
		glfwSwapBuffers(window);
	}

	stop_ibkr();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	igDestroyContext(ctx);
	glfwTerminate();

	return 0;

}