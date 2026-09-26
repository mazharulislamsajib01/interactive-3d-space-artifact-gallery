#define NOMINMAX

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <GL/glu.h>
#include <GL/glut.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

const float PI = 3.14159265358979323846f;
const int TIMER_INTERVAL_MS = 16;
const int ARTIFACT_COUNT = 3;

enum TextureId
{
    TEX_WOOD = 0,
    TEX_FLOOR,
    TEX_WALL,
    TEX_COUNT
};

struct Camera
{
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
};

struct ArtifactTransform
{
    float x;
    float y;
    float z;
    float rotationY;
    float scale;
};

Camera camera = {0.0f, 4.2f, 14.0f, -90.0f, -5.0f};

ArtifactTransform artifactTransforms[ARTIFACT_COUNT] =
{
    {0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f, 1.0f}
};

GLuint textures[TEX_COUNT] = {0, 0, 0};
GLUquadric *sharedQuadric = 0;

int windowWidth = 1280;
int windowHeight = 720;
int selectedArtifact = 0;

bool animationRunning = true;
bool nightMode = true;
bool cometVisible = true;
bool showHelp = true;
bool lightEnabled[3] = {true, true, true};
bool doorTargetOpen = false;

float doorAngle = 0.0f;
float orreryRotation = 0.0f;
float planetOrbit[3] = {0.0f, 120.0f, 235.0f};
float planetSpin[3] = {0.0f, 0.0f, 0.0f};
float satelliteRotation = 0.0f;
float cometPosition = 0.0f;

/* ------------------------------------------------------------------------- */
/* Small utility functions                                                   */
/* ------------------------------------------------------------------------- */

float clampFloat(float value, float minimum, float maximum)
{
    if (value < minimum)
        return minimum;

    if (value > maximum)
        return maximum;

    return value;
}

float degreesToRadians(float degrees)
{
    return degrees * PI / 180.0f;
}

unsigned char byteClamp(int value)
{
    if (value < 0)
        value = 0;

    if (value > 255)
        value = 255;

    return static_cast<unsigned char>(value);
}

void setMaterial(float r, float g, float b,
                 float specularStrength, float shininess)
{
    GLfloat ambient[] =
    {
        r * 0.24f,
        g * 0.24f,
        b * 0.24f,
        1.0f
    };

    GLfloat diffuse[] =
    {
        r,
        g,
        b,
        1.0f
    };

    GLfloat specular[] =
    {
        specularStrength,
        specularStrength,
        specularStrength,
        1.0f
    };

    GLfloat emission[] =
    {
        0.0f,
        0.0f,
        0.0f,
        1.0f
    };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
    glMaterialf(
        GL_FRONT_AND_BACK,
        GL_SHININESS,
        clampFloat(shininess, 0.0f, 128.0f)
    );
}

void setEmissiveMaterial(float r, float g, float b,
                         float emissionStrength)
{
    setMaterial(r, g, b, 0.75f, 70.0f);

    GLfloat emission[] =
    {
        r * emissionStrength,
        g * emissionStrength,
        b * emissionStrength,
        1.0f
    };

    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
}

void resetMaterialEmission()
{
    GLfloat emission[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
}

/* ------------------------------------------------------------------------- */
/* Procedural textures                                                       */
/* ------------------------------------------------------------------------- */

void createProceduralTextures()
{
    const int SIZE = 64;

    unsigned char wood[SIZE * SIZE * 3];
    unsigned char floorTexture[SIZE * SIZE * 3];
    unsigned char wallTexture[SIZE * SIZE * 3];

    for (int y = 0; y < SIZE; ++y)
    {
        for (int x = 0; x < SIZE; ++x)
        {
            int index = (y * SIZE + x) * 3;

            /*
               Wood texture: layered sine waves produce grain, while the
               small modular term gives subtle knots and irregularity.
            */
            float grainWave =
                std::sin(x * 0.52f + std::sin(y * 0.20f) * 2.2f);

            int grain = static_cast<int>(grainWave * 21.0f);
            int knot =
                ((x - 17) * (x - 17) +
                 (y - 33) * (y - 33)) % 37;

            wood[index + 0] = byteClamp(125 + grain + knot / 6);
            wood[index + 1] = byteClamp(68 + grain / 2);
            wood[index + 2] = byteClamp(28 + grain / 4);

            /*
               Floor texture: alternating tiles with sinusoidal marble veins.
            */
            int tile = ((x / 8) + (y / 8)) % 2;

            float veinWave =
                std::sin(x * 0.29f + y * 0.21f) +
                std::sin(y * 0.43f);

            int vein = static_cast<int>(veinWave * 8.0f);
            int floorBase = tile ? 185 : 215;

            floorTexture[index + 0] = byteClamp(floorBase + vein);
            floorTexture[index + 1] = byteClamp(floorBase + vein + 5);
            floorTexture[index + 2] = byteClamp(floorBase + vein + 12);

            /*
               Wall texture: staggered rectangular panels with dark mortar.
            */
            bool horizontalMortar = (y % 16) < 2;
            int shiftedX = x + ((y / 16) % 2) * 8;
            bool verticalMortar = (shiftedX % 16) < 2;
            bool mortar = horizontalMortar || verticalMortar;

            if (mortar)
            {
                wallTexture[index + 0] = 78;
                wallTexture[index + 1] = 82;
                wallTexture[index + 2] = 90;
            }
            else
            {
                int variation = ((x * 13 + y * 7) % 18) - 9;

                wallTexture[index + 0] = byteClamp(104 + variation);
                wallTexture[index + 1] = byteClamp(116 + variation);
                wallTexture[index + 2] = byteClamp(132 + variation);
            }
        }
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(TEX_COUNT, textures);

    glBindTexture(GL_TEXTURE_2D, textures[TEX_WOOD]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        SIZE,
        SIZE,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        wood
    );

    glBindTexture(GL_TEXTURE_2D, textures[TEX_FLOOR]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        SIZE,
        SIZE,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        floorTexture
    );

    glBindTexture(GL_TEXTURE_2D, textures[TEX_WALL]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        SIZE,
        SIZE,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        wallTexture
    );

    glBindTexture(GL_TEXTURE_2D, 0);
}

/* ------------------------------------------------------------------------- */
/* Reusable primitive drawing functions                                      */
/* ------------------------------------------------------------------------- */

void drawCube(float width, float height, float depth)
{
    glPushMatrix();
    glScalef(width, height, depth);
    glutSolidCube(1.0);
    glPopMatrix();
}

void drawTexturedBox(float width, float height, float depth,
                     GLuint texture, float repeatS, float repeatT)
{
    float x = width * 0.5f;
    float y = height * 0.5f;
    float z = depth * 0.5f;

    /*
       All texture state changes are local to this function. The previous
       state is restored by glPopAttrib().
    */
    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);

    /* Front face */
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-x, -y, z);

    glTexCoord2f(repeatS, 0.0f);
    glVertex3f(x, -y, z);

    glTexCoord2f(repeatS, repeatT);
    glVertex3f(x, y, z);

    glTexCoord2f(0.0f, repeatT);
    glVertex3f(-x, y, z);

    /* Back face */
    glNormal3f(0.0f, 0.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(x, -y, -z);

    glTexCoord2f(repeatS, 0.0f);
    glVertex3f(-x, -y, -z);

    glTexCoord2f(repeatS, repeatT);
    glVertex3f(-x, y, -z);

    glTexCoord2f(0.0f, repeatT);
    glVertex3f(x, y, -z);

    /* Right face */
    glNormal3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(x, -y, z);

    glTexCoord2f(repeatS, 0.0f);
    glVertex3f(x, -y, -z);

    glTexCoord2f(repeatS, repeatT);
    glVertex3f(x, y, -z);

    glTexCoord2f(0.0f, repeatT);
    glVertex3f(x, y, z);

    /* Left face */
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-x, -y, -z);

    glTexCoord2f(repeatS, 0.0f);
    glVertex3f(-x, -y, z);

    glTexCoord2f(repeatS, repeatT);
    glVertex3f(-x, y, z);

    glTexCoord2f(0.0f, repeatT);
    glVertex3f(-x, y, -z);

    /* Top face */
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-x, y, z);

    glTexCoord2f(repeatS, 0.0f);
    glVertex3f(x, y, z);

    glTexCoord2f(repeatS, repeatT);
    glVertex3f(x, y, -z);

    glTexCoord2f(0.0f, repeatT);
    glVertex3f(-x, y, -z);

    /* Bottom face */
    glNormal3f(0.0f, -1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-x, -y, -z);

    glTexCoord2f(repeatS, 0.0f);
    glVertex3f(x, -y, -z);

    glTexCoord2f(repeatS, repeatT);
    glVertex3f(x, -y, z);

    glTexCoord2f(0.0f, repeatT);
    glVertex3f(-x, -y, z);

    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glPopAttrib();
}

void drawCylinderZ(float radius, float height, int slices)
{
    /*
       GLU cylinders point along local +Z. This helper also supplies both
       end caps and flips the bottom disk so its normal points outward.
    */
    gluQuadricNormals(sharedQuadric, GLU_SMOOTH);
    gluQuadricOrientation(sharedQuadric, GLU_OUTSIDE);

    gluCylinder(
        sharedQuadric,
        radius,
        radius,
        height,
        slices,
        1
    );

    glPushMatrix();
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    gluDisk(sharedQuadric, 0.0, radius, slices, 1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, height);
    gluDisk(sharedQuadric, 0.0, radius, slices, 1);
    glPopMatrix();
}

void drawCylinderY(float radius, float height, int slices)
{
    glPushMatrix();

    glTranslatef(0.0f, -height * 0.5f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);

    drawCylinderZ(radius, height, slices);

    glPopMatrix();
}

void drawCylinderX(float radius, float length, int slices)
{
    glPushMatrix();

    glTranslatef(-length * 0.5f, 0.0f, 0.0f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);

    drawCylinderZ(radius, length, slices);

    glPopMatrix();
}

void drawConeY(float radius, float height, int slices)
{
    glPushMatrix();

    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glutSolidCone(radius, height, slices, 8);

    glPopMatrix();
}

void drawCylinderBetween(float x1, float y1, float z1,
                         float x2, float y2, float z2,
                         float radius, int slices)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;

    float length = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (length < 0.0001f)
        return;

    /*
       The primitive initially points along +Z. The rotation axis is the
       cross product between +Z and the required direction.
    */
    float cosine = clampFloat(dz / length, -1.0f, 1.0f);
    float angle = std::acos(cosine) * 180.0f / PI;

    float axisX = -dy;
    float axisY = dx;
    float axisLength = std::sqrt(axisX * axisX + axisY * axisY);

    glPushMatrix();

    glTranslatef(x1, y1, z1);

    if (axisLength > 0.0001f)
    {
        glRotatef(
            angle,
            axisX / axisLength,
            axisY / axisLength,
            0.0f
        );
    }
    else if (dz < 0.0f)
    {
        glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    }

    drawCylinderZ(radius, length, slices);

    glPopMatrix();
}

void drawHorizontalTorus(float tubeRadius, float ringRadius)
{
    /*
       A GLUT torus normally lies in the XY plane. Rotating it 90 degrees
       around X places it horizontally in the XZ plane.
    */
    glPushMatrix();
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glutSolidTorus(tubeRadius, ringRadius, 12, 48);
    glPopMatrix();
}

/* ------------------------------------------------------------------------- */
/* Exterior environment                                                      */
/* ------------------------------------------------------------------------- */

void drawExterior()
{
    /*
       Sky, stars, and distant silhouettes are intentionally unlit so they
       behave like a self-luminous background. The state is restored before
       the lit museum is rendered.
    */
    glPushAttrib(
        GL_ENABLE_BIT |
        GL_CURRENT_BIT |
        GL_POINT_BIT |
        GL_LINE_BIT |
        GL_LIGHTING_BIT |
        GL_TEXTURE_BIT
    );

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    if (nightMode)
        glColor3f(0.018f, 0.025f, 0.085f);
    else
        glColor3f(0.25f, 0.62f, 0.90f);

    /*
       Three physical sky planes are placed outside the museum. They are
       world-space geometry rather than a screen overlay.
    */
    glBegin(GL_QUADS);

    /* Left exterior sky plane */
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-32.0f, -1.0f, -25.0f);
    glVertex3f(-32.0f, 24.0f, -25.0f);
    glVertex3f(-32.0f, 24.0f, 22.0f);
    glVertex3f(-32.0f, -1.0f, 22.0f);

    /* Right exterior sky plane */
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(32.0f, -1.0f, 22.0f);
    glVertex3f(32.0f, 24.0f, 22.0f);
    glVertex3f(32.0f, 24.0f, -25.0f);
    glVertex3f(32.0f, -1.0f, -25.0f);

    /* Rear exterior sky plane */
    glNormal3f(0.0f, 0.0f, 1.0f);
   glVertex3f(-32.0f, -1.0f, -42.0f);
    glVertex3f(32.0f, -1.0f, -42.0f);
    glVertex3f(32.0f, 24.0f, -42.0f);
    glVertex3f(-32.0f, 24.0f, -42.0f);

    glEnd();

