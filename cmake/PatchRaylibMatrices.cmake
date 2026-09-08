# Apply the matrix layout fix to the pinned source, including cached builds.
set(header "${RAYLIB_SOURCE_DIR}/src/rlgl.h")
file(READ "${header}" source)
set(original [=[void rlSetUniformMatrices(int locIndex, const Matrix *matrices, int count)
{
#if defined(GRAPHICS_API_OPENGL_33)
    glUniformMatrix4fv(locIndex, count, true, (const float *)matrices);
#elif defined(GRAPHICS_API_OPENGL_ES2)
    // WARNING: WebGL does not support Matrix transpose ("true" parameter)
    // REF: https://developer.mozilla.org/en-US/docs/Web/API/WebGLRenderingContext/uniformMatrix
    glUniformMatrix4fv(locIndex, count, false, (const float *)matrices);
#endif
}]=])
set(corrected [=[void rlSetUniformMatrices(int locIndex, const Matrix *matrices, int count)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    // Crownless: pack each matrix in the same order as rlSetUniformMatrix.
    if ((matrices == NULL) || (count <= 0) ||
        ((size_t)count > (size_t)-1/sizeof(rl_float16))) return;
    rl_float16 local[32];
    rl_float16 *packed = (count <= 32)? local :
        (rl_float16 *)RL_MALLOC((size_t)count*sizeof(rl_float16));
    if (packed == NULL) return;
    for (int i = 0; i < count; i++) packed[i] = rlMatrixToFloatV(matrices[i]);
    glUniformMatrix4fv(locIndex, count, false, packed[0].v);
    if (packed != local) RL_FREE(packed);
#endif
}]=])
string(FIND "${source}" "${corrected}" corrected_at)
if(corrected_at GREATER_EQUAL 0)
    return()
endif()
string(FIND "${source}" "${original}" original_at)
if(original_at LESS 0)
    message(FATAL_ERROR "The raylib matrix upload changed. Review the pinned-source patch.")
endif()
string(REPLACE "${original}" "${corrected}" source "${source}")
file(WRITE "${header}" "${source}")
