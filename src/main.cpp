

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

    /* Exterior ground */
    if (nightMode)
        glColor3f(0.025f, 0.035f, 0.050f);
    else
        glColor3f(0.18f, 0.25f, 0.30f);

    glBegin(GL_QUADS);

    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-40.0f, -0.15f, 28.0f);
    glVertex3f(40.0f, -0.15f, 28.0f);
    glVertex3f(40.0f, -0.15f, -45.0f);
    glVertex3f(-40.0f, -0.15f, -45.0f);

    glEnd();

    if (nightMode)
    {
        glPointSize(2.2f);
        glColor3f(0.92f, 0.95f, 1.0f);

        glBegin(GL_POINTS);

        for (int i = 0; i < 60; ++i)
        {
            float starY =
                3.0f +
                static_cast<float>((i * 37) % 175) / 10.0f;

            float starZ =
                -22.0f +
                static_cast<float>((i * 53) % 410) / 10.0f;

            glVertex3f(-31.8f, starY, starZ);

            starY =
                3.0f +
                static_cast<float>((i * 43) % 175) / 10.0f;

            starZ =
                -22.0f +
                static_cast<float>((i * 29) % 410) / 10.0f;

            glVertex3f(31.8f, starY, starZ);

            float starX =
                -30.0f +
                static_cast<float>((i * 47) % 600) / 10.0f;

            starY =
                3.0f +
                static_cast<float>((i * 31) % 175) / 10.0f;

            glVertex3f(starX, starY, -41.8f);
        }

        glEnd();

        /* Moon behind the left window */
        glColor3f(0.92f, 0.92f, 0.75f);

        glPushMatrix();
        glTranslatef(-27.5f, 8.0f, -6.0f);
        glutSolidSphere(2.0, 28, 20);
        glPopMatrix();
    }
    else
    {
        /* Day-mode Sun in the same exterior scene */
        glColor3f(1.0f, 0.83f, 0.25f);

        glPushMatrix();
        glTranslatef(-27.5f, 10.5f, -6.0f);
        glutSolidSphere(1.7, 28, 20);
        glPopMatrix();
    }

    /* Mountain silhouettes behind the left-side window */
    if (nightMode)
        glColor3f(0.06f, 0.075f, 0.11f);
    else
        glColor3f(0.20f, 0.29f, 0.35f);

    glBegin(GL_TRIANGLES);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-27.0f, 0.0f, -19.0f);
    glVertex3f(-27.0f, 7.0f, -13.0f);
    glVertex3f(-27.0f, 0.0f, -7.0f);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-27.1f, 0.0f, -11.0f);
    glVertex3f(-27.1f, 5.0f, -4.0f);
    glVertex3f(-27.1f, 0.0f, 3.0f);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-27.2f, 0.0f, -3.0f);
    glVertex3f(-27.2f, 6.2f, 5.0f);
    glVertex3f(-27.2f, 0.0f, 13.0f);

    glEnd();

    /* Distant city outside the right-side window */
    if (nightMode)
        glColor3f(0.07f, 0.09f, 0.14f);
    else
        glColor3f(0.27f, 0.34f, 0.39f);

    for (int i = 0; i < 10; ++i)
    {
        float buildingHeight =
            2.0f + static_cast<float>((i * 7) % 5);

        float z = -16.0f + i * 3.0f;

        glPushMatrix();

        glTranslatef(26.5f, buildingHeight * 0.5f, z);
        glScalef(1.2f, buildingHeight, 2.0f);
        glutSolidCube(1.0);

        glPopMatrix();
    }

    if (nightMode)
    {
        glPointSize(3.0f);
        glColor3f(1.0f, 0.76f, 0.18f);

        glBegin(GL_POINTS);

        for (int i = 0; i < 18; ++i)
        {
            float z =
                -16.0f +
                static_cast<float>(i % 10) * 3.0f;

            float y =
                1.0f +
                static_cast<float>((i * 5) % 4);

            glVertex3f(25.85f, y, z);
        }

        glEnd();
    }

    /* Optional moving comet behind the left window */
    if (cometVisible && nightMode)
    {
        float cometZ = -18.0f + cometPosition;

        float cometY =
            10.5f +
            std::sin(cometPosition * 0.22f) * 1.2f;

        glLineWidth(4.0f);

        glBegin(GL_LINES);

        glColor3f(0.35f, 0.58f, 1.0f);
        glVertex3f(-27.3f, cometY - 0.5f, cometZ - 4.5f);

        glColor3f(1.0f, 1.0f, 1.0f);
        glVertex3f(-27.3f, cometY, cometZ);

        glEnd();

        glColor3f(1.0f, 0.95f, 0.75f);

        glPushMatrix();
        glTranslatef(-27.3f, cometY, cometZ);
        glutSolidSphere(0.22, 16, 12);
        glPopMatrix();
    }

    glPopAttrib();
}

/* ------------------------------------------------------------------------- */
/* Museum architecture and furniture                                         */
/* ------------------------------------------------------------------------- */

void drawWindow(float x, float zCenter, float windowSpan)
{
    float insideX =
        (x < 0.0f) ? x + 0.21f : x - 0.21f;

    setMaterial(0.20f, 0.24f, 0.29f, 0.85f, 75.0f);

    glPushMatrix();
    glTranslatef(
        insideX,
        6.0f,
        zCenter - windowSpan * 0.5f
    );
    drawCube(0.34f, 6.3f, 0.28f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(
        insideX,
        6.0f,
        zCenter + windowSpan * 0.5f
    );
    drawCube(0.34f, 6.3f, 0.28f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(insideX, 3.0f, zCenter);
    drawCube(0.34f, 0.28f, windowSpan + 0.3f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(insideX, 9.0f, zCenter);
    drawCube(0.34f, 0.28f, windowSpan + 0.3f);
    glPopMatrix();

    /* Horizontal and vertical mullions */
    glPushMatrix();
    glTranslatef(insideX, 6.0f, zCenter);
    drawCube(0.34f, 0.18f, windowSpan);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(insideX, 6.0f, zCenter);
    drawCube(0.34f, 6.0f, 0.16f);
    glPopMatrix();
}

void drawSideWallWithWindow(float x,
                            float windowCenterZ,
                            float windowSpan)
{
    const float roomMinZ = -20.0f;
    const float roomMaxZ = 16.0f;

    float windowStart =
        windowCenterZ - windowSpan * 0.5f;

    float windowEnd =
        windowCenterZ + windowSpan * 0.5f;

    float beforeDepth = windowStart - roomMinZ;
    float afterDepth = roomMaxZ - windowEnd;

    setMaterial(0.68f, 0.72f, 0.80f, 0.20f, 20.0f);

    /*
       Lower and upper strips leave a real opening from y=3 to y=9.
    */
    glPushMatrix();
    glTranslatef(x, 1.5f, -2.0f);
    drawTexturedBox(
        0.4f,
        3.0f,
        36.0f,
        textures[TEX_WALL],
        8.0f,
        1.0f
    );
    glPopMatrix();

    glPushMatrix();
    glTranslatef(x, 10.5f, -2.0f);
    drawTexturedBox(
        0.4f,
        3.0f,
        36.0f,
        textures[TEX_WALL],
        8.0f,
        1.0f
    );
    glPopMatrix();

    if (beforeDepth > 0.0f)
    {
        glPushMatrix();

        glTranslatef(
            x,
            6.0f,
            (roomMinZ + windowStart) * 0.5f
        );

        drawTexturedBox(
            0.4f,
            6.0f,
            beforeDepth,
            textures[TEX_WALL],
            2.0f,
            2.0f
        );

        glPopMatrix();
    }

    if (afterDepth > 0.0f)
    {
        glPushMatrix();

        glTranslatef(
            x,
            6.0f,
            (windowEnd + roomMaxZ) * 0.5f
        );

        drawTexturedBox(
            0.4f,
            6.0f,
            afterDepth,
            textures[TEX_WALL],
            4.0f,
            2.0f
        );

        glPopMatrix();
    }

    drawWindow(x, windowCenterZ, windowSpan);
}

void drawDoor()
{
    const float doorWidth = 5.4f;
    const float doorHeight = 6.7f;
    const float hingeX = -2.7f;

    /* Door frame */
    setMaterial(0.19f, 0.13f, 0.08f, 0.35f, 25.0f);

    glPushMatrix();
    glTranslatef(-2.95f, 3.5f, 15.75f);
    drawCube(0.42f, 7.0f, 0.48f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(2.95f, 3.5f, 15.75f);
    drawCube(0.42f, 7.0f, 0.48f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 7.0f, 15.75f);
    drawCube(6.3f, 0.42f, 0.48f);
    glPopMatrix();

    /*
       Correct hinge transformation:
       translate to hinge -> rotate -> translate to panel center.
    */
    glPushMatrix();

    glTranslatef(hingeX, doorHeight * 0.5f, 15.70f);
    glRotatef(doorAngle, 0.0f, 1.0f, 0.0f);
    glTranslatef(doorWidth * 0.5f, 0.0f, 0.0f);

    setMaterial(0.70f, 0.45f, 0.18f, 0.30f, 28.0f);

    drawTexturedBox(
        doorWidth,
        doorHeight,
        0.24f,
        textures[TEX_WOOD],
        3.0f,
        3.0f
    );

    /* Door handle */
    setMaterial(0.75f, 0.50f, 0.14f, 0.85f, 70.0f);

    glPushMatrix();
    glTranslatef(1.9f, 0.0f, -0.22f);
    glutSolidSphere(0.15, 18, 12);
    glPopMatrix();

    glPopMatrix();
}

void drawReceptionDesk()
{
    setMaterial(0.62f, 0.34f, 0.13f, 0.28f, 25.0f);

    glPushMatrix();
    glTranslatef(0.0f, 1.1f, 0.0f);
    drawTexturedBox(
        5.0f,
        2.2f,
        1.5f,
        textures[TEX_WOOD],
        4.0f,
        2.0f
    );
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 2.35f, -0.1f);
    drawTexturedBox(
        5.5f,
        0.30f,
        1.9f,
        textures[TEX_WOOD],
        4.0f,
        1.0f
    );
    glPopMatrix();

    /* Information monitor */
    setMaterial(0.08f, 0.10f, 0.13f, 0.70f, 60.0f);

    glPushMatrix();
    glTranslatef(0.8f, 3.0f, 0.0f);
    drawCube(1.4f, 1.0f, 0.12f);
    glPopMatrix();

    setEmissiveMaterial(0.08f, 0.38f, 0.58f, 0.18f);

    glPushMatrix();
    glTranslatef(0.8f, 3.0f, 0.07f);
    drawCube(1.15f, 0.73f, 0.035f);
    glPopMatrix();

    resetMaterialEmission();

    setMaterial(0.20f, 0.22f, 0.25f, 0.60f, 55.0f);

    glPushMatrix();
    glTranslatef(0.8f, 2.55f, 0.0f);
    drawCylinderY(0.08f, 0.55f, 16);
    glPopMatrix();
}

void drawBench()
{
    setMaterial(0.58f, 0.31f, 0.12f, 0.25f, 22.0f);

    glPushMatrix();
    glTranslatef(0.0f, 1.15f, 0.0f);
    drawTexturedBox(
        4.5f,
        0.34f,
        1.25f,
        textures[TEX_WOOD],
        4.0f,
        1.0f
    );
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 2.0f, 0.52f);
    drawTexturedBox(
        4.5f,
        1.25f,
        0.25f,
        textures[TEX_WOOD],
        4.0f,
        1.0f
    );
    glPopMatrix();

    setMaterial(0.22f, 0.23f, 0.25f, 0.65f, 55.0f);

    const float legX[2] = {-1.65f, 1.65f};

    for (int i = 0; i < 2; ++i)
    {
        glPushMatrix();
        glTranslatef(legX[i], 0.55f, -0.40f);
        drawCube(0.22f, 1.1f, 0.22f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(legX[i], 0.55f, 0.40f);
        drawCube(0.22f, 1.1f, 0.22f);
        glPopMatrix();
    }
}

void drawSpacePicture(float pictureType)
{
    /* Wooden outer frame */
    setMaterial(0.55f, 0.36f, 0.12f, 0.55f, 45.0f);

    glPushMatrix();
    glTranslatef(0.0f, 1.15f, 0.0f);
    drawCube(3.4f, 0.22f, 0.22f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, -1.15f, 0.0f);
    drawCube(3.4f, 0.22f, 0.22f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-1.6f, 0.0f, 0.0f);
    drawCube(0.22f, 2.4f, 0.22f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(1.6f, 0.0f, 0.0f);
    drawCube(0.22f, 2.4f, 0.22f);
    glPopMatrix();

    /* Dark picture background */
    setMaterial(0.025f, 0.04f, 0.11f, 0.12f, 10.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.08f);
    drawCube(3.0f, 2.0f, 0.10f);
    glPopMatrix();

    if (pictureType < 0.5f)
        setEmissiveMaterial(0.95f, 0.72f, 0.16f, 0.18f);
    else if (pictureType < 1.5f)
        setEmissiveMaterial(0.25f, 0.55f, 0.92f, 0.12f);
    else
        setEmissiveMaterial(0.74f, 0.29f, 0.64f, 0.12f);

    glPushMatrix();

    if (pictureType < 0.5f)
        glTranslatef(-0.3f, 0.1f, 0.05f);
    else if (pictureType < 1.5f)
        glTranslatef(0.4f, -0.1f, 0.05f);
    else
        glTranslatef(0.0f, 0.0f, 0.05f);

    glScalef(0.85f, 0.65f, 0.13f);
    glutSolidSphere(0.62, 20, 14);

    glPopMatrix();

    resetMaterialEmission();

    if (pictureType > 1.5f)
    {
        setMaterial(0.70f, 0.70f, 0.76f, 0.50f, 35.0f);

        glPushMatrix();

        glTranslatef(0.0f, 0.0f, 0.08f);
        glScalef(1.0f, 0.58f, 1.0f);
        glutSolidTorus(0.045, 0.82, 10, 30);

        glPopMatrix();
    }
}

void drawInformationBoard(float accentR,
                          float accentG,
                          float accentB)
{
    setMaterial(0.18f, 0.19f, 0.22f, 0.55f, 45.0f);

    glPushMatrix();
    glTranslatef(0.0f, 1.0f, 0.0f);
    drawCylinderY(0.09f, 2.0f, 14);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.12f, 0.0f);
    drawCube(1.25f, 0.24f, 0.85f);
    glPopMatrix();

    setMaterial(0.80f, 0.82f, 0.78f, 0.22f, 18.0f);

    glPushMatrix();
    glTranslatef(0.0f, 2.35f, 0.0f);
    drawCube(2.1f, 1.45f, 0.16f);
    glPopMatrix();

    setMaterial(accentR, accentG, accentB, 0.40f, 35.0f);

    glPushMatrix();
    glTranslatef(0.0f, 2.73f, 0.10f);
    drawCube(1.75f, 0.22f, 0.06f);
    glPopMatrix();

    /* Simple raised lines suggest printed information */
    setMaterial(0.17f, 0.18f, 0.20f, 0.15f, 12.0f);

    for (int i = 0; i < 3; ++i)
    {
        glPushMatrix();

        glTranslatef(
            -0.12f,
            2.35f - i * 0.25f,
            0.105f
        );

        drawCube(1.35f, 0.07f, 0.04f);

        glPopMatrix();
    }
}

void drawRopePost(float x, float z)
{
    setMaterial(0.72f, 0.55f, 0.14f, 0.85f, 75.0f);

    glPushMatrix();
    glTranslatef(x, 0.12f, z);
    drawCylinderY(0.25f, 0.24f, 20);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(x, 0.78f, z);
    drawCylinderY(0.09f, 1.32f, 16);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(x, 1.46f, z);
    glutSolidSphere(0.18, 18, 12);
    glPopMatrix();
}

void drawRopeBarrier(float x1, float z1,
                     float x2, float z2)
{
    drawRopePost(x1, z1);
    drawRopePost(x2, z2);

    setMaterial(0.56f, 0.035f, 0.045f, 0.22f, 18.0f);

    /*
       Two straight cylinders with a lower middle point approximate a
       gently sagging rope.
    */
    drawCylinderBetween(
        x1,
        1.22f,
        z1,
        (x1 + x2) * 0.5f,
        1.03f,
        (z1 + z2) * 0.5f,
        0.065f,
        12
    );

    drawCylinderBetween(
        (x1 + x2) * 0.5f,
        1.03f,
        (z1 + z2) * 0.5f,
        x2,
        1.22f,
        z2,
        0.065f,
        12
    );
}

void drawEntranceSign()
{
    setMaterial(0.13f, 0.18f, 0.25f, 0.55f, 45.0f);

    glPushMatrix();
    glTranslatef(0.0f, 9.3f, 15.68f);
    drawCube(8.0f, 1.3f, 0.18f);
    glPopMatrix();

    setEmissiveMaterial(0.18f, 0.58f, 0.92f, 0.15f);

    for (int i = -3; i <= 3; ++i)
    {
        glPushMatrix();
        glTranslatef(i * 0.85f, 9.3f, 15.55f);
        drawCube(0.48f, 0.14f, 0.05f);
        glPopMatrix();
    }

    resetMaterialEmission();
}

void drawRoom()
{
    /* Textured marble/tiled floor */
    setMaterial(0.88f, 0.89f, 0.92f, 0.28f, 28.0f);

    glPushMatrix();
    glTranslatef(0.0f, -0.16f, -2.0f);
    drawTexturedBox(
        36.0f,
        0.32f,
        36.0f,
        textures[TEX_FLOOR],
        9.0f,
        9.0f
    );
    glPopMatrix();

    /* Ceiling */
    setMaterial(0.67f, 0.70f, 0.76f, 0.18f, 18.0f);

    glPushMatrix();
    glTranslatef(0.0f, 12.15f, -2.0f);
    drawTexturedBox(
        36.0f,
        0.30f,
        36.0f,
        textures[TEX_WALL],
        8.0f,
        8.0f
    );
    glPopMatrix();

    /* Rear wall */
    glPushMatrix();
    glTranslatef(0.0f, 6.0f, -20.0f);
    drawTexturedBox(
        36.0f,
        12.0f,
        0.40f,
        textures[TEX_WALL],
        9.0f,
        3.0f
    );
    glPopMatrix();

    /*
       Each side wall is built from separate pieces surrounding a real
       opening. The scenery lies beyond those openings.
    */
    drawSideWallWithWindow(-18.0f, -6.0f, 8.0f);
    drawSideWallWithWindow(18.0f, 1.0f, 8.0f);

    /* Front wall leaves the central entrance open */
    glPushMatrix();
    glTranslatef(-10.5f, 6.0f, 16.0f);
    drawTexturedBox(
        15.0f,
        12.0f,
        0.40f,
        textures[TEX_WALL],
        4.0f,
        3.0f
    );
    glPopMatrix();

    glPushMatrix();
    glTranslatef(10.5f, 6.0f, 16.0f);
    drawTexturedBox(
        15.0f,
        12.0f,
        0.40f,
        textures[TEX_WALL],
        4.0f,
        3.0f
    );
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 10.0f, 16.0f);
    drawTexturedBox(
        6.0f,
        4.0f,
        0.40f,
        textures[TEX_WALL],
        2.0f,
        1.0f
    );
    glPopMatrix();

    drawDoor();
    drawEntranceSign();

    /* Wall-mounted space pictures */
    glPushMatrix();
    glTranslatef(-11.0f, 7.0f, -19.72f);
    drawSpacePicture(0.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 7.0f, -19.72f);
    drawSpacePicture(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(11.0f, 7.0f, -19.72f);
    drawSpacePicture(2.0f);
    glPopMatrix();

    /* Reception desk near the entrance */
    glPushMatrix();
    glTranslatef(-10.5f, 0.0f, 11.3f);
    glRotatef(12.0f, 0.0f, 1.0f, 0.0f);
    drawReceptionDesk();
    glPopMatrix();

    /* Two visitor benches */
    glPushMatrix();
    glTranslatef(-7.3f, 0.0f, 5.5f);
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
    drawBench();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(7.3f, 0.0f, 5.5f);
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
    drawBench();
    glPopMatrix();

    /* Visible ceiling-light housings */
    setEmissiveMaterial(0.96f, 0.84f, 0.62f, 0.16f);

    const float fixtureX[3] = {-10.0f, 0.0f, 10.0f};

    for (int i = 0; i < 3; ++i)
    {
        glPushMatrix();
        glTranslatef(fixtureX[i], 11.75f, -2.0f);
        drawCube(3.2f, 0.12f, 0.75f);
        glPopMatrix();
    }

    resetMaterialEmission();
}

/* ------------------------------------------------------------------------- */
/* Protective display frames                                                 */
/* ------------------------------------------------------------------------- */

void drawCaseFrame(float width, float height,
                   float depth, float bottomY)
{
    float x = width * 0.5f;
    float z = depth * 0.5f;
    float middleY = bottomY + height * 0.5f;

    setMaterial(0.48f, 0.53f, 0.59f, 0.90f, 90.0f);

    /* Four vertical case posts */
    for (int ix = -1; ix <= 1; ix += 2)
    {
        for (int iz = -1; iz <= 1; iz += 2)
        {
            glPushMatrix();

            glTranslatef(
                ix * x,
                middleY,
                iz * z
            );

            drawCube(0.10f, height, 0.10f);

            glPopMatrix();
        }
    }

    /* Bottom and top rectangular frame edges */
    for (int level = 0; level < 2; ++level)
    {
        float y = bottomY + level * height;

        for (int iz = -1; iz <= 1; iz += 2)
        {
            glPushMatrix();
            glTranslatef(0.0f, y, iz * z);
            drawCube(width, 0.10f, 0.10f);
            glPopMatrix();
        }

        for (int ix = -1; ix <= 1; ix += 2)
        {
            glPushMatrix();
            glTranslatef(ix * x, y, 0.0f);
            drawCube(0.10f, 0.10f, depth);
            glPopMatrix();
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Main artifact: mechanical Orrery                                          */
/* ------------------------------------------------------------------------- */

void drawGear(float radius, float y, int teeth,
              float r, float g, float b)
{
    setMaterial(r, g, b, 0.82f, 78.0f);

    glPushMatrix();

    glTranslatef(0.0f, y, 0.0f);
    drawHorizontalTorus(0.12f, radius);

    for (int i = 0; i < teeth; ++i)
    {
        float angle =
            360.0f * i /
            static_cast<float>(teeth);

        glPushMatrix();

        glRotatef(angle, 0.0f, 1.0f, 0.0f);
        glTranslatef(radius, 0.0f, 0.0f);
        drawCube(0.40f, 0.16f, 0.24f);

        glPopMatrix();
    }

    glPopMatrix();
}

void drawOrbitPlanet(float orbitRadius,
                     float planetRadius,
                     float orbitAngle,
                     float spinAngle,
                     float r,
                     float g,
                     float b,
                     bool hasRing)
{
    glPushMatrix();

    /*
       Parent rotation makes the planet and its arm revolve around the Sun.
    */
    glRotatef(orbitAngle, 0.0f, 1.0f, 0.0f);

    /* Mechanical arm from the center to the planet */
    setMaterial(0.54f, 0.58f, 0.64f, 0.82f, 75.0f);

    glPushMatrix();
    glTranslatef(orbitRadius * 0.5f, 0.0f, 0.0f);
    drawCylinderX(0.055f, orbitRadius, 12);
    glPopMatrix();

    glTranslatef(orbitRadius, 0.0f, 0.0f);

    /*
       This child rotation is the planet's spin around its own local axis.
    */
    glRotatef(spinAngle, 0.0f, 1.0f, 0.0f);
    glRotatef(12.0f, 0.0f, 0.0f, 1.0f);

    setMaterial(r, g, b, 0.50f, 48.0f);
    glutSolidSphere(planetRadius, 24, 16);

    /* Equatorial detail makes self-rotation easier to observe */
    setMaterial(0.84f, 0.85f, 0.88f, 0.60f, 55.0f);

    glPushMatrix();
    glScalef(1.03f, 1.03f, 1.03f);
    drawHorizontalTorus(0.018f, planetRadius * 0.72f);
    glPopMatrix();

    if (hasRing)
    {
        setMaterial(0.76f, 0.65f, 0.40f, 0.42f, 35.0f);

        glPushMatrix();
        glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
        drawHorizontalTorus(0.055f, planetRadius * 1.55f);
        glPopMatrix();
    }

    glPopMatrix();
}

void drawOrrery()
{
    /* Dark museum pedestal */
    setMaterial(0.10f, 0.12f, 0.15f, 0.72f, 62.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.35f, 0.0f);
    drawCylinderY(4.4f, 0.70f, 48);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.80f, 0.0f);
    drawCylinderY(3.75f, 0.40f, 48);
    glPopMatrix();

    /* Stable protective display frame */
    drawCaseFrame(10.2f, 7.2f, 10.2f, 0.65f);

    /*
       The complete mechanical assembly rotates below. The museum pedestal
       and protective frame remain stationary.
    */
    glPushMatrix();

    glRotatef(orreryRotation, 0.0f, 1.0f, 0.0f);

    /* Circular metal base */
    setMaterial(0.64f, 0.48f, 0.14f, 0.92f, 95.0f);

    glPushMatrix();
    glTranslatef(0.0f, 1.20f, 0.0f);
    drawCylinderY(3.25f, 0.55f, 48);
    glPopMatrix();

    /* Decorative lower gears */
    drawGear(
        2.45f,
        1.62f,
        18,
        0.72f,
        0.54f,
        0.15f
    );

    drawGear(
        1.45f,
        1.88f,
        14,
        0.55f,
        0.60f,
        0.67f
    );

    /* Central supporting column */
    setMaterial(0.52f, 0.57f, 0.64f, 0.92f, 95.0f);

    glPushMatrix();
    glTranslatef(0.0f, 2.85f, 0.0f);
    drawCylinderY(0.33f, 2.35f, 28);
    glPopMatrix();

    setMaterial(0.74f, 0.55f, 0.12f, 0.95f, 105.0f);

    glPushMatrix();
    glTranslatef(0.0f, 4.15f, 0.0f);
    drawCylinderY(0.65f, 0.32f, 32);
    glPopMatrix();

    /* Metallic Sun */
    setEmissiveMaterial(0.95f, 0.64f, 0.10f, 0.18f);

    glPushMatrix();
    glTranslatef(0.0f, 4.80f, 0.0f);
    glutSolidSphere(0.82, 32, 22);
    glPopMatrix();

    resetMaterialEmission();

    /*
       All three orbit branches share the Sun's coordinate system but are
       isolated from one another by their own matrix stack operations.
    */
    glPushMatrix();

    glTranslatef(0.0f, 4.80f, 0.0f);

    setMaterial(0.65f, 0.69f, 0.74f, 0.88f, 82.0f);

    drawHorizontalTorus(0.040f, 1.85f);
    drawHorizontalTorus(0.040f, 3.05f);
    drawHorizontalTorus(0.040f, 4.15f);

    drawOrbitPlanet(
        1.85f,
        0.30f,
        planetOrbit[0],
        planetSpin[0],
        0.20f,
        0.48f,
        0.92f,
        false
    );

    drawOrbitPlanet(
        3.05f,
        0.42f,
        planetOrbit[1],
        planetSpin[1],
        0.82f,
        0.26f,
        0.12f,
        false
    );

    drawOrbitPlanet(
        4.15f,
        0.50f,
        planetOrbit[2],
        planetSpin[2],
        0.72f,
        0.59f,
        0.32f,
        true
    );

    glPopMatrix();
    glPopMatrix();
}

/* ------------------------------------------------------------------------- */
/* Satellite artifact                                                        */
/* ------------------------------------------------------------------------- */

void drawSolarPanel(float centerX)
{
    setMaterial(0.035f, 0.16f, 0.52f, 0.72f, 68.0f);

    glPushMatrix();
    glTranslatef(centerX, 0.0f, 0.0f);
    drawCube(2.25f, 1.75f, 0.12f);
    glPopMatrix();

    /* Raised metal grid on the blue panel */
    setMaterial(0.48f, 0.58f, 0.70f, 0.86f, 82.0f);

    for (int i = -2; i <= 2; ++i)
    {
        glPushMatrix();

        glTranslatef(
            centerX + i * 0.43f,
            0.0f,
            0.075f
        );

        drawCube(0.035f, 1.72f, 0.035f);

        glPopMatrix();
    }

    for (int i = -1; i <= 1; ++i)
    {
        glPushMatrix();

        glTranslatef(
            centerX,
            i * 0.52f,
            0.075f
        );

        drawCube(2.22f, 0.035f, 0.035f);

        glPopMatrix();
    }
}

void drawSatellite()
{
    /* Display platform */
    setMaterial(0.10f, 0.12f, 0.15f, 0.72f, 62.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.25f, 0.0f);
    drawCylinderY(2.35f, 0.50f, 40);
    glPopMatrix();

    /* Supporting stand */
    setMaterial(0.44f, 0.48f, 0.54f, 0.86f, 82.0f);

    glPushMatrix();
    glTranslatef(0.0f, 1.75f, 0.0f);
    drawCylinderY(0.15f, 2.70f, 18);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 3.10f, 0.0f);
    drawCube(1.2f, 0.18f, 1.2f);
    glPopMatrix();

    /*
       Only the satellite model rotates. Its stand and platform are fixed.
    */
    glPushMatrix();

    glTranslatef(0.0f, 3.65f, 0.0f);
    glRotatef(satelliteRotation, 0.0f, 1.0f, 0.0f);

    /* Metallic central body */
    setMaterial(0.56f, 0.61f, 0.68f, 0.95f, 105.0f);
    drawCylinderX(0.68f, 2.15f, 28);

    setMaterial(0.78f, 0.58f, 0.15f, 0.88f, 90.0f);

    glPushMatrix();
    glTranslatef(-0.70f, 0.0f, 0.0f);
    drawCylinderX(0.72f, 0.13f, 24);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.70f, 0.0f, 0.0f);
    drawCylinderX(0.72f, 0.13f, 24);
    glPopMatrix();

    drawSolarPanel(-2.35f);
    drawSolarPanel(2.35f);

    /* Antenna dish */
    setMaterial(0.77f, 0.79f, 0.83f, 0.92f, 95.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.67f, 0.0f);
    drawConeY(0.72f, 0.38f, 30);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.69f, 0.0f);
    drawHorizontalTorus(0.045f, 0.70f);
    glPopMatrix();

    setMaterial(0.36f, 0.39f, 0.43f, 0.85f, 80.0f);

    drawCylinderBetween(
        0.0f,
        0.72f,
        0.0f,
        0.0f,
        1.38f,
        0.0f,
        0.045f,
        12
    );

    glPushMatrix();
    glTranslatef(0.0f, 1.42f, 0.0f);
    glutSolidSphere(0.10, 14, 10);
    glPopMatrix();

    /* Separate communication antenna */
    drawCylinderBetween(
        0.75f,
        0.40f,
        0.0f,
        1.40f,
        1.45f,
        0.35f,
        0.035f,
        10
    );

    glPushMatrix();
    glTranslatef(1.40f, 1.45f, 0.35f);
    glutSolidSphere(0.085, 12, 8);
    glPopMatrix();

    glPopMatrix();
}

/* ------------------------------------------------------------------------- */
/* Rocket artifact                                                           */
/* ------------------------------------------------------------------------- */

void drawRocketFin()
{
    const float innerX = 0.58f;
    const float outerX = 2.05f;
    const float topY = 2.15f;
    const float thickness = 0.10f;

    setMaterial(0.72f, 0.10f, 0.08f, 0.52f, 45.0f);

    /* Front and back triangles */
    glBegin(GL_TRIANGLES);

    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(innerX, 0.0f, thickness);
    glVertex3f(outerX, 0.0f, thickness);
    glVertex3f(innerX, topY, thickness);

    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(innerX, topY, -thickness);
    glVertex3f(outerX, 0.0f, -thickness);
    glVertex3f(innerX, 0.0f, -thickness);

    glEnd();

    /* Fin thickness */
    glBegin(GL_QUADS);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(innerX, 0.0f, -thickness);
    glVertex3f(outerX, 0.0f, -thickness);
    glVertex3f(outerX, 0.0f, thickness);
    glVertex3f(innerX, 0.0f, thickness);

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(innerX, 0.0f, thickness);
    glVertex3f(innerX, topY, thickness);
    glVertex3f(innerX, topY, -thickness);
    glVertex3f(innerX, 0.0f, -thickness);

    /*
       Approximate normalized outward normal of the sloped outer edge.
    */
    glNormal3f(0.82f, 0.57f, 0.0f);
    glVertex3f(outerX, 0.0f, thickness);
    glVertex3f(outerX, 0.0f, -thickness);
    glVertex3f(innerX, topY, -thickness);
    glVertex3f(innerX, topY, thickness);

    glEnd();
}

void drawRocketWindow(float y)
{
    setMaterial(0.03f, 0.26f, 0.48f, 0.92f, 95.0f);

    glPushMatrix();

    glTranslatef(0.0f, y, 0.80f);
    glScalef(0.48f, 0.48f, 0.10f);
    glutSolidSphere(0.60, 22, 16);

    glPopMatrix();

    setMaterial(0.48f, 0.54f, 0.61f, 0.90f, 92.0f);

    glPushMatrix();

    glTranslatef(0.0f, y, 0.84f);
    glutSolidTorus(0.065, 0.33, 12, 28);

    glPopMatrix();
}

void drawRocket()
{
    /* Launch/display platform */
    setMaterial(0.10f, 0.12f, 0.15f, 0.72f, 62.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.25f, 0.0f);
    drawCylinderY(2.5f, 0.50f, 42);
    glPopMatrix();

    setMaterial(0.40f, 0.44f, 0.49f, 0.86f, 80.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.62f, 0.0f);
    drawCylinderY(1.35f, 0.30f, 32);
    glPopMatrix();

    /* Engine section */
    setMaterial(0.24f, 0.27f, 0.31f, 0.92f, 92.0f);

    glPushMatrix();
    glTranslatef(0.0f, 1.05f, 0.0f);
    drawCylinderY(0.94f, 0.85f, 30);
    glPopMatrix();

    /* Four downward-facing engine nozzles */
    setMaterial(0.75f, 0.77f, 0.79f, 0.88f, 88.0f);

    for (int i = 0; i < 4; ++i)
    {
        float angle = i * 90.0f;
        float radians = degreesToRadians(angle);

        float x = std::cos(radians) * 0.42f;
        float z = std::sin(radians) * 0.42f;

        glPushMatrix();

        glTranslatef(x, 0.68f, z);

        /*
           A GLUT cone points along +Z. Rotating +90 degrees around X
           points it toward world -Y.
        */
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        glutSolidCone(0.25, 0.45, 16, 5);

        glPopMatrix();
    }

    /* White cylindrical body */
    setMaterial(0.88f, 0.90f, 0.92f, 0.72f, 66.0f);

    glPushMatrix();
    glTranslatef(0.0f, 3.95f, 0.0f);
    drawCylinderY(0.82f, 5.6f, 32);
    glPopMatrix();

    /* Red nose cone */
    setMaterial(0.76f, 0.12f, 0.10f, 0.58f, 52.0f);

    glPushMatrix();
    glTranslatef(0.0f, 6.75f, 0.0f);
    drawConeY(0.82f, 1.65f, 32);
    glPopMatrix();

    /* Decorative body rings */
    setMaterial(0.48f, 0.52f, 0.58f, 0.90f, 90.0f);

    glPushMatrix();
    glTranslatef(0.0f, 2.15f, 0.0f);
    drawHorizontalTorus(0.065f, 0.82f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 6.05f, 0.0f);
    drawHorizontalTorus(0.065f, 0.82f);
    glPopMatrix();

    /* Four fins */
    for (int i = 0; i < 4; ++i)
    {
        glPushMatrix();

        glTranslatef(0.0f, 1.12f, 0.0f);
        glRotatef(i * 90.0f, 0.0f, 1.0f, 0.0f);
        drawRocketFin();

        glPopMatrix();
    }

    drawRocketWindow(4.55f);
    drawRocketWindow(5.55f);
}

/* ------------------------------------------------------------------------- */
/* Moon-rock display                                                         */
/* ------------------------------------------------------------------------- */

void drawMoonRock(float x, float y, float z,
                  float scaleX, float scaleY, float scaleZ,
                  float rotation)
{
    setMaterial(0.35f, 0.36f, 0.37f, 0.14f, 12.0f);

    glPushMatrix();

    glTranslatef(x, y, z);
    glRotatef(rotation, 0.4f, 1.0f, 0.3f);

    /*
       A low-resolution sphere plus nonuniform scaling creates a simple,
       irregular-looking rock while remaining beginner-friendly.
    */
    glScalef(scaleX, scaleY, scaleZ);
    glutSolidSphere(1.0, 10, 7);

    glPopMatrix();
}

void drawMoonRockDisplay()
{
    setMaterial(0.10f, 0.12f, 0.15f, 0.70f, 60.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.35f, 0.0f);
    drawCube(5.0f, 0.70f, 4.0f);
    glPopMatrix();

    setMaterial(0.26f, 0.28f, 0.31f, 0.45f, 38.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.88f, 0.0f);
    drawCube(4.3f, 0.38f, 3.3f);
    glPopMatrix();

    drawMoonRock(
        -1.15f,
        1.55f,
        0.20f,
        0.65f,
        0.85f,
        0.58f,
        17.0f
    );

    drawMoonRock(
        0.15f,
        1.45f,
        -0.55f,
        0.82f,
        0.55f,
        0.68f,
        44.0f
    );

    drawMoonRock(
        1.20f,
        1.50f,
        0.45f,
        0.56f,
        0.72f,
        0.48f,
        78.0f
    );

    drawMoonRock(
        0.25f,
        1.30f,
        0.80f,
        0.42f,
        0.38f,
        0.48f,
        105.0f
    );

    /* Stable frame instead of difficult transparent glass panels */
    drawCaseFrame(5.4f, 4.8f, 4.4f, 0.70f);

    /* Visible spotlight housing */
    setMaterial(0.27f, 0.29f, 0.33f, 0.82f, 78.0f);

    glPushMatrix();

    glTranslatef(0.0f, 5.25f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glutSolidCone(0.38, 0.55, 20, 5);

    glPopMatrix();

    setEmissiveMaterial(0.75f, 0.86f, 1.0f, 0.14f);

    glPushMatrix();
    glTranslatef(0.0f, 5.08f, 0.0f);
    glutSolidSphere(0.16, 16, 10);
    glPopMatrix();

    resetMaterialEmission();
}

/* ------------------------------------------------------------------------- */
/* Simple astronaut statue                                                   */
/* ------------------------------------------------------------------------- */

void drawAstronaut()
{
    /* Display platform */
    setMaterial(0.10f, 0.12f, 0.15f, 0.70f, 62.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.25f, 0.0f);
    drawCylinderY(2.1f, 0.50f, 38);
    glPopMatrix();

    /* Legs */
    setMaterial(0.82f, 0.85f, 0.88f, 0.42f, 38.0f);

    glPushMatrix();
    glTranslatef(-0.38f, 1.25f, 0.0f);
    drawCube(0.55f, 1.75f, 0.62f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.38f, 1.25f, 0.0f);
    drawCube(0.55f, 1.75f, 0.62f);
    glPopMatrix();

    /* Boots */
    setMaterial(0.20f, 0.22f, 0.25f, 0.38f, 30.0f);

    glPushMatrix();
    glTranslatef(-0.38f, 0.47f, 0.18f);
    drawCube(0.68f, 0.35f, 0.92f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.38f, 0.47f, 0.18f);
    drawCube(0.68f, 0.35f, 0.92f);
    glPopMatrix();

    /* Torso */
    setMaterial(0.88f, 0.90f, 0.92f, 0.48f, 42.0f);

    glPushMatrix();
    glTranslatef(0.0f, 2.85f, 0.0f);
    drawCube(1.65f, 1.85f, 0.95f);
    glPopMatrix();

    /* Chest control panel */
    setMaterial(0.13f, 0.17f, 0.23f, 0.62f, 55.0f);

    glPushMatrix();
    glTranslatef(0.0f, 2.85f, 0.51f);
    drawCube(1.02f, 0.65f, 0.12f);
    glPopMatrix();

    setEmissiveMaterial(0.12f, 0.65f, 0.42f, 0.12f);

    glPushMatrix();
    glTranslatef(-0.27f, 2.95f, 0.585f);
    glutSolidSphere(0.08, 12, 8);
    glPopMatrix();

    resetMaterialEmission();

    setEmissiveMaterial(0.82f, 0.18f, 0.12f, 0.10f);

    glPushMatrix();
    glTranslatef(0.27f, 2.95f, 0.585f);
    glutSolidSphere(0.08, 12, 8);
    glPopMatrix();

    resetMaterialEmission();

    /* Arms */
    setMaterial(0.82f, 0.85f, 0.88f, 0.42f, 38.0f);

    drawCylinderBetween(
        -0.75f,
        3.45f,
        0.0f,
        -1.42f,
        2.35f,
        0.20f,
        0.25f,
        18
    );

    drawCylinderBetween(
        0.75f,
        3.45f,
        0.0f,
        1.42f,
        2.35f,
        0.20f,
        0.25f,
        18
    );

    glPushMatrix();
    glTranslatef(-1.44f, 2.28f, 0.21f);
    glutSolidSphere(0.31, 18, 12);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(1.44f, 2.28f, 0.21f);
    glutSolidSphere(0.31, 18, 12);
    glPopMatrix();

    /* Backpack */
    setMaterial(0.58f, 0.62f, 0.67f, 0.62f, 55.0f);

    glPushMatrix();
    glTranslatef(0.0f, 2.95f, -0.72f);
    drawCube(1.35f, 1.65f, 0.62f);
    glPopMatrix();

    /* Oxygen tanks */
    setMaterial(0.42f, 0.47f, 0.53f, 0.78f, 68.0f);

    glPushMatrix();
    glTranslatef(-0.42f, 2.95f, -1.04f);
    drawCylinderY(0.22f, 1.45f, 16);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.42f, 2.95f, -1.04f);
    drawCylinderY(0.22f, 1.45f, 16);
    glPopMatrix();

    /* Helmet shell */
    setMaterial(0.88f, 0.90f, 0.92f, 0.68f, 62.0f);

    glPushMatrix();
    glTranslatef(0.0f, 4.45f, 0.0f);
    glutSolidSphere(0.82, 28, 20);
    glPopMatrix();

    /* Dark visor */
    setMaterial(0.12f, 0.20f, 0.29f, 0.95f, 105.0f);

    glPushMatrix();

    glTranslatef(0.0f, 4.43f, 0.62f);
    glScalef(0.68f, 0.48f, 0.16f);
    glutSolidSphere(0.75, 26, 18);

    glPopMatrix();
}

/* ------------------------------------------------------------------------- */
/* Artifact placement and information boards                                 */
/* ------------------------------------------------------------------------- */

void applyArtifactTransform(const ArtifactTransform &transform)
{
    /*
       Object-coordinate transformation order:
       translation -> rotation -> uniform scale -> local object geometry.
    */
    glTranslatef(transform.x, transform.y, transform.z);
    glRotatef(transform.rotationY, 0.0f, 1.0f, 0.0f);
    glScalef(
        transform.scale,
        transform.scale,
        transform.scale
    );
}

void drawArtifacts()
{
    /* Central Orrery */
    glPushMatrix();

    glTranslatef(0.0f, 0.0f, -5.0f);
    applyArtifactTransform(artifactTransforms[0]);
    drawOrrery();

    glPopMatrix();

    /* Left-side satellite exhibition */
    glPushMatrix();

    glTranslatef(-11.5f, 0.0f, -11.5f);
    applyArtifactTransform(artifactTransforms[1]);
    drawSatellite();

    glPopMatrix();

    /* Right-side rocket exhibition */
    glPushMatrix();

    glTranslatef(11.5f, 0.0f, -11.5f);
    applyArtifactTransform(artifactTransforms[2]);
    drawRocket();

    glPopMatrix();

    /* Moon-rock display */
    glPushMatrix();
    glTranslatef(-11.5f, 0.0f, 2.5f);
    drawMoonRockDisplay();
    glPopMatrix();

    /* Astronaut statue */
    glPushMatrix();
    glTranslatef(11.5f, 0.0f, 2.5f);
    drawAstronaut();
    glPopMatrix();

    /* Orrery information board */
    glPushMatrix();
    glTranslatef(6.7f, 0.0f, -0.8f);
    glRotatef(-35.0f, 0.0f, 1.0f, 0.0f);
    drawInformationBoard(0.86f, 0.61f, 0.12f);
    glPopMatrix();

    /* Satellite board */
    glPushMatrix();
    glTranslatef(-7.2f, 0.0f, -13.6f);
    glRotatef(25.0f, 0.0f, 1.0f, 0.0f);
    drawInformationBoard(0.10f, 0.38f, 0.84f);
    glPopMatrix();

    /* Rocket board */
    glPushMatrix();
    glTranslatef(7.2f, 0.0f, -13.6f);
    glRotatef(-25.0f, 0.0f, 1.0f, 0.0f);
    drawInformationBoard(0.82f, 0.16f, 0.11f);
    glPopMatrix();

    /* Moon-rock board */
    glPushMatrix();
    glTranslatef(-7.3f, 0.0f, 1.0f);
    glRotatef(28.0f, 0.0f, 1.0f, 0.0f);
    drawInformationBoard(0.48f, 0.50f, 0.52f);
    glPopMatrix();

    /* Astronaut board */
    glPushMatrix();
    glTranslatef(7.3f, 0.0f, 1.0f);
    glRotatef(-28.0f, 0.0f, 1.0f, 0.0f);
    drawInformationBoard(0.85f, 0.88f, 0.92f);
    glPopMatrix();

    /* Rope barriers surrounding the central Orrery */
    drawRopeBarrier(-6.2f, -10.8f, 6.2f, -10.8f);
    drawRopeBarrier(6.2f, -10.8f, 6.2f, 0.8f);
    drawRopeBarrier(6.2f, 0.8f, -6.2f, 0.8f);
    drawRopeBarrier(-6.2f, 0.8f, -6.2f, -10.8f);
}

/* ------------------------------------------------------------------------- */
/* Fixed-function lighting                                                   */
/* ------------------------------------------------------------------------- */

void setupLights()
{
    GLfloat globalAmbientNight[] =
    {
        0.055f,
        0.065f,
        0.090f,
        1.0f
    };

    GLfloat globalAmbientDay[] =
    {
        0.13f,
        0.14f,
        0.16f,
        1.0f
    };

    glLightModelfv(
        GL_LIGHT_MODEL_AMBIENT,
        nightMode ? globalAmbientNight : globalAmbientDay
    );

    /*
       GL_LIGHT0: warm main ceiling light.
    */
    GLfloat light0Ambient[] =
    {
        0.10f,
        0.075f,
        0.050f,
        1.0f
    };

    GLfloat light0Diffuse[] =
    {
        1.00f,
        0.79f,
        0.56f,
        1.0f
    };

    GLfloat light0Specular[] =
    {
        0.95f,
        0.82f,
        0.67f,
        1.0f
    };

    GLfloat light0Position[] =
    {
        0.0f,
        10.8f,
        6.0f,
        1.0f
    };

    glLightfv(GL_LIGHT0, GL_AMBIENT, light0Ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0Specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light0Position);
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 180.0f);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.012f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.001f);

    /*
       GL_LIGHT1: cool spotlight directed toward the Orrery.
    */
    GLfloat light1Ambient[] =
    {
        0.025f,
        0.035f,
        0.060f,
        1.0f
    };

    GLfloat light1Diffuse[] =
    {
        0.52f,
        0.70f,
        1.00f,
        1.0f
    };

    GLfloat light1Specular[] =
    {
        0.72f,
        0.84f,
        1.00f,
        1.0f
    };

    GLfloat light1Position[] =
    {
        0.0f,
        11.2f,
        -5.0f,
        1.0f
    };

    GLfloat light1Direction[] =
    {
        0.0f,
        -1.0f,
        0.0f
    };

    glLightfv(GL_LIGHT1, GL_AMBIENT, light1Ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1Diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1Specular);
    glLightfv(GL_LIGHT1, GL_POSITION, light1Position);
    glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, light1Direction);
    glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 32.0f);
    glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 22.0f);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.015f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.001f);

    /*
       GL_LIGHT2: small spotlight above the Moon-rock display.
    */
    GLfloat light2Ambient[] =
    {
        0.015f,
        0.025f,
        0.045f,
        1.0f
    };

    GLfloat light2Diffuse[] =
    {
        0.55f,
        0.74f,
        1.00f,
        1.0f
    };

    GLfloat light2Specular[] =
    {
        0.78f,
        0.88f,
        1.00f,
        1.0f
    };

    GLfloat light2Position[] =
    {
        -11.5f,
        6.8f,
        2.5f,
        1.0f
    };

    GLfloat light2Direction[] =
    {
        0.0f,
        -1.0f,
        0.0f
    };

    glLightfv(GL_LIGHT2, GL_AMBIENT, light2Ambient);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2Diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, light2Specular);
    glLightfv(GL_LIGHT2, GL_POSITION, light2Position);
    glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, light2Direction);
    glLightf(GL_LIGHT2, GL_SPOT_CUTOFF, 27.0f);
    glLightf(GL_LIGHT2, GL_SPOT_EXPONENT, 25.0f);
    glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.020f);
    glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.002f);

    if (lightEnabled[0])
        glEnable(GL_LIGHT0);
    else
        glDisable(GL_LIGHT0);

    if (lightEnabled[1])
        glEnable(GL_LIGHT1);
    else
        glDisable(GL_LIGHT1);

    if (lightEnabled[2])
        glEnable(GL_LIGHT2);
    else
        glDisable(GL_LIGHT2);
}

/* ------------------------------------------------------------------------- */
/* Two-dimensional information overlay                                       */
/* ------------------------------------------------------------------------- */

const char *selectedArtifactName()
{
    if (selectedArtifact == 0)
        return "Mechanical Orrery";

    if (selectedArtifact == 1)
        return "Satellite";

    return "Rocket";
}

void drawBitmapText(float x, float y,
                    const char *text, void *font)
{
    glRasterPos2f(x, y);

    for (const char *character = text;
         *character != '\0';
         ++character)
    {
        glutBitmapCharacter(
            font,
            static_cast<unsigned char>(*character)
        );
    }
}

void drawOverlay()
{
    char statusLine[180];

    /*
       Save all states altered by the 2D pass. The matrices are handled
       separately below.
    */
    glPushAttrib(
        GL_ENABLE_BIT |
        GL_CURRENT_BIT |
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT |
        GL_LINE_BIT
    );

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    gluOrtho2D(
        0.0,
        static_cast<double>(windowWidth),
        0.0,
        static_cast<double>(windowHeight)
    );

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0.84f, 0.92f, 1.0f);

    drawBitmapText(
        18.0f,
        windowHeight - 28.0f,
        "Interactive 3D Space Artifact Gallery",
        GLUT_BITMAP_HELVETICA_18
    );

    std::sprintf(
        statusLine,
        "Selected: %s   |   Animation: %s   |   Exterior: %s",
        selectedArtifactName(),
        animationRunning ? "RUNNING" : "PAUSED",
        nightMode ? "NIGHT" : "DAY"
    );

    glColor3f(1.0f, 0.90f, 0.56f);

    drawBitmapText(
        18.0f,
        windowHeight - 52.0f,
        statusLine,
        GLUT_BITMAP_9_BY_15
    );

    glColor3f(0.88f, 0.90f, 0.94f);

    drawBitmapText(
        18.0f,
        windowHeight - 74.0f,
        "WASD move | Arrow keys look | Tab select | Space pause | H help",
        GLUT_BITMAP_8_BY_13
    );

    if (showHelp)
    {
        int y = windowHeight - 104;

        glColor3f(0.74f, 0.86f, 1.0f);

        drawBitmapText(
            18.0f,
            static_cast<float>(y),
            "Selected object: J/L left-right, I/K forward-back, U/O up-down",
            GLUT_BITMAP_8_BY_13
        );

        y -= 18;

        drawBitmapText(
            18.0f,
            static_cast<float>(y),
            "Q/E rotate, +/- scale (scale is clamped from 0.6 to 1.5)",
            GLUT_BITMAP_8_BY_13
        );

        y -= 18;

        drawBitmapText(
            18.0f,
            static_cast<float>(y),
            "1/2/3 lights | N day/night | P comet | G door | R reset | Esc exit",
            GLUT_BITMAP_8_BY_13
        );
    }

    std::sprintf(
        statusLine,
        "Lights: 1[%s] 2[%s] 3[%s]   Door: %s",
        lightEnabled[0] ? "ON" : "OFF",
        lightEnabled[1] ? "ON" : "OFF",
        lightEnabled[2] ? "ON" : "OFF",
        doorAngle > 44.0f ? "OPEN" : "CLOSED"
    );

    glColor3f(0.78f, 0.83f, 0.89f);

    drawBitmapText(
        18.0f,
        18.0f,
        statusLine,
        GLUT_BITMAP_8_BY_13
    );

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

/* ------------------------------------------------------------------------- */
/* Camera, reset, rendering, and interaction                                  */
/* ------------------------------------------------------------------------- */

void resetScene()
{
    camera.x = 0.0f;
    camera.y = 4.2f;
    camera.z = 14.0f;
    camera.yaw = -90.0f;
    camera.pitch = -5.0f;

    for (int i = 0; i < ARTIFACT_COUNT; ++i)
    {
        artifactTransforms[i].x = 0.0f;
        artifactTransforms[i].y = 0.0f;
        artifactTransforms[i].z = 0.0f;
        artifactTransforms[i].rotationY = 0.0f;
        artifactTransforms[i].scale = 1.0f;
    }

    selectedArtifact = 0;

    orreryRotation = 0.0f;

    planetOrbit[0] = 0.0f;
    planetOrbit[1] = 120.0f;
    planetOrbit[2] = 235.0f;

    planetSpin[0] = 0.0f;
    planetSpin[1] = 0.0f;
    planetSpin[2] = 0.0f;

    satelliteRotation = 0.0f;
    cometPosition = 0.0f;

    doorTargetOpen = false;
    doorAngle = 0.0f;

    animationRunning = true;
}

void clampCameraPosition()
{
    camera.x = clampFloat(camera.x, -17.2f, 17.2f);
    camera.y = clampFloat(camera.y, 1.2f, 10.5f);
    camera.z = clampFloat(camera.z, -18.8f, 21.0f);
}

void clampSelectedTransform()
{
    ArtifactTransform &transform =
        artifactTransforms[selectedArtifact];

    transform.x =
        clampFloat(transform.x, -6.0f, 6.0f);

    transform.y =
        clampFloat(transform.y, -0.5f, 3.0f);

    transform.z =
        clampFloat(transform.z, -6.0f, 6.0f);

    transform.scale =
        clampFloat(transform.scale, 0.6f, 1.5f);

    if (transform.rotationY >= 360.0f)
        transform.rotationY -= 360.0f;

    if (transform.rotationY <= -360.0f)
        transform.rotationY += 360.0f;
}

void display()
{
    if (nightMode)
        glClearColor(0.015f, 0.022f, 0.045f, 1.0f);
    else
        glClearColor(0.30f, 0.56f, 0.76f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float yawRadians =
        degreesToRadians(camera.yaw);

    float pitchRadians =
        degreesToRadians(camera.pitch);

    float directionX =
        std::cos(pitchRadians) *
        std::cos(yawRadians);

    float directionY =
        std::sin(pitchRadians);

    float directionZ =
        std::cos(pitchRadians) *
        std::sin(yawRadians);

    /*
       Viewing-coordinate transformation.
    */
    gluLookAt(
        camera.x,
        camera.y,
        camera.z,
        camera.x + directionX,
        camera.y + directionY,
        camera.z + directionZ,
        0.0f,
        1.0f,
        0.0f
    );

    /*
       Light positions are submitted after gluLookAt() and before any local
       model transformation. Therefore they stay fixed in museum coordinates.
    */
    setupLights();

    drawExterior();
    drawRoom();
    drawArtifacts();
    drawOverlay();

    glutSwapBuffers();
}

void reshape(int width, int height)
{
    if (height <= 0)
        height = 1;

    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(
        62.0,
        static_cast<double>(width) /
            static_cast<double>(height),
        0.10,
        120.0
    );

    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int, int)
{
    /* Make alphabetic controls case-insensitive */
    if (key >= 'A' && key <= 'Z')
    {
        key = static_cast<unsigned char>(
            key - 'A' + 'a'
        );
    }

    if (key == 27)
    {
        if (sharedQuadric)
        {
            gluDeleteQuadric(sharedQuadric);
            sharedQuadric = 0;
        }

        if (textures[0] != 0)
        {
            glDeleteTextures(TEX_COUNT, textures);
        }

        std::exit(0);
    }

    if (key == '\t')
    {
        selectedArtifact =
            (selectedArtifact + 1) % ARTIFACT_COUNT;

        glutPostRedisplay();
        return;
    }

    if (key == ' ')
    {
        animationRunning = !animationRunning;

        glutPostRedisplay();
        return;
    }

    float yawRadians =
        degreesToRadians(camera.yaw);

    float forwardX = std::cos(yawRadians);
    float forwardZ = std::sin(yawRadians);

    float rightX = -forwardZ;
    float rightZ = forwardX;

    const float cameraStep = 0.45f;

    ArtifactTransform &transform =
        artifactTransforms[selectedArtifact];

    switch (key)
    {
        case 'w':
            camera.x += forwardX * cameraStep;
            camera.z += forwardZ * cameraStep;
            break;

        case 's':
            camera.x -= forwardX * cameraStep;
            camera.z -= forwardZ * cameraStep;
            break;

        case 'a':
            camera.x -= rightX * cameraStep;
            camera.z -= rightZ * cameraStep;
            break;

        case 'd':
            camera.x += rightX * cameraStep;
            camera.z += rightZ * cameraStep;
            break;

        case 'j':
            transform.x -= 0.30f;
            break;

        case 'l':
            transform.x += 0.30f;
            break;

        case 'i':
            transform.z -= 0.30f;
            break;

        case 'k':
            transform.z += 0.30f;
            break;

        case 'u':
            transform.y += 0.20f;
            break;

        case 'o':
            transform.y -= 0.20f;
            break;

        case 'q':
            transform.rotationY -= 5.0f;
            break;

        case 'e':
            transform.rotationY += 5.0f;
            break;

        case '+':
        case '=':
            transform.scale += 0.05f;
            break;

        case '-':
        case '_':
            transform.scale -= 0.05f;
            break;

        case '1':
            lightEnabled[0] = !lightEnabled[0];
            break;

        case '2':
            lightEnabled[1] = !lightEnabled[1];
            break;

        case '3':
            lightEnabled[2] = !lightEnabled[2];
            break;

        case 'n':
            nightMode = !nightMode;
            break;

        case 'p':
            cometVisible = !cometVisible;
            break;

        case 'g':
            doorTargetOpen = !doorTargetOpen;
            break;

        case 'h':
            showHelp = !showHelp;
            break;

        case 'r':
            resetScene();
            break;

        default:
            break;
    }

    clampCameraPosition();
    clampSelectedTransform();

    glutPostRedisplay();
}

void specialKeys(int key, int, int)
{
    const float lookStep = 3.0f;

    switch (key)
    {
        case GLUT_KEY_LEFT:
            camera.yaw -= lookStep;
            break;

        case GLUT_KEY_RIGHT:
            camera.yaw += lookStep;
            break;

        case GLUT_KEY_UP:
            camera.pitch += lookStep;
            break;

        case GLUT_KEY_DOWN:
            camera.pitch -= lookStep;
            break;

        default:
            break;
    }

    camera.pitch =
        clampFloat(camera.pitch, -80.0f, 80.0f);

    if (camera.yaw > 360.0f)
        camera.yaw -= 360.0f;

    if (camera.yaw < -360.0f)
        camera.yaw += 360.0f;

    glutPostRedisplay();
}

void timer(int)
{
    if (animationRunning)
    {
        /* Slow parent rotation of the mechanical Orrery */
        orreryRotation += 0.12f;

        /* Different revolution speeds */
        planetOrbit[0] += 0.72f;
        planetOrbit[1] += 0.43f;
        planetOrbit[2] += 0.25f;

        /* Different local spin speeds */
        planetSpin[0] += 2.20f;
        planetSpin[1] += 1.45f;
        planetSpin[2] += 0.92f;

        satelliteRotation += 0.28f;

        if (cometVisible)
        {
            cometPosition += 0.09f;

            if (cometPosition > 32.0f)
                cometPosition = 0.0f;
        }

        if (orreryRotation >= 360.0f)
            orreryRotation -= 360.0f;

        if (satelliteRotation >= 360.0f)
            satelliteRotation -= 360.0f;

        for (int i = 0; i < 3; ++i)
        {
            if (planetOrbit[i] >= 360.0f)
                planetOrbit[i] -= 360.0f;

            if (planetSpin[i] >= 360.0f)
                planetSpin[i] -= 360.0f;
        }
    }

    /*
       Door movement remains active while exhibit animation is paused.
    */
    const float targetAngle =
        doorTargetOpen ? 88.0f : 0.0f;

    const float doorStep = 2.0f;

    if (doorAngle < targetAngle)
    {
        doorAngle += doorStep;

        if (doorAngle > targetAngle)
            doorAngle = targetAngle;
    }
    else if (doorAngle > targetAngle)
    {
        doorAngle -= doorStep;

        if (doorAngle < targetAngle)
            doorAngle = targetAngle;
    }

    glutPostRedisplay();
    glutTimerFunc(TIMER_INTERVAL_MS, timer, 0);
}

/* ------------------------------------------------------------------------- */
/* Initialization and program entry point                                    */
/* ------------------------------------------------------------------------- */

void initializeOpenGL()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);

    glShadeModel(GL_SMOOTH);

    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    /*
       One reusable quadric is allocated after the OpenGL context exists.
       It is reused by every cylinder and disk.
    */
    sharedQuadric = gluNewQuadric();

    if (!sharedQuadric)
    {
        std::fprintf(
            stderr,
            "Unable to create the shared GLU quadric.\n"
        );

        std::exit(EXIT_FAILURE);
    }

    gluQuadricNormals(sharedQuadric, GLU_SMOOTH);
    gluQuadricTexture(sharedQuadric, GL_FALSE);

    createProceduralTextures();
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(
        GLUT_DOUBLE |
        GLUT_RGB |
        GLUT_DEPTH
    );

    glutInitWindowSize(1280, 720);
    glutInitWindowPosition(70, 45);

    glutCreateWindow(
        "Interactive 3D Space Artifact Gallery"
    );

    initializeOpenGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);

    glutTimerFunc(
        TIMER_INTERVAL_MS,
        timer,
        0
    );

    glutMainLoop();

    return 0;
}

