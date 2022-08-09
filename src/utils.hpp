#pragma once

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <span>
#include <glm/glm.hpp>
#define GLAD_DEBUG
#include <glad/glad.h>
#include <cgltf.h>

typedef uint8_t u8;
typedef int32_t i32;
typedef uint32_t u32;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat2;
using glm::mat3;
using glm::mat4;
typedef const char* const ConstStr;
constexpr float PI = 3.1415926535897932f;

constexpr int SCRATCH_BUFFER_SIZE = 4 * 1024 * 1024;
extern char buffer[SCRATCH_BUFFER_SIZE];
extern std::span<u8> bufferU8;
template <typename T> auto bufferSpan(size_t offset = 0) { return std::span<T>((T*)(buffer + offset), (SCRATCH_BUFFER_SIZE - offset) / sizeof(T)); }
typedef const char* const ConstStr;

void glErrorCallback(const char* name, void* funcptr, int len_args, ...);

char* checkCompileErrors(u32 shad, std::span<char> buffer);
char* checkLinkErrors(u32 prog, std::span<char> buffer);
void printShaderCodeWithHeader(const char* src);
u32 easyCreateShader(const char* name, const char* src, GLenum type);
u32 easyCreateShaderProg(const char* name, const char* vertShadSrc, const char* fragShadSrc);
u32 easyCreateShaderProg(const char* name, const char* vertShadSrc, const char* fragShadSrc, u32 vertShad, u32 fragShad);

void createIcoSphereMeshData(u32& numVerts, u32& numInds, glm::vec3* verts, u32* inds, u32 subDivs);
void createIcoSphereMesh(u32& vao, u32& vbo, u32& ebo, u32& numInds, u32 subDivs);

extern const float k_icosaedronVerts[20 * 3];

GLenum toGl(cgltf_component_type t);
GLenum toGl(cgltf_primitive_type t);
u32 getAttribLocation(cgltf_attribute_type t, u32 i);
void uriToPath(std::span<char> buffer, const char* gltfFilePath, const char* uri);

glm::mat3 randRotMtx(vec3 u);

static inline float distance2 (vec3 a, vec3 b) {
    vec3 ab = a - b;
    return dot(ab, ab);
};

// -- DEFER --
template <typename F>
struct _Defer {
    F f;
    _Defer(F f) : f(f) {}
    ~_Defer() { f(); }
};

template <typename F>
_Defer<F> _defer_func(F f) {
    return _Defer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = _defer_func([&](){code;})