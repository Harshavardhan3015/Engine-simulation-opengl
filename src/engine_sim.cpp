#include <iostream>
#include <cmath>
#include <vector>

// --- Third-party Library Headers ---
// You will need to install and link these libraries.
#include <GL/glew.h>  // GLEW for managing OpenGL extensions
#include <GLFW/glfw3.h> // GLFW for creating the window and context
#include <glm/glm.hpp>  // GLM for vector math (useful for transformations)
#include <glm/gtc/matrix_transform.hpp>

// --- Constants and Global Variables ---

// Window settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;
const char* WINDOW_TITLE = "OpenGL Engine Simulation - 4 Stroke";

// Engine simulation state
float engine_speed_rpm = 1000.0f; // Current RPM
float crank_angle_degrees = 0.0f; // Crankshaft angle (0 to 720 for a full cycle)
bool is_running = false;
bool show_labels = true;

// Engine geometry (simplified)
const float BORE = 0.5f;       // Cylinder diameter
const float STROKE = 1.0f;     // Total travel of the piston (2 * crank_radius)
const float CRANK_RADIUS = 0.5f; // R
const float CONROD_LENGTH = 2.0f; // L (must be > R)
const int NUM_CYLINDERS = 4;

// Time tracking for smooth movement
float last_frame_time = 0.0f;

// --- Function Declarations ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void render_scene();
void draw_piston_assembly(float crank_angle_degrees, float x_offset, int cylinder_index);
float get_piston_position(float angle_rad);
std::string get_stroke_name(int cylinder_index);
void draw_ui_controls();

// --- Main Function ---

int main() {
    // 1. Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 2. Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 3. Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // Initialize ImGui (optional, but highly recommended for the UI)
    // NOTE: ImGui setup code would go here. For simplicity, this example
    // uses basic OpenGL drawing for the UI elements, which is complicated.
    // In a real project, use ImGui.

    // 4. Game Loop (Render Loop)
    while (!glfwWindowShouldClose(window)) {
        // --- Timing ---
        float current_time = glfwGetTime();
        float delta_time = current_time - last_frame_time;
        last_frame_time = current_time;

        // --- Input ---
        processInput(window);

        // --- Update Simulation ---
        if (is_running) {
            // Convert RPM to degrees per second
            float degrees_per_second = engine_speed_rpm * 6.0f; // (RPM * 360 degrees) / 60 seconds
            
            // Update the crank angle
            crank_angle_degrees += degrees_per_second * delta_time;
            
            // Keep the angle within a full cycle (0 to 720 degrees for 4-stroke)
            if (crank_angle_degrees >= 720.0f) {
                crank_angle_degrees -= 720.0f;
            }
        }

        // --- Render ---
        render_scene();

        // --- Poll and swap ---
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 5. Terminate GLFW
    // ImGui shutdown code would also go here.
    glfwTerminate();
    return 0;
}

// --- Function Definitions ---

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    // Simple keyboard controls for Start/Pause/Reset
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        is_running = !is_running;
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        crank_angle_degrees = 0.0f;
        is_running = false;
    }
}

/**
 * @brief Calculates the linear piston position (y-axis) based on the crank angle.
 * This is the Rod-Piston Kinematics formula.
 * @param angle_rad The crank angle in radians.
 * @return The vertical position of the piston head.
 */
float get_piston_position(float angle_rad) {
    // Standard piston position formula:
    // y = -R * cos(theta) + L * sqrt(1 - (R/L)^2 * sin^2(theta))
    // Note: The sign and offset depends on your coordinate system.
    
    // Piston position relative to the center of the cylinder:
    float R = CRANK_RADIUS;
    float L = CONROD_LENGTH;
    float sin_sq = std::pow(std::sin(angle_rad), 2);
    
    // We adjust the formula to place 0 at the center of the stroke for simpler drawing
    float displacement = -R * std::cos(angle_rad) + L * std::sqrt(1.0f - std::pow(R / L, 2) * sin_sq);
    
    // Adjust so the piston position is 'down' in screen space (positive y is up in OpenGL world)
    // and the maximum value corresponds to the top of the cylinder.
    return displacement;
}

/**
 * @brief Determines the current stroke phase for a given cylinder.
 * The 4-stroke cycle is 720 degrees.
 * Firing Order for a simple inline-4 is typically 1-3-4-2.
 * @param cylinder_index The index of the cylinder (0, 1, 2, 3 for 1-4).
 * @return The name of the current stroke.
 */
std::string get_stroke_name(int cylinder_index) {
    // 4-Cylinder Firing Order: 1-3-4-2. The phases are offset by 180 degrees (360/2).
    // Cylinder 1 (Index 0) is at angle A.
    // Cylinder 3 (Index 2) is at angle A + 180.
    // Cylinder 4 (Index 3) is at angle A + 360.
    // Cylinder 2 (Index 1) is at angle A + 540.
    
    // Crank angle relative to the cylinder's phase
    float phase_offset = 0.0f;
    if (cylinder_index == 1) phase_offset = 540.0f; // Cylinder 2
    if (cylinder_index == 2) phase_offset = 180.0f; // Cylinder 3
    if (cylinder_index == 3) phase_offset = 360.0f; // Cylinder 4
    
    float effective_angle = std::fmod(crank_angle_degrees + phase_offset, 720.0f);

    // Each stroke is 180 degrees
    if (effective_angle >= 0.0f && effective_angle < 180.0f) {
        return "INTAKE";
    } else if (effective_angle >= 180.0f && effective_angle < 360.0f) {
        return "COMPRESSION";
    } else if (effective_angle >= 360.0f && effective_angle < 540.0f) {
        return "POWER/COMBUSTION";
    } else { // 540 to 720 degrees
        return "EXHAUST";
    }
}

/**
 * @brief Draws a single piston, connecting rod, and crankshaft assembly.
 */
void draw_piston_assembly(float crank_angle_degrees, float x_offset, int cylinder_index) {
    // Simple 2D drawing (using GL_QUADS and GL_LINES - fixed function, for simplicity)

    glPushMatrix();
    glTranslatef(x_offset, 0.0f, 0.0f); // Position the whole assembly

    // 1. Calculate Kinematics
    float angle_rad = glm::radians(crank_angle_degrees);
    float R = CRANK_RADIUS;
    float L = CONROD_LENGTH;
    
    // Crank Pin Center (A)
    float crank_x = R * std::sin(angle_rad);
    float crank_y = -R * std::cos(angle_rad);
    
    // Piston Center (B)
    float piston_y = get_piston_position(angle_rad);
    
    // Connecting Rod Angle (phi)
    // sin(phi) = -R/L * sin(theta)
    float sin_phi = -(R / L) * std::sin(angle_rad);
    float phi_rad = std::asin(sin_phi);
    
    // Piston Head position (Simplified BDC/TDC handling)
    float piston_pos_y = piston_y + (STROKE / 2.0f); // Base Y position

    // 2. Draw Cylinder
    glColor3f(0.3f, 0.3f, 0.3f); // Dark gray
    glBegin(GL_QUADS);
        glVertex2f(-BORE/2.0f - 0.1f, piston_pos_y + STROKE + 0.5f);
        glVertex2f( BORE/2.0f + 0.1f, piston_pos_y + STROKE + 0.5f);
        glVertex2f( BORE/2.0f + 0.1f, piston_pos_y - STROKE);
        glVertex2f(-BORE/2.0f - 0.1f, piston_pos_y - STROKE);
    glEnd();

    // 3. Draw Stroke Visualization (Gas/Flame/Smoke)
    std::string stroke = get_stroke_name(cylinder_index);
    float gas_top_y = piston_pos_y + STROKE;
    float gas_bottom_y = piston_pos_y;
    
    if (stroke == "INTAKE") glColor3f(0.8f, 0.8f, 0.8f); // White/Gray for air/fuel
    else if (stroke == "COMPRESSION") glColor3f(0.9f, 0.8f, 0.0f); // Yellow/Gold for compressed gas
    else if (stroke == "POWER/COMBUSTION") glColor3f(1.0f, 0.4f, 0.0f); // Orange/Red for explosion
    else if (stroke == "EXHAUST") glColor3f(0.5f, 0.5f, 0.5f); // Darker gray for exhaust gas

    // Draw the gas/flame block inside the cylinder
    glBegin(GL_QUADS);
        glVertex2f(-BORE/2.0f, gas_top_y);
        glVertex2f( BORE/2.0f, gas_top_y);
        glVertex2f( BORE/2.0f, gas_bottom_y);
        glVertex2f(-BORE/2.0f, gas_bottom_y);
    glEnd();

    // 4. Draw Piston
    glColor3f(0.6f, 0.6f, 0.6f); // Light gray
    glBegin(GL_QUADS);
        glVertex2f(-BORE/2.0f, piston_pos_y + 0.05f); // Piston top
        glVertex2f( BORE/2.0f, piston_pos_y + 0.05f);
        glVertex2f( BORE/2.0f, piston_pos_y - 0.2f);  // Piston bottom
        glVertex2f(-BORE/2.0f, piston_pos_y - 0.2f);
    glEnd();

    // 5. Draw Connecting Rod
    glColor3f(0.2f, 0.2f, 0.2f); // Darker gray
    glLineWidth(5.0f);
    glBegin(GL_LINES);
        // From Crank Pin (A) to Piston Pin (B, assumed at piston center)
        glVertex2f(crank_x, crank_y - CONROD_LENGTH); // Crank center is at (0, -L)
        glVertex2f(0.0f, piston_pos_y);
    glEnd();
    
    // 6. Draw Crankshaft (simplified as a circle and connecting line)
    // ... This part is complex and typically uses a proper model or more advanced GL drawing
    
    // 7. Draw Stroke Label (Text Rendering is complicated in OpenGL, but conceptually...)
    if (show_labels) {
        // Conceptually, you would use a text rendering library here (e.g., FreeType)
        // to draw the 'stroke' string above the cylinder.
        // Example: draw_text(stroke.c_str(), -0.5f, piston_pos_y + STROKE + 0.6f);
    }

    glPopMatrix();
}

/**
 * @brief Sets up the view and calls rendering functions.
 */
void render_scene() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark background
    glClear(GL_COLOR_BUFFER_BIT);

    // --- Setup 2D Orthographic Projection ---
    // The world coordinate system will be centered at (0, 0) and extend to +/- 10.0
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // This allows you to define your drawing coordinates easily.
    // e.g., an x-range of -8 to 8 and a y-range of -10 to 10
    float aspect = (float)SCR_WIDTH / (float)SCR_HEIGHT;
    glOrtho(-8.0f * aspect, 8.0f * aspect, -10.0f, 10.0f, -1.0f, 1.0f);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // --- Draw Engine Cylinders (Inline-4) ---
    float x_spacing = 3.0f;
    float start_x = -(NUM_CYLINDERS - 1) * x_spacing / 2.0f;

    for (int i = 0; i < NUM_CYLINDERS; ++i) {
        // The crank angle for an inline-4 with 180-degree offset journals
        // is the main crank angle plus an offset of 0, 540, 180, or 360 degrees.
        // We handle the effective angle inside draw_piston_assembly via cylinder_index
        
        draw_piston_assembly(crank_angle_degrees, start_x + (i * x_spacing), i);
    }
    
    // --- Draw UI ---
    draw_ui_controls(); // Conceptual UI drawing

    glFlush();
}

/**
 * @brief Conceptually draws the UI elements seen in the image.
 * In a real project, this would be ImGui code.
 */
void draw_ui_controls() {
    // This function would be hundreds of lines of complex OpenGL calls to draw
    // rectangles, circles, and render text for buttons, sliders, and labels.
    // Since this is highly non-trivial to implement from scratch and is best done
    // with a dedicated library like ImGui, we will only leave the placeholder.
    
    // Conceptual placeholder for the UI:
    // ... ImGui::Begin("Controls");
    // ... ImGui::Checkbox("Start/Pause", &is_running);
    // ... ImGui::SliderFloat("Speed (RPM)", &engine_speed_rpm, 100.0f, 5000.0f);
    // ... ImGui::End();
}