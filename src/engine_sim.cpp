// engine_sim.cpp
// 4-Cylinder Engine Simulation with Landing Page (OpenGL + GLUT / freeglut)
// Includes centering so the engine is always centered in the window.
// Compile (Linux):
//   g++ src/engine_sim.cpp -o engine_sim -lGL -lGLU -lglut
// Windows MinGW (MSYS2):
//   g++ src/engine_sim.cpp -o engine_sim.exe -lfreeglut -lopengl32 -lglu32
// Windows MSVC (Developer Command Prompt):
//   cl /EHsc src\engine_sim.cpp /Fe:engine_sim.exe freeglut.lib opengl32.lib glu32.lib

#include <GL/glut.h>
#include <cmath>
#include <ctime>
#include <string>
#include <algorithm>

// MSVC does not always define M_PI, M_PI_2 — define manually if missing
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

// Window
int winW = 900, winH = 360;

// Animation
float crankAngle = 0.0f;            // degrees
float crankSpeedDegPerSec = 90.0f;  // degrees per second (adjust speed)
int lastTime = 0;

// Engine geometry
const float cylinderWidth = 120.0f;
const float cylinderHeight = 160.0f;
const float pistonWidth = 70.0f;
const float pistonHeight = 90.0f;
const int numCyl = 4;
const float blockTopY = 220.0f;
const float blockLeftX = 40.0f;
const float spacing = 170.0f;

// Visual stroke / mapping
const float stroke = 80.0f;   // piston stroke (vertical travel)
const float crankRadius = stroke / 2.0f;
const float conRodLen = 120.0f;

// UI States
enum AppState { LANDING, ANIMATION };
AppState appState = LANDING;

// Landing page button bounds (pixels)
float btnX = 0, btnY = 0, btnW = 260, btnH = 56;
bool hoverBtn = false;

//////////////////////////////////////////////////////////////////////////
// Utility drawing helpers
//////////////////////////////////////////////////////////////////////////
void drawFilledRect(float cx, float cy, float w, float h) {
    float x0 = cx - w/2.0f;
    float y0 = cy - h/2.0f;
    glBegin(GL_QUADS);
      glVertex2f(x0, y0);
      glVertex2f(x0 + w, y0);
      glVertex2f(x0 + w, y0 + h);
      glVertex2f(x0, y0 + h);
    glEnd();
}

void drawRoundedRect(float cx, float cy, float w, float h, float radius = 8.0f, int segments = 12) {
    // simple rounded rectangle by drawing central rect + corner circles
    drawFilledRect(cx, cy, w - 2*radius, h);
    drawFilledRect(cx - (w/2.0f - radius), cy, 2*radius, h - 2*radius);
    drawFilledRect(cx + (w/2.0f - radius), cy, 2*radius, h - 2*radius);
    // corners
    float corners[4][2] = {
        {cx - w/2.0f + radius, cy - h/2.0f + radius},
        {cx + w/2.0f - radius, cy - h/2.0f + radius},
        {cx + w/2.0f - radius, cy + h/2.0f - radius},
        {cx - w/2.0f + radius, cy + h/2.0f - radius}
    };
    for(int c=0;c<4;c++){
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(corners[c][0], corners[c][1]);
        for(int i=0;i<=segments;i++){
            float a = (float)i/segments * M_PI_2;
            float ax = cosf(a) * radius;
            float ay = sinf(a) * radius;
            if(c==0) glVertex2f(corners[c][0] - ax, corners[c][1] - ay);
            if(c==1) glVertex2f(corners[c][0] + ax, corners[c][1] - ay);
            if(c==2) glVertex2f(corners[c][0] + ax, corners[c][1] + ay);
            if(c==3) glVertex2f(corners[c][0] - ax, corners[c][1] + ay);
        }
        glEnd();
    }
}

void drawCircle(float cx, float cy, float r, int segments = 32) {
    glBegin(GL_TRIANGLE_FAN);
      glVertex2f(cx, cy);
      for(int i=0;i<=segments;i++){
          float a = (float)i/segments * 2.0f * M_PI;
          glVertex2f(cx + cosf(a)*r, cy + sinf(a)*r);
      }
    glEnd();
}

void drawText(const std::string &s, float x, float y, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for(char c: s) glutBitmapCharacter(font, c);
}

//////////////////////////////////////////////////////////////////////////
// Engine drawing and animation
//////////////////////////////////////////////////////////////////////////
void drawCombustionEffect(float cx, float cy, float size, int kind) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    int layers = 6;
    float timef = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    for(int i=0;i<layers;i++){
        float t = (float)i/layers;
        float r = size * (0.6f + t*0.8f);
        float alpha = 0.18f * (1.0f - t) + 0.02f;
        if(kind==0) glColor4f(0.95f, 0.95f, 0.55f, alpha); // faint yellow
        else if(kind==1) glColor4f(0.6f, 0.6f, 0.6f, alpha); // compression (darkish cloud)
        else if(kind==2) glColor4f(1.0f, 0.45f, 0.05f, alpha); // orange flame
        else glColor4f(0.6f, 0.6f, 0.6f, alpha); // exhaust grey
        float ox = (sinf(timef*1.5f + i*1.7f) * 6.0f * t) + (i*2.0f);
        float oy = (cosf(timef*1.1f + i*2.9f) * 8.0f * t) + (i*4.0f);
        drawCircle(cx + ox, cy + oy, r, 22);
    }
    glDisable(GL_BLEND);
}

void drawBlock() {
    glColor3f(0.58f, 0.58f, 0.58f);
    float blockW = spacing*(numCyl-1) + cylinderWidth + 80.0f;
    float blockH = 220.0f;
    drawFilledRect(blockLeftX + blockW/2.0f, blockTopY - blockH/2.0f + 20.0f, blockW, blockH);
}

void drawCylinderAndPiston(int idx, float pistonCenterY, int phaseKind) {
    float cx = blockLeftX + idx * spacing + cylinderWidth/2.0f + 20.0f;
    float cyTop = blockTopY - cylinderHeight/2.0f;

    float innerW = pistonWidth + 6.0f;
    float innerH = cylinderHeight - 10.0f;
    glColor3f(0.33f,0.33f,0.33f);
    drawFilledRect(cx, cyTop, innerW, innerH);

    glColor3f(0.18f,0.18f,0.18f);
    glBegin(GL_LINE_LOOP);
      float x0 = cx - innerW/2.0f;
      float y0 = cyTop - innerH/2.0f;
      glVertex2f(x0, y0);
      glVertex2f(x0 + innerW, y0);
      glVertex2f(x0 + innerW, y0 + innerH);
      glVertex2f(x0, y0 + innerH);
    glEnd();

    float pistonCX = cx;
    float pistonCY = pistonCenterY;
    glColor3f(0.15f,0.15f,0.15f);
    drawFilledRect(pistonCX, pistonCY, pistonWidth, pistonHeight);

    // piston top grooves
    glColor3f(0.05f, 0.05f, 0.05f);
    glBegin(GL_LINES);
      glVertex2f(pistonCX - pistonWidth/2.0f + 6.0f, pistonCY + pistonHeight/4.0f);
      glVertex2f(pistonCX + pistonWidth/2.0f - 6.0f, pistonCY + pistonHeight/4.0f);
      glVertex2f(pistonCX - pistonWidth/2.0f + 8.0f, pistonCY);
      glVertex2f(pistonCX + pistonWidth/2.0f - 8.0f, pistonCY);
    glEnd();

    float effectY = pistonCY + pistonHeight/2.0f + 12.0f;
    float effectSize = 18.0f + fabsf(sinf(crankAngle * M_PI/180.0f + idx * 0.9f) * 10.0f);
    drawCombustionEffect(pistonCX, effectY, effectSize, phaseKind);
}

float pistonPositionForCrank(float baseTopY, float angleDeg, float phaseOffsetDeg) {
    float a = (angleDeg + phaseOffsetDeg) * M_PI / 180.0f;
    float R = crankRadius;
    float disp = R - R * cosf(a);
    float smallCOR = (1.0f - cosf(a)) * (R*0.15f);
    float total = baseTopY - disp - smallCOR;
    return total;
}

int getPhaseKindForCylinder(float angleDeg, float phaseOffsetDeg) {
    float a = fmodf(angleDeg + phaseOffsetDeg, 720.0f);
    if (a < 0) a += 720.0f;
    if (a < 180.0f) return 0;
    else if (a < 360.0f) return 1;
    else if (a < 540.0f) return 2;
    else return 3;
}

void drawCrankshaft(float x, float y, float length) {
    glPushMatrix();
      glColor3f(0.35f, 0.35f, 0.35f);
      glBegin(GL_QUADS);
        glVertex2f(x - length/2.0f, y - 8.0f);
        glVertex2f(x + length/2.0f, y - 8.0f);
        glVertex2f(x + length/2.0f, y + 8.0f);
        glVertex2f(x - length/2.0f, y + 8.0f);
      glEnd();
    glPopMatrix();
}

//////////////////////////////////////////////////////////////////////////
// Landing page drawing
//////////////////////////////////////////////////////////////////////////
void drawLandingPage() {
    // Background slightly different
    glClearColor(0.96f, 0.96f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Title
    glColor3f(0.12f, 0.12f, 0.12f);
    std::string title = "4-Cylinder Engine Simulation";
    float tx = 40.0f;
    float ty = winH - 70.0f;
    drawText(title, tx, ty, GLUT_BITMAP_TIMES_ROMAN_24);

    // Subtitle / description
    glColor3f(0.18f, 0.18f, 0.18f);
    std::string desc = "Visualizes pistons, connecting rods, crankshaft and the 4-stroke cycle.";
    drawText(desc, tx, ty - 30.0f, GLUT_BITMAP_HELVETICA_18);
    std::string inst = "Click START or press Enter → Space toggles run/pause • 'f'/'s' adjust speed • 'm' return to menu";
    drawText(inst, tx, ty - 52.0f, GLUT_BITMAP_HELVETICA_12);

    // center preview box with faint schematic (draw block + cylinders small)
    float previewCX = winW - 360.0f;
    float previewCY = winH/2.0f;
    glColor3f(0.88f, 0.88f, 0.9f);
    drawRoundedRect(previewCX, previewCY, 520.0f, 240.0f, 12.0f);
    // small engine preview inside
    float previewBlockLeft = previewCX - 220.0f;
    float scaledSpacing = spacing * 0.6f;
    float scaledCylW = cylinderWidth * 0.6f;
    float previewTopY = previewCY + 20.0f;
    // block
    glColor3f(0.6f, 0.6f, 0.6f);
    float pbW = scaledSpacing*(numCyl-1) + scaledCylW + 80.0f;
    drawFilledRect(previewBlockLeft + pbW/2.0f, previewTopY - 60.0f, pbW, 160.0f);

    // small pistons
    for(int i=0;i<numCyl;i++){
        float cx = previewBlockLeft + i * scaledSpacing + scaledCylW/2.0f + 20.0f;
        float cy = previewTopY - 20.0f;
        glColor3f(0.33f,0.33f,0.33f);
        drawFilledRect(cx, cy, scaledCylW, cylinderHeight * 0.6f);
    }

    // Draw Start button
    btnX = 160.0f;
    btnY = winH/2.0f - 20.0f;
    float labelY = btnY - 6.0f;

    if(hoverBtn) glColor3f(0.10f, 0.55f, 0.92f);
    else glColor3f(0.12f, 0.47f, 0.82f);
    drawRoundedRect(btnX, btnY, btnW, btnH, 10.0f);

    // button text
    glColor3f(1.0f,1.0f,1.0f);
    std::string btext = "Start Simulation";
    float textX = btnX - (btext.size() * 8.0f)/2.0f + 8.0f;
    drawText(btext, textX, labelY, GLUT_BITMAP_HELVETICA_18);

    // footer small
    glColor3f(0.3f,0.3f,0.3f);
    drawText("Harshavardhan3015 - Engine Simulation (click Start)", 10.0f, 10.0f, GLUT_BITMAP_HELVETICA_12);

    glutSwapBuffers();
}

//////////////////////////////////////////////////////////////////////////
// Main display
//////////////////////////////////////////////////////////////////////////
void display() {
    if(appState == LANDING) {
        drawLandingPage();
        return;
    }

    // Animation state: clear
    glClearColor(0.92f, 0.92f, 0.94f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // === CENTER ENGINE ===
    glPushMatrix();
    float engineWidth  = spacing * (numCyl - 1) + cylinderWidth + 80.0f;
    float engineHeight = 260.0f; // approximate height that includes block + crank area
    float centerX = (winW - engineWidth) * 0.5f;
    float centerY = (winH - engineHeight) * 0.5f;
    glTranslatef(centerX, centerY, 0.0f);

    // Draw engine (uses blockLeftX, blockTopY, spacing, etc.)
    drawBlock();

    float baseTopY = blockTopY - cylinderHeight/2.0f + 40.0f;
    float crankY = blockTopY + 40.0f;
    float crankX = blockLeftX + (spacing*(numCyl-1))/2.0f + cylinderWidth/2.0f + 20.0f;
    drawCrankshaft(crankX, crankY, spacing*(numCyl-1) + 120.0f);

    for(int i=0;i<numCyl;i++){
        float cylinderX = blockLeftX + i * spacing + cylinderWidth/2.0f + 20.0f;
        float phaseOffset = i * 180.0f;
        float pistonCY = pistonPositionForCrank(baseTopY + 40.0f, crankAngle, phaseOffset);
        int phaseKind = getPhaseKindForCylinder(crankAngle, phaseOffset);
        drawCylinderAndPiston(i, pistonCY, phaseKind);

        float lateral = (i - (numCyl-1)/2.0f) * spacing;
        float a = (crankAngle + phaseOffset) * M_PI/180.0f;
        float crankPinX = crankX + lateral;
        float crankPinY = crankY - crankRadius * sinf(a);

        glColor3f(0.20f, 0.20f, 0.20f);
        drawCircle(crankPinX, crankPinY, 8.0f, 20);

        glLineWidth(6.0f);
        glColor3f(0.22f, 0.22f, 0.22f);
        glBegin(GL_LINES);
          glVertex2f(crankPinX, crankPinY);
          glVertex2f(cylinderX, pistonCY - pistonHeight/2.0f + 8.0f);
        glEnd();
        glLineWidth(1.0f);
    }

    for(int i=0;i<numCyl;i++){
        float lateral = (i - (numCyl-1)/2.0f) * spacing;
        float a = (crankAngle + i*180.0f) * M_PI/180.0f;
        float crankPinX = crankX + lateral;
        float crankPinY = crankY - crankRadius * sinf(a);
        glColor3f(0.28f, 0.28f, 0.28f);
        drawFilledRect(crankPinX - 6.0f, crankY, 28.0f, 10.0f);
        drawCircle(crankPinX, crankY, 12.0f, 24);
    }

    glPopMatrix(); // restore
    glutSwapBuffers();
}

//////////////////////////////////////////////////////////////////////////
// Input handling (keyboard + mouse)
//////////////////////////////////////////////////////////////////////////
void keyboard(unsigned char key, int x, int y) {
    if(appState == LANDING) {
        if(key == 13 || key == 10) { // Enter
            appState = ANIMATION;
            lastTime = glutGet(GLUT_ELAPSED_TIME);
            glutPostRedisplay();
            return;
        }
        if(key == 27) exit(0);
    } else {
        if(key == 27) exit(0);
        else if(key == ' ') {
            if (crankSpeedDegPerSec > 1.0f) crankSpeedDegPerSec = 0.0f;
            else crankSpeedDegPerSec = 120.0f;
        } else if (key == 'f') {
            crankSpeedDegPerSec += 30.0f;
        } else if (key == 's') {
            crankSpeedDegPerSec = std::max(0.0f, crankSpeedDegPerSec - 30.0f);
        } else if (key == 'm' || key == 'M') {
            appState = LANDING;
            glutPostRedisplay();
        }
    }
}

void passiveMouse(int x, int y) {
    // convert window coords (0,0 bottom-left in our projection) -> GLUT gives top-left
    float wx = x;
    float wy = winH - y;
    // check hover over button
    float bx = btnX - btnW/2.0f;
    float by = btnY - btnH/2.0f;
    bool hovered = (wx >= bx && wx <= bx + btnW && wy >= by && wy <= by + btnH);
    if(hovered != hoverBtn) {
        hoverBtn = hovered;
        glutPostRedisplay();
    }
}

void mouseClick(int button, int state, int x, int y) {
    if(appState == LANDING && button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        float wx = x;
        float wy = winH - y;
        float bx = btnX - btnW/2.0f;
        float by = btnY - btnH/2.0f;
        bool clicked = (wx >= bx && wx <= bx + btnW && wy >= by && wy <= by + btnH);
        if(clicked) {
            appState = ANIMATION;
            lastTime = glutGet(GLUT_ELAPSED_TIME);
            glutPostRedisplay();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// Window & timer
//////////////////////////////////////////////////////////////////////////
void reshape(int w, int h) {
    winW = w; winH = h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)w, 0.0, (double)h, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void timer(int value) {
    if(appState == ANIMATION) {
        int t = glutGet(GLUT_ELAPSED_TIME);
        if(lastTime==0) lastTime = t;
        int dt = t - lastTime;
        lastTime = t;
        crankAngle += crankSpeedDegPerSec * (dt / 1000.0f);
        if(crankAngle > 720.0f) crankAngle = fmodf(crankAngle, 720.0f);
        glutPostRedisplay();
    }
    glutTimerFunc(16, timer, 0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Engine Simulation - Landing Page");

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(passiveMouse);
    glutMouseFunc(mouseClick);

    lastTime = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timer, 0);

    glutMainLoop();
    return 0;
}
