// engine_sim.cpp
// Landing page + Inline-4 IC engine animation with exhaust smoke, engine sound,
// and college logo image on landing page.
// C++ + FreeGLUT + MSVC (Windows).

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include <GL/freeglut.h>
#include <cmath>
#include <cstdlib>
#include <ctime>

// ---------- stb_image (for PNG logo) ----------
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------- APP STATE ----------------

enum AppState {
    STATE_LANDING = 0,
    STATE_ENGINE  = 1
};

AppState appState = STATE_LANDING;

int winWidth  = 1800;
int winHeight = 1600;

// Landing page button
const float BTN_WIDTH  = 260.0f;
const float BTN_HEIGHT = 60.0f;
const float BTN_Y      = 90.0f;   // distance from bottom

// ---------------- ENGINE GLOBALS ----------------

float crankAngle = 0.0f;      // 0–720 degrees (4-stroke)
float rpm        = 250.0f;
int   lastTimeMs = 0;

// Camera
float camRotX = -35.0f;
float camRotY =  20.0f;
float camZoom =  20.0f;

// Inline-4 configuration
const int   NUM_CYL = 4;
const float cylX[NUM_CYL]     = { -3.0f, -1.0f,  1.0f,  3.0f };
const float cylPhase[NUM_CYL] = {   0.0f, 180.0f, 360.0f, 540.0f };

// ---------------- SMOKE PARTICLES ----------------

struct SmokeParticle {
    bool  active;
    float x, y, z;
    float vx, vy, vz;
    float life; // 0..1
};

const int MAX_SMOKE = 400;
SmokeParticle smokeP[MAX_SMOKE];

// ---------------- TEXTURE (LOGO) ----------------

GLuint logoTexID = 0;

GLuint loadTexture(const char* filename)
{
    int w, h, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filename, &w, &h, &channels, 4);
    

    if (!data) {
        MessageBoxA(nullptr, "Failed to load logo image (logocg.png)", "Error", MB_OK);
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        w, h, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data
    );

    stbi_image_free(data);
    return tex;
}

// Forward declarations
void displayWrapper();
void displayLanding();
void displayEngine();
void updateSmoke(float dt);
void drawSmoke();
void startEngineSimulation();

// ---------------- SMOKE FUNCTIONS ----------------

void spawnSmoke(float x, float y, float z)
{
    for (int i = 0; i < MAX_SMOKE; ++i) {
        if (!smokeP[i].active) {
            smokeP[i].active = true;
            smokeP[i].x = x;
            smokeP[i].y = y;
            smokeP[i].z = z;

            float ang = ((float)std::rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float speed = 0.3f + 0.2f * ((float)std::rand() / RAND_MAX);

            smokeP[i].vx = std::cos(ang) * speed * 0.1f;
            smokeP[i].vz = std::sin(ang) * speed * 0.1f;
            smokeP[i].vy = 0.5f + 0.3f * ((float)std::rand() / RAND_MAX);

            smokeP[i].life = 1.0f;
            return;
        }
    }
}

void updateSmoke(float dt)
{
    for (int i = 0; i < MAX_SMOKE; ++i) {
        if (!smokeP[i].active) continue;

        smokeP[i].life -= dt * 0.35f;
        if (smokeP[i].life <= 0.0f) {
            smokeP[i].active = false;
            continue;
        }

        smokeP[i].x += smokeP[i].vx * dt;
        smokeP[i].y += smokeP[i].vy * dt;
        smokeP[i].z += smokeP[i].vz * dt;

        smokeP[i].vx *= (1.0f - 0.3f * dt);
        smokeP[i].vz *= (1.0f - 0.3f * dt);
    }
}

void drawSmoke()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < MAX_SMOKE; ++i) {
        if (!smokeP[i].active) continue;

        float a    = smokeP[i].life * 0.6f;
        float size = 0.25f + (1.0f - smokeP[i].life) * 0.35f;
        float s    = size * 0.5f;

        glPushMatrix();
        glTranslatef(smokeP[i].x, smokeP[i].y, smokeP[i].z);
        glColor4f(0.75f, 0.75f, 0.75f, a);

        glBegin(GL_QUADS);
        // front
        glVertex3f(-s,-s, s); glVertex3f( s,-s, s);
        glVertex3f( s, s, s); glVertex3f(-s, s, s);
        // back
        glVertex3f(-s,-s,-s); glVertex3f(-s, s,-s);
        glVertex3f( s, s,-s); glVertex3f( s,-s,-s);
        // left
        glVertex3f(-s,-s,-s); glVertex3f(-s,-s, s);
        glVertex3f(-s, s, s); glVertex3f(-s, s,-s);
        // right
        glVertex3f( s,-s,-s); glVertex3f( s, s,-s);
        glVertex3f( s, s, s); glVertex3f( s,-s, s);
        // top
        glVertex3f(-s, s,-s); glVertex3f(-s, s, s);
        glVertex3f( s, s, s); glVertex3f( s, s,-s);
        // bottom
        glVertex3f(-s,-s,-s); glVertex3f( s,-s,-s);
        glVertex3f( s,-s, s); glVertex3f(-s,-s, s);
        glEnd();

        glPopMatrix();
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ---------------- BASIC DRAW HELPERS ----------------

void drawBox(float w, float h, float d)
{
    w *= 0.5f; h *= 0.5f; d *= 0.5f;

    glBegin(GL_QUADS);
    // Front
    glNormal3f(0,0,1);
    glVertex3f(-w,-h,d); glVertex3f( w,-h,d);
    glVertex3f( w, h,d); glVertex3f(-w, h,d);
    // Back
    glNormal3f(0,0,-1);
    glVertex3f(-w,-h,-d); glVertex3f(-w, h,-d);
    glVertex3f( w, h,-d); glVertex3f( w,-h,-d);
    // Left
    glNormal3f(-1,0,0);
    glVertex3f(-w,-h,-d); glVertex3f(-w,-h,d);
    glVertex3f(-w, h,d);  glVertex3f(-w, h,-d);
    // Right
    glNormal3f(1,0,0);
    glVertex3f( w,-h,-d); glVertex3f( w, h,-d);
    glVertex3f( w, h,d);  glVertex3f( w,-h,d);
    // Top
    glNormal3f(0,1,0);
    glVertex3f(-w, h,-d); glVertex3f(-w, h,d);
    glVertex3f( w, h,d);  glVertex3f( w, h,-d);
    // Bottom
    glNormal3f(0,-1,0);
    glVertex3f(-w,-h,-d); glVertex3f( w,-h,-d);
    glVertex3f( w,-h,d);  glVertex3f(-w,-h,d);
    glEnd();
}

void drawDisk(float radius, int slices)
{
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(1,0,0);       // facing +X
    glVertex3f(0,0,0);
    for (int i = 0; i <= slices; ++i) {
        float th = (float)i / slices * 2.0f * (float)M_PI;
        float y = radius * std::cos(th);
        float z = radius * std::sin(th);
        glVertex3f(0,y,z);
    }
    glEnd();
}

// ---------------- ENGINE MATH ----------------

float getPistonHeight(float angleDeg)
{
    const float R = 0.75f;
    const float L = 2.4f;

    float th = angleDeg * (float)M_PI / 180.0f;
    float s  = std::sin(th);
    float c  = std::cos(th);

    float y = R * c + std::sqrt(L*L - R*R*s*s);

    float minY = L - R;
    float maxY = L + R;
    float t = (y - minY) / (maxY - minY);

    return 1.5f + t * 3.2f;
}

void getValveStates(float angleDeg, float &intakeLift, float &exhaustLift)
{
    float a = std::fmod(angleDeg, 720.0f);
    intakeLift  = 0.0f;
    exhaustLift = 0.0f;
    if (a < 180.0f)    intakeLift  = 1.0f;
    if (a >= 540.0f)   exhaustLift = 1.0f;
}

// ---------------- LIGHTING ----------------

void setupLighting()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat lightPos[] = { 0.0f, 12.0f, 15.0f, 1.0f };
    GLfloat amb[]      = { 0.12f, 0.12f, 0.15f, 1.0f };
    GLfloat diff[]     = { 0.9f,  0.9f,  0.95f, 1.0f };
    GLfloat spec[]     = { 1.0f,  1.0f,  1.0f,  1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spec);

    GLfloat shininess[] = { 80.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
}

// ---------------- ENGINE DISPLAY (3D) ----------------

void displayEngine()
{
    glClearColor(0.03f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Reset projection to perspective (important after landing 2D)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)winWidth / (float)winHeight, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(0.0f, -2.5f, -camZoom);
    glRotatef(camRotY, 1, 0, 0);
    glRotatef(camRotX, 0, 1, 0);

    setupLighting();

    // Ground
    glPushMatrix();
    glTranslatef(0.0f, -0.1f, 0.0f);
    glColor3f(0.08f, 0.08f, 0.09f);
    glNormal3f(0,1,0);
    glBegin(GL_QUADS);
    glVertex3f(-12,0,-7);
    glVertex3f( 12,0,-7);
    glVertex3f( 12,0, 7);
    glVertex3f(-12,0, 7);
    glEnd();
    glPopMatrix();

    // Engine block
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    glColor3f(0.10f, 0.55f, 0.10f);
    drawBox(9.2f, 1.2f, 2.6f);
    glPopMatrix();

    // Crankshaft shaft
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    glColor3f(0.05f, 0.45f, 0.05f);
    drawBox(9.2f, 0.35f, 0.7f);
    glPopMatrix();

    const float crankY = 0.8f;
    const float crankR = 0.7f;

    // Flywheel
    glPushMatrix();
    glTranslatef(cylX[0] - 1.8f, crankY, 0.0f);
    glRotatef(crankAngle, 1,0,0);
    glColor3f(0.8f, 0.15f, 0.15f);
    drawDisk(1.8f, 40);
    glPopMatrix();

    // Timing pulleys (right)
    glPushMatrix();
    glTranslatef(cylX[NUM_CYL-1] + 2.0f, 5.3f, 1.5f);
    glRotatef(-crankAngle * 0.5f, 1,0,0);
    glColor3f(0.85f, 0.15f, 0.15f);
    drawDisk(0.9f, 30);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(cylX[NUM_CYL-1] + 2.0f, crankY, 1.5f);
    glRotatef(crankAngle, 1,0,0);
    glColor3f(0.85f, 0.15f, 0.15f);
    drawDisk(1.0f, 30);
    glPopMatrix();

    // Belt frame
    glPushMatrix();
    glTranslatef(cylX[NUM_CYL-1] + 2.0f, 3.0f, 1.5f);
    glColor3f(0.10f, 0.55f, 0.95f);
    drawBox(0.25f, 4.7f, 0.2f);
    glPopMatrix();

    // Belt line
    glDisable(GL_LIGHTING);
    glColor3f(0.05f, 0.05f, 0.05f);
    glBegin(GL_LINE_STRIP);
    glVertex3f(cylX[NUM_CYL-1] + 2.0f, crankY,  1.5f);
    glVertex3f(cylX[NUM_CYL-1] + 2.0f, 5.3f,    1.5f);
    glEnd();
    glEnable(GL_LIGHTING);

    // Rocker / cam housing bar
    glPushMatrix();
    glTranslatef(0.0f, 5.5f, 0.0f);
    glColor3f(0.95f, 0.80f, 0.20f);
    drawBox(9.2f, 0.7f, 1.6f);
    glPopMatrix();

    // Cylinders, pistons, rods, valves, flames
    for (int i = 0; i < NUM_CYL; ++i) {
        float cx = cylX[i];
        float localAngle = crankAngle + cylPhase[i];

        // sleeve
        glPushMatrix();
        glTranslatef(cx, 3.2f, 0.0f);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.5f, 0.7f, 0.9f, 0.22f);
        drawBox(1.6f, 4.0f, 1.6f);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
        glPopMatrix();

        float pistonY = getPistonHeight(localAngle);

        // piston
        glPushMatrix();
        glTranslatef(cx, pistonY, 0.0f);
        glColor3f(0.96f, 0.82f, 0.20f);
        drawBox(1.4f, 0.7f, 1.4f);
        glPopMatrix();

        // crank web disc
        float th = localAngle * (float)M_PI / 180.0f;
        glPushMatrix();
        glTranslatef(cx, crankY, 0.0f);
        glRotatef(localAngle, 1,0,0);
        glColor3f(0.05f, 0.65f, 0.10f);
        drawDisk(0.6f, 28);
        glPopMatrix();

        // connecting rod
        {
            float pinY = crankY + crankR * std::cos(th);
            float pinZ = crankR * std::sin(th);

            float pistonPinY = pistonY - 0.4f;
            float pistonZ    = 0.0f;

            float midY = (pinY + pistonPinY) * 0.5f;
            float midZ = (pinZ + pistonZ)   * 0.5f;

            float dy = pistonPinY - pinY;
            float dz = pistonZ    - pinZ;
            float length = std::sqrt(dy*dy + dz*dz);

            glPushMatrix();
            glTranslatef(cx, midY, midZ);

            float angleZ = std::atan2(dz, dy) * 180.0f / (float)M_PI;
            glRotatef(-angleZ, 0,0,1);

            glColor3f(0.05f, 0.85f, 0.40f);
            drawBox(0.25f, length, 0.25f);
            glPopMatrix();
        }

        // valves
        float intakeLift, exhaustLift;
        getValveStates(localAngle, intakeLift, exhaustLift);

        glPushMatrix();
        glTranslatef(cx - 0.35f, 5.7f + intakeLift*0.6f, 0.35f);
        glColor3f(0.15f, 1.0f, 0.25f);
        drawBox(0.18f, 0.9f, 0.18f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(cx + 0.35f, 5.7f + exhaustLift*0.6f, -0.35f);
        glColor3f(0.95f, 0.15f, 0.15f);
        drawBox(0.18f, 0.9f, 0.18f);
        glPopMatrix();

        // flame
        float phase = std::fmod(localAngle, 720.0f);
        if (phase >= 360.0f && phase < 540.0f) {
            float t = (phase - 360.0f) / 180.0f;
            float flicker = 0.35f + 0.25f * std::sin(t * 6.0f);

            glPushMatrix();
            glTranslatef(cx, 4.6f, 0.0f);
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1.0f, 0.46f, 0.08f, flicker);
            drawBox(1.25f, 1.0f, 1.25f);
            glDisable(GL_BLEND);
            glEnable(GL_LIGHTING);
            glPopMatrix();
        }
    }

    // smoke last
    drawSmoke();

    glutSwapBuffers();
}

// ---------------- LANDING PAGE (2D) ----------------

void drawLandingText(float x, float y, const char* str, void* font)
{
    glRasterPos2f(x, y);
    for (const char* p = str; *p; ++p)
        glutBitmapCharacter(font, *p);
}

void displayLanding()
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glClearColor(0.80f, 0.88f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, winWidth, 0, winHeight);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float centerX = winWidth / 2.0f;
    glColor3f(0.0f, 0.25f, 0.6f);

    const char* line1 = "Dayananda Sagar Academy of Technology & Management";
    const char* line2 = "Autonomous Institute under VTU";
    const char* line3 = "Department of Computer Science and Engineering";
    const char* line4 = "Internal Combustion Engine Simulation (OpenGL)";

    auto drawCentered = [&](float y, const char* text, void* font) {
        int w = 0;
        for (const char* p = text; *p; ++p)
            w += glutBitmapWidth(font, *p);
        float x = centerX - w / 2.0f;
        drawLandingText(x, y, text, font);
    };

    drawCentered(winHeight - 80,  line1, GLUT_BITMAP_HELVETICA_18);
    drawCentered(winHeight - 110, line2, GLUT_BITMAP_HELVETICA_12);
    drawCentered(winHeight - 135, line3, GLUT_BITMAP_HELVETICA_12);
    drawCentered(winHeight - 170, line4, GLUT_BITMAP_HELVETICA_18);

    // Team names (edit for your group)
    drawCentered(winHeight - 210, "Harshavardhan S  1DT23CS072", GLUT_BITMAP_HELVETICA_12);
    drawCentered(winHeight - 230, "Laxmikant        1DT23CS105",       GLUT_BITMAP_HELVETICA_12);
    drawCentered(winHeight - 250, "Likhith P        1DT23CS107",       GLUT_BITMAP_HELVETICA_12);
    drawCentered(winHeight - 270, "Mithun S         1DT23CS124",       GLUT_BITMAP_HELVETICA_12);

    // ---- Logo image (logocg.png) ----
    float logoSize = 160.0f;
    float logoX = centerX - logoSize / 2.0f;
    float logoY = winHeight / 2.0f - logoSize / 2.0f + 20.0f;

    if (logoTexID != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, logoTexID);
        glColor3f(1.0f, 1.0f, 1.0f); // no tint

        glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(logoX,             logoY);
        glTexCoord2f(1,0); glVertex2f(logoX+logoSize,    logoY);
        glTexCoord2f(1,1); glVertex2f(logoX+logoSize,    logoY+logoSize);
        glTexCoord2f(0,1); glVertex2f(logoX,             logoY+logoSize);
        glEnd();

        glDisable(GL_TEXTURE_2D);
    } else {
        // fallback: white box if texture failed
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(logoX,          logoY);
        glVertex2f(logoX+logoSize, logoY);
        glVertex2f(logoX+logoSize, logoY+logoSize);
        glVertex2f(logoX,          logoY+logoSize);
        glEnd();
    }

    // Start button
    float btnX = centerX - BTN_WIDTH / 2.0f;
    float btnY = BTN_Y;

    glColor3f(0.0f, 0.45f, 0.90f);
    glBegin(GL_QUADS);
    glVertex2f(btnX,          btnY);
    glVertex2f(btnX+BTN_WIDTH,btnY);
    glVertex2f(btnX+BTN_WIDTH,btnY+BTN_HEIGHT);
    glVertex2f(btnX,          btnY+BTN_HEIGHT);
    glEnd();

    glColor3f(0.0f, 0.25f, 0.6f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(btnX,          btnY);
    glVertex2f(btnX+BTN_WIDTH,btnY);
    glVertex2f(btnX+BTN_WIDTH,btnY+BTN_HEIGHT);
    glVertex2f(btnX,          btnY+BTN_HEIGHT);
    glEnd();
    glLineWidth(1.0f);

    const char* btnText = "START APPLICATION";
    int tw = 0;
    for (const char* p = btnText; *p; ++p)
        tw += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, *p);
    float tx = centerX - tw / 2.0f;
    float ty = btnY + BTN_HEIGHT / 2.0f - 5.0f;
    glColor3f(1.0f, 1.0f, 1.0f);
    drawLandingText(tx, ty, btnText, GLUT_BITMAP_HELVETICA_18);

    glutSwapBuffers();
}

// ---------------- STATE SWITCH ----------------

void startEngineSimulation()
{
    if (appState == STATE_ENGINE) return;
    appState = STATE_ENGINE;

    PlaySound(TEXT("engine_loop.wav"), NULL,
              SND_ASYNC | SND_LOOP | SND_FILENAME);

    lastTimeMs = glutGet(GLUT_ELAPSED_TIME);
}

// ---------------- DISPLAY WRAPPER ----------------

void displayWrapper()
{
    if (appState == STATE_LANDING)
        displayLanding();
    else
        displayEngine();
}

// ---------------- IDLE (ANIMATION) ----------------

void idle()
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (lastTimeMs == 0) lastTimeMs = now;
    float dtMs = (float)(now - lastTimeMs);
    float dt   = dtMs / 1000.0f;
    lastTimeMs = now;

    if (appState == STATE_ENGINE) {
        float degPerMs = rpm * 360.0f / 60000.0f;
        crankAngle += degPerMs * dtMs;
        if (crankAngle >= 720.0f) crankAngle -= 720.0f;

        // exhaust smoke
        for (int i = 0; i < NUM_CYL; ++i) {
            float localAngle = crankAngle + cylPhase[i];
            float phase = std::fmod(localAngle, 720.0f);
            if (phase >= 540.0f && phase < 720.0f) {
                float spawnChance = 0.03f + (rpm / 4000.0f) * 0.05f;
                float r = (float)std::rand() / RAND_MAX;
                if (r < spawnChance) {
                    float exX = cylX[i] + 0.7f;
                    float exY = 5.9f;
                    float exZ = -0.5f;
                    spawnSmoke(exX, exY, exZ);
                }
            }
        }

        updateSmoke(dt);
    }

    glutPostRedisplay();
}

// ---------------- RESHAPE ----------------

void reshape(int w, int h)
{
    winWidth  = w;
    winHeight = (h == 0 ? 1 : h);

    glViewport(0, 0, winWidth, winHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)winWidth/(float)winHeight, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

// ---------------- INPUT ----------------

void keyboard(unsigned char key, int, int)
{
    if (appState == STATE_LANDING) {
        if (key == 13 || key == ' ' || key == 's' || key == 'S')
            startEngineSimulation();
        else if (key == 27) {
            PlaySound(NULL, 0, 0);
            std::exit(0);
        }
        return;
    }

    switch (key)
    {
    case 27:
        PlaySound(NULL, 0, 0);
        std::exit(0);
        break;
    case 'w': camRotY += 2.0f; break;
    case 's': camRotY -= 2.0f; break;
    case 'a': camRotX -= 2.0f; break;
    case 'd': camRotX += 2.0f; break;
    case '+':
    case '=':
        camZoom -= 0.5f;
        if (camZoom < 12.0f) camZoom = 12.0f;
        break;
    case '-':
        camZoom += 0.5f;
        if (camZoom > 28.0f) camZoom = 28.0f;
        break;
    case 'z':
        rpm -= 40.0f;
        if (rpm < 80.0f) rpm = 80.0f;
        break;
    case 'x':
        rpm += 40.0f;
        if (rpm > 3500.0f) rpm = 3500.0f;
        break;
    case '1':
        rpm = 150.0f; break;
    case '2':
        rpm = 400.0f; break;
    case '3':
        rpm = 900.0f; break;
    }
}

void mouse(int button, int state, int x, int y)
{
    if (appState != STATE_LANDING) return;
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

    int yInv = winHeight - y;

    float btnX = winWidth / 2.0f - BTN_WIDTH / 2.0f;
    float btnY = BTN_Y;

    if (x >= btnX && x <= btnX + BTN_WIDTH &&
        yInv >= btnY && yInv <= btnY + BTN_HEIGHT) {
        startEngineSimulation();
    }
}

// ---------------- MAIN ----------------

int main(int argc, char** argv)
{
    std::srand((unsigned int)std::time(nullptr));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winWidth, winHeight);
    glutCreateWindow("Internal Combustion Engine Simulation");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    // load logo texture (logocg.png in project root)
    logoTexID = loadTexture("logocg.png");

    glutDisplayFunc(displayWrapper);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);

    glutMainLoop();
    return 0;
}
