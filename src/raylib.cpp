#include "dib/raylib.h"
#include "raymath.h"
#include "rlgl.h"

float dib::raylib::get_zoom_2d()
{
    auto mat = rlGetMatrixModelview();

    Vector3 tr, scl;
    Quaternion rot;
    MatrixDecompose(mat, &tr, &rot, &scl);

    return Vector2Length({ .x = scl.x, .y = scl.y });
}