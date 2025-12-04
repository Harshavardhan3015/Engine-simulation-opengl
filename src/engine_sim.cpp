// engine_sim.cpp
// Inline-4 IC engine animation with exhaust smoke + looping engine sound.
// C++ + FreeGLUT + MSVC. No text drawn on screen.

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include <GL/freeglut.h>
#include <cmath>
#include <cstdlib>
#include <ctime>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --------- Global state ---------
float crankAngle = 0.0f;      // 0–720 degrees (4-stroke cycle)
float rpm        = 250.0f;    // start fairly slow
int   lastTimeMs = 0;

// Camera
float camRotX = -35.0f;       // yaw
float camRotY = 20.0f;        // pitch
float camZoom = 20.0f;        // distance

// Inline-4 layout: cylinders along X
const int   NUM_CYL = 4;
const float cylX[NUM_CYL]     = { -3.0f, -1.0f,  1.0f,  3.0f };
const float cylPhase[NUM_CYL] = {   0.0f, 180.0f, 360.0f, 540.0f }; // simple 180° spacing

// --------- Smoke particle system ---------
struct SmokeParticle {
    bool  active;
    float x, y, z;
    float vx, vy, vz;
    float life;    // 0..1
};

const int MAX_SMOKE = 400;
SmokeParticle smoke[MAX_SMOKE];

// Spawn a single smoke particle at world position (x,y,z)
void spawnSmoke(float x, float y, float z)
{
    for (int i = 0; i < MAX_SMOKE; ++i) {
        if (!smoke[i].active) {
            smoke[i].active = true;
            smoke[i].x = x;
            smoke[i].y = y;
            smoke[i].z = z;

            // Small random sideways drift, rising up
            float angle = ((float)std::rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float speed = 0.3f + 0.2f * ((float)std::rand() / RAND_MAX);

            smoke[i].vx = std::cos(angle) * speed * 0.1f;
            smoke[i].vz = std::sin(angle) * speed * 0.1f;
            smoke[i].vy = 0.5f + 0.3f * ((float)std::rand() / RAND_MAX);

            smoke[i].life = 1.0f; // start fully visible
            return;
        }
    }
}

// Update all smoke particles
void updateSmoke(float dtSeconds)
{
    for (int i = 0; i < MAX_SMOKE; ++i) {
        if (!smoke[i].active) continue;

        smoke[i].life -= dtSeconds * 0.35f;  // fade over time
        if (smoke[i].life <= 0.0f) {
            smoke[i].active = false;
            continue;
        }

        // simple motion: rise + drift
        smoke[i].x += smoke[i].vx * dtSeconds;
        smoke[i].y += smoke[i].vy * dtSeconds;
        smoke[i].z += smoke[i].vz * dtSeconds;

        // slightly slow down horizontal drift
        smoke[i].vx *= (1.0f - 0.3f * dtSeconds);
        smoke[i].vz *= (1.0f - 0.3f * dtSeconds);
    }
}

// Draw smoke cubes
void drawSmoke()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < MAX_SMOKE; ++i) {
        if (!smoke[i].active) continue;

        float a = smoke[i].life * 0.6f;  // fade out
        float size = 0.25f + (1.0f - smoke[i].life) * 0.35f;  // expand over time

        glPushMatrix();
        glTranslatef(smoke[i].x, smoke[i].y, smoke[i].z);
        glColor4f(0.75f, 0.75f, 0.75f, a);
        glBegin(GL_QUADS);
        float s = size * 0.5f;

        // Simple cube (no normals needed for smoke)
        glVertex3f(-s, -s,  s); glVertex3f( s, -s,  s);
        glVertex3f( s,  s,  s); glVertex3f(-s,  s,  s);

        glVertex3f(-s, -s, -s); glVertex3f(-s,  s, -s);
        glVertex3f( s,  s, -s); glVertex3f( s, -s, -s);

        glVertex3f(-s, -s, -s); glVertex3f(-s, -s,  s);
        glVertex3f(-s,  s,  s); glVertex3f(-s,  s, -s);

        glVertex3f( s, -s, -s); glVertex3f( s,  s, -s);
        glVertex3f( s,  s,  s); glVertex3f( s, -s,  s);

        glVertex3f(-s,  s, -s); glVertex3f(-s,  s,  s);
        glVertex3f( s,  s,  s); glVertex3f( s,  s, -s);

        glVertex3f(-s, -s, -s); glVertex3f( s, -s, -s);
        glVertex3f( s, -s,  s); glVertex3f(-s, -s,  s);

        glEnd();
        glPopMatrix();
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ---------- Helper draw functions ----------

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
    glNormal3f(1,0,0);       // facing +X (we rotate as needed)
    glVertex3f(0,0,0);
    for (int i = 0; i <= slices; ++i) {
        float th = (float)i / slices * 2.0f * (float)M_PI;
        float y = radius * std::cos(th);
        float z = radius * std::sin(th);
        glVertex3f(0,y,z);
    }
    glEnd();
}

// ---------- Engine math ----------

float getPistonHeight(float angleDeg)
{
    const float R = 0.75f;  // crank radius
    const float L = 2.4f;   // rod length

    float th = angleDeg * (float)M_PI / 180.0f;
    float s  = std::sin(th);
    float c  = std::cos(th);

    float y = R * c + std::sqrt(L*L - R*R*s*s);

    float minY = L - R;
    float maxY = L + R;
    float t = (y - minY) / (maxY - minY);   // 0–1

    return 1.5f + t * 3.2f; // world coordinates
}

void getValveStates(float angleDeg, float &intakeLift, float &exhaustLift)
{
    float a = std::fmod(angleDeg, 720.0f);
    intakeLift  = 0.0f;
    exhaustLift = 0.0f;

    // very simple: intake open 0–180, exhaust open 540–720
    if (a < 180.0f)    intakeLift  = 1.0f;
    if (a >= 540.0f)   exhaustLift = 1.0f;
}

// ---------- Lighting ----------

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

// ---------- Display ----------

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // camera transform
    glTranslatef(0.0f, -2.5f, -camZoom);
    glRotatef(camRotY, 1, 0, 0);
    glRotatef(camRotX, 0, 1, 0);

    setupLighting();

    // Ground plane
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

    // Engine block / crankcase (green)
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    glColor3f(0.10f, 0.55f, 0.10f);
    drawBox(9.2f, 1.2f, 2.6f);
    glPopMatrix();

    // Crankshaft main shaft
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    glColor3f(0.05f, 0.45f, 0.05f);
    drawBox(9.2f, 0.35f, 0.7f);
    glPopMatrix();

    const float crankY = 0.8f;
    const float crankR = 0.7f;

    // Flywheel (red disc on the left)
    glPushMatrix();
    glTranslatef(cylX[0] - 1.8f, crankY, 0.0f);
    glRotatef(crankAngle, 1,0,0);
    glColor3f(0.8f, 0.15f, 0.15f);
    drawDisk(1.8f, 40);
    glPopMatrix();

    // Timing pulleys & belt on right
    // Top pulley
    glPushMatrix();
    glTranslatef(cylX[NUM_CYL-1] + 2.0f, 5.3f, 1.5f);
    glRotatef(-crankAngle * 0.5f, 1,0,0);
    glColor3f(0.85f, 0.15f, 0.15f);
    drawDisk(0.9f, 30);
    glPopMatrix();

    // Bottom pulley
    glPushMatrix();
    glTranslatef(cylX[NUM_CYL-1] + 2.0f, crankY, 1.5f);
    glRotatef(crankAngle, 1,0,0);
    glColor3f(0.85f, 0.15f, 0.15f);
    drawDisk(1.0f, 30);
    glPopMatrix();

    // Belt frame (blue)
    glPushMatrix();
    glTranslatef(cylX[NUM_CYL-1] + 2.0f, 3.0f, 1.5f);
    glColor3f(0.10f, 0.55f, 0.95f);
    drawBox(0.25f, 4.7f, 0.2f);
    glPopMatrix();

    // Belt (dark line)
    glDisable(GL_LIGHTING);
    glColor3f(0.05f, 0.05f, 0.05f);
    glBegin(GL_LINE_STRIP);
    glVertex3f(cylX[NUM_CYL-1] + 2.0f, crankY,  1.5f);
    glVertex3f(cylX[NUM_CYL-1] + 2.0f, 5.3f,    1.5f);
    glEnd();
    glEnable(GL_LIGHTING);

    // Yellow rocker / cam housing bar on top
    glPushMatrix();
    glTranslatef(0.0f, 5.5f, 0.0f);
    glColor3f(0.95f, 0.80f, 0.20f);
    drawBox(9.2f, 0.7f, 1.6f);
    glPopMatrix();

    // Cylinders, pistons, rods, flames, valves for all 4
    for (int i = 0; i < NUM_CYL; ++i)
    {
        float cx = cylX[i];
        float localAngle = crankAngle + cylPhase[i];

        // Transparent cylinder sleeve
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

        // Piston position
        float pistonY = getPistonHeight(localAngle);

        // Piston (gold)
        glPushMatrix();
        glTranslatef(cx, pistonY, 0.0f);
        glColor3f(0.96f, 0.82f, 0.20f);
        drawBox(1.4f, 0.7f, 1.4f);
        glPopMatrix();

        // Crank web disc (green)
        float th = localAngle * (float)M_PI / 180.0f;
        glPushMatrix();
        glTranslatef(cx, crankY, 0.0f);
        glRotatef(localAngle, 1,0,0);
        glColor3f(0.05f, 0.65f, 0.10f);
        drawDisk(0.6f, 28);
        glPopMatrix();

        // Connecting rod
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

            glColor3f(0.05f, 0.85f, 0.40f);   // bluish green rod
            drawBox(0.25f, length, 0.25f);
            glPopMatrix();
        }

        // Valves above cylinder
        float intakeLift, exhaustLift;
        getValveStates(localAngle, intakeLift, exhaustLift);

        // Intake valve (greenish)
        glPushMatrix();
        glTranslatef(cx - 0.35f, 5.7f + intakeLift*0.6f, 0.35f);
        glColor3f(0.15f, 1.0f, 0.25f);
        drawBox(0.18f, 0.9f, 0.18f);
        glPopMatrix();

        // Exhaust valve (red)
        glPushMatrix();
        glTranslatef(cx + 0.35f, 5.7f + exhaustLift*0.6f, -0.35f);
        glColor3f(0.95f, 0.15f, 0.15f);
        drawBox(0.18f, 0.9f, 0.18f);
        glPopMatrix();

        // Combustion flame (only during power stroke)
        float phase = std::fmod(localAngle, 720.0f);
        if (phase >= 360.0f && phase < 540.0f)
        {
            float t = (phase - 360.0f) / 180.0f;           // 0..1
            float flicker = 0.35f + 0.25f * std::sin(t * 6.0f);

            glPushMatrix();
            glTranslatef(cx, 4.6f, 0.0f);
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1.0f, 0.46f, 0.08f, flicker);       // hot orange flame
            drawBox(1.25f, 1.0f, 1.25f);
            glDisable(GL_BLEND);
            glEnable(GL_LIGHTING);
            glPopMatrix();
        }
    }

    // Draw smoke last so it overlays engine parts slightly
    drawSmoke();

    glutSwapBuffers();
}

// ---------- Animation ----------

void idle()
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (lastTimeMs == 0) lastTimeMs = now;

    float dtMs      = (float)(now - lastTimeMs);
    float dtSeconds = dtMs / 1000.0f;
    lastTimeMs = now;

    // rpm -> degrees per millisecond
    float degPerMs = rpm * 360.0f / 60000.0f;
    crankAngle += degPerMs * dtMs;
    if (crankAngle >= 720.0f) crankAngle -= 720.0f;

    // Spawn smoke from cylinders in exhaust stroke
    for (int i = 0; i < NUM_CYL; ++i) {
        float localAngle = crankAngle + cylPhase[i];
        float phase = std::fmod(localAngle, 720.0f);
        if (phase >= 540.0f && phase < 720.0f) {
            // small probability each frame depending on rpm
            float spawnChance = 0.03f + (rpm / 4000.0f) * 0.05f; // more rpm = more smoke
            float r = (float)std::rand() / RAND_MAX;
            if (r < spawnChance) {
                // exhaust outlet near top on exhaust side
                float exX = cylX[i] + 0.7f;
                float exY = 5.9f;
                float exZ = -0.5f;
                spawnSmoke(exX, exY, exZ);
            }
        }
    }

    // Update smoke motion
    updateSmoke(dtSeconds);

    glutPostRedisplay();
}

// ---------- Reshape ----------

void reshape(int w, int h)
{
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)w / (float)h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

// ---------- Controls ----------

void keyboard(unsigned char key, int, int)
{
    switch (key)
    {
    case 27:  // ESC
        std::exit(0);
        break;
    case 'w':
        camRotY += 2.0f; break;
    case 's':
        camRotY -= 2.0f; break;
    case 'a':
        camRotX -= 2.0f; break;
    case 'd':
        camRotX += 2.0f; break;
    case '+':
    case '=':
        camZoom -= 0.5f;
        if (camZoom < 12.0f) camZoom = 12.0f;
        break;
    case '-':
        camZoom += 0.5f;
        if (camZoom > 28.0f) camZoom = 28.0f;
        break;
    case 'z':           // slower
        rpm -= 40.0f;
        if (rpm < 80.0f) rpm = 80.0f;
        break;
    case 'x':           // faster
        rpm += 40.0f;
        if (rpm > 3500.0f) rpm = 3500.0f;
        break;
    case '1':
        rpm = 150.0f; break;  // demo speed
    case '2':
        rpm = 400.0f; break;  // medium
    case '3':
        rpm = 900.0f; break;  // higher idle
    }
}

// ---------- Main ----------

int main(int argc, char** argv)
{
    std::srand((unsigned int)std::time(nullptr)); // random for smoke

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Inline-4 IC Engine Simulation");

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.03f, 0.03f, 0.05f, 1.0f);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyboard);

    // Start looping engine sound.
    // Make sure "engine_loop.wav" is in the same folder as the EXE,
    // or replace with a full absolute path if you like.
    PlaySound(TEXT("engine_loop.wav"), NULL,
              SND_ASYNC | SND_LOOP | SND_FILENAME);

    glutMainLoop();
    return 0;
}
