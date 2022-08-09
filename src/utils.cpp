#include "utils.hpp"
#include <span>
#include <glm/gtx/euler_angles.hpp>

char buffer[SCRATCH_BUFFER_SIZE];
std::span<u8> bufferU8((u8*)buffer, SCRATCH_BUFFER_SIZE);

namespace shader_srcs
{

extern ConstStr header =
"#version 330\n"
"#define PI 3.1415926535897932\n";

}

static const char* geGlErrStr(GLenum const err)
{
	switch (err) {
	case GL_NO_ERROR: return "GL_NO_ERROR";
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
	case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
	case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
	default:
		assert(!"unknown error");
		return nullptr;
	}
}

void glErrorCallback(const char* name, void* funcptr, int len_args, ...) {
	GLenum error_code;
	error_code = glad_glGetError();
	if (error_code != GL_NO_ERROR) {
		fprintf(stderr, "ERROR %s in %s\n", geGlErrStr(error_code), name);
		assert(false);
	}
}

// --- shader utils ---

char* checkCompileErrors(u32 shad, std::span<char> buffer)
{
    i32 ok;
    glGetShaderiv(shad, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLsizei outSize;
        glGetShaderInfoLog(shad, buffer.size(), &outSize, buffer.data());
        return buffer.data();
    }
    return nullptr;
}

char* checkLinkErrors(u32 prog, std::span<char> buffer)
{
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(prog, buffer.size(), nullptr, buffer.data());
        return buffer.data();
    }
    return nullptr;
}

static void printCodeWithLines(std::span<const char*> srcs)
{
    printf("%4d| ", 1);
    int line = 2;
    for (const char* s : srcs)
    {
        int start = 0;
        int end = 0;
        while (s[end]) {
            if (s[end] == '\n') {
                printf("%.*s\n", end - start, s + start);
                printf("%4d| ", line);
                start = end = end + 1;
                line++;
            }
            else
                end++;
        }
    }
    printf("\n");
}

void printShaderCodeWithHeader(const char* src)
{
    const char* srcs[2] = { shader_srcs::header, src };
    printCodeWithLines(srcs);
}

u32 easyCreateShader(const char* name, const char* src, GLenum type)
{
    static ConstStr s_shaderTypeNames[] = { "VERT", "FRAG", "GEOM" };
    const char* typeName = nullptr;
    switch (type) {
    case GL_VERTEX_SHADER:
        typeName = s_shaderTypeNames[0]; break;
    case GL_FRAGMENT_SHADER:
        typeName = s_shaderTypeNames[1]; break;
    case GL_GEOMETRY_SHADER:
        typeName = s_shaderTypeNames[2]; break;
    default:
        assert(false);
    }

    const u32 shad = glCreateShader(type);
    ConstStr srcs[] = { shader_srcs::header, src };
    glShaderSource(shad, 2, srcs, nullptr);
    glCompileShader(shad);
    if (const char* errMsg = checkCompileErrors(shad, buffer)) {
        printf("Error in '%s'(%s):\n%s", name, typeName, errMsg);
        printShaderCodeWithHeader(src);
        assert(false);
    }
    return shad;
}

u32 easyCreateShaderProg(const char* name, const char* vertShadSrc, const char* fragShadSrc, u32 vertShad, u32 fragShad)
{
    u32 prog = glCreateProgram();

    glAttachShader(prog, vertShad);
    glAttachShader(prog, fragShad);
    defer(
        glDetachShader(prog, vertShad);
    glDetachShader(prog, fragShad);
    );

    glLinkProgram(prog);
    if (const char* errMsg = checkLinkErrors(prog, buffer)) {
        printf("%s\n", errMsg);
        printf("Vertex Shader:\n");
        printShaderCodeWithHeader(vertShadSrc);
        printf("Fragment Shader:\n");
        printShaderCodeWithHeader(fragShadSrc);
        assert(false);
    }

    return prog;
}

u32 easyCreateShaderProg(const char* name, const char* vertShadSrc, const char* fragShadSrc)
{
    const u32 vertShad = easyCreateShader(name, vertShadSrc, GL_VERTEX_SHADER);
    const u32 fragShad = easyCreateShader(name, fragShadSrc, GL_FRAGMENT_SHADER);
    const u32 prog = easyCreateShaderProg(name, vertShadSrc, fragShadSrc, vertShad, fragShad);
    glDeleteShader(vertShad);
    glDeleteShader(fragShad);
    return prog;
}


static const vec3 s_icosahedronVerts[12] = {
    {0.0000000000000000000000000, 1.0000000000000000000000000, 0.0000000000000000000000000},
    {0.0000000000000000000000000, 0.4472136497497558593750000, -0.8944272398948669433593750},
    {-0.8506507873535156250000000, 0.4472136497497558593750000, -0.2763932347297668457031250},
    {-0.5257311463356018066406250, 0.4472136497497558593750000, 0.7236067652702331542968750},
    {0.5257310271263122558593750, 0.4472136497497558593750000, 0.7236068248748779296875000},
    {0.8506507873535156250000000, 0.4472136497497558593750000, -0.2763930857181549072265625},
    {-0.5257310867309570312500000, -0.4472136497497558593750000, -0.7236068248748779296875000},
    {-0.8506507873535156250000000, -0.4472136497497558593750000, 0.2763931453227996826171875},
    {-0.0000000626720364493849047, -0.4472136497497558593750000, 0.8944271802902221679687500},
    {0.8506507277488708496093750, -0.4472136497497558593750000, 0.2763932645320892333984375},
    {0.5257312059402465820312500, -0.4472136497497558593750000, -0.7236067056655883789062500},
    {-0.0000000000000000000000000, -1.0000000000000000000000000, -0.0000000000000000000000000},
};

void createIcoSphereMeshData(u32& numVerts, u32& numInds, vec3* verts, u32* inds, u32 subDivs)
{
    const int fnl = 1 << subDivs; // num levels per face
    numVerts =
        2 + // top and bot verts
        2 * 5 * (fnl-1) * fnl / 2 + // top and bot caps
        5 * (fnl+1) * fnl; // middle

    numInds = 3 * (20 * (1 << (2 * subDivs)));

    if(verts)
    {
        int vi = 0;
        auto addVert = [&](vec3 p) {
            verts[vi++] = glm::normalize(p);
        };
        addVert(s_icosahedronVerts[0]);

        using glm::mix;
        for(int l = 1; l < fnl; l++) {
            const float vertPercent = float(l) / fnl;
            for(int f = 0; f < 5; f++) {
                const vec3 pLeft = mix(s_icosahedronVerts[0], s_icosahedronVerts[1+f], vertPercent);
                const vec3 pRight = mix(s_icosahedronVerts[0], s_icosahedronVerts[1+(f+1)%5], vertPercent);
                for(int x = 0; x < l; x++) {
                    const vec3 p = mix(pLeft, pRight, float(x) / l);
                    addVert(p);
                }
            }
        }

        for(int l = 0 ; l < fnl; l++) {
            const float vertPercent = float(l) / fnl;
            for(int f = 0; f < 5; f++) {
                const vec3 topLeft = s_icosahedronVerts[1+f];
                const vec3 topRight = s_icosahedronVerts[1+(f+1)%5];
                const vec3 botLeft = s_icosahedronVerts[6+f];
                const vec3 botRight = s_icosahedronVerts[6+(f+1)%5];
                const vec3 left = mix(topLeft, botLeft, vertPercent);
                const vec3 mid = mix(topRight, botLeft, vertPercent);
                const vec3 right = mix(topRight, botRight, vertPercent);
                for(int x = 0; x < fnl-l; x++) {
                    const vec3 p = mix(left, mid, float(x) / (fnl-l));
                    addVert(p);
                }
                for(int x = 0; x < l; x++) {
                    const vec3 p = mix(mid, right, float(x) / l);
                    addVert(p);
                }
            }
        }

        for(int l = 0; l < fnl; l++) {
            const float vertPercent = float(l) / fnl;
            for(int f = 0; f < 5; f++) {
                const vec3 pLeft = mix(s_icosahedronVerts[6+f], s_icosahedronVerts[11], vertPercent);
                const vec3 pRight = mix(s_icosahedronVerts[6+(f+1)%5], s_icosahedronVerts[11], vertPercent);
                for(int x = 0; x < fnl-l; x++) {
                    const vec3 p = mix(pLeft, pRight, float(x) / (fnl-l));
                    addVert(p);
                }
            }
        }

        addVert(s_icosahedronVerts[11]);
        assert(vi == (int)numVerts);
    }

    if(inds)
    {
        int i = 0;
        auto addTri = [&](int i0, int i1, int i2) {
            inds[i++] = i0;
            inds[i++] = i1;
            inds[i++] = i2;
        };

        // top
        for(int f = 0; f < 5; f++)
            addTri(0, 1+f, 1+(f+1)%5);

        int rowOffset = 1;
        for(int l = 1; l < fnl; l++) {
            const int rowLen = l*5;
            const int nextRowLen = (l+1)*5;
            for(int f = 0; f < 5; f++) {
                for(int x = 0; x < l; x++) {
                    const int topLeft = rowOffset + l*f + x;
                    const int topRight = rowOffset + (l*f + x + 1) % rowLen;
                    const int botLeft = rowOffset + rowLen + (l+1) * f + x;
                    addTri(
                        topLeft,
                        botLeft,
                        botLeft + 1);
                    addTri(
                        topLeft,
                        botLeft + 1,
                        topRight);
                }
                addTri(
                    rowOffset + (l*(f+1)) % rowLen, // review!
                    rowOffset + rowLen + (l+1) * f + l,
                    rowOffset + rowLen + ((l+1) * f + l + 1) % nextRowLen);
            }
            rowOffset += rowLen;
        }

        { // middle
            const int rowLen = 5 * fnl;
            for(int l = 0; l < fnl; l++) {
                for(int x = 0; x < rowLen; x++) {
                    const int topLeft = rowOffset + x;
                    const int topRight = rowOffset + (x+1) % rowLen;
                    const int botLeft = rowOffset + rowLen + x;
                    const int botRight = rowOffset + rowLen + (x+1) % rowLen;
                    addTri(topLeft, botLeft, topRight);
                    addTri(topRight, botLeft, botRight);
                }
                rowOffset += rowLen;
            }
        }

        // bottom
        for(int l = 0; l < fnl-1; l++) {
            const int faceLen = fnl - l;
            const int rowLen = faceLen*5;
            const int nextRowLen = (faceLen-1)*5;
            for(int f = 0; f < 5; f++) {
                for(int x = 0; x < fnl-l-1; x++) {
                    const int topLeft = rowOffset + faceLen*f + x;
                    const int topRight = rowOffset + faceLen*f + x + 1;
                    const int botLeft = rowOffset + rowLen + (faceLen-1) * f + x;
                    const int botRight = rowOffset + rowLen + ((faceLen-1) * f + x + 1) % nextRowLen;
                    addTri(topLeft, botLeft, topRight);
                    addTri(topRight, botLeft, botRight);
                }
                addTri(
                    rowOffset + faceLen*(f+1) - 1,
                    rowOffset + rowLen + ((faceLen-1) * (f+1)) % nextRowLen,
                    rowOffset + faceLen*(f+1) % rowLen);
            }
            rowOffset += rowLen;
        }
        for(int f = 0; f < 5; f++)
            addTri(numVerts-6+f, numVerts-1, numVerts-6 + (f+1)%5);

        assert(i == (int)numInds);
    }
}

void createIcoSphereMesh(u32& vao, u32& vbo, u32& ebo, u32& numInds, u32 subDivs)
{
    u32 numVerts;
    createIcoSphereMeshData(numVerts, numInds, nullptr, nullptr, subDivs);
    vec3* positions = new vec3[numVerts];
    defer(delete[] positions);
    u32* inds = new u32[numInds];
    defer(delete[] inds);
    createIcoSphereMeshData(numVerts, numInds, positions, inds, subDivs);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, numVerts * sizeof(vec3), positions, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1); // normals
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numInds * sizeof(u32), inds, GL_STATIC_DRAW);
}

GLenum toGl(cgltf_component_type t) {
    switch (t)
    {
    case cgltf_component_type_r_8:
        return GL_BYTE;
    case cgltf_component_type_r_8u:
        return GL_UNSIGNED_BYTE;
    case cgltf_component_type_r_16:
        return GL_SHORT;
    case cgltf_component_type_r_16u:
        return GL_UNSIGNED_SHORT;
    case cgltf_component_type_r_32u:
        return GL_UNSIGNED_INT;
    case cgltf_component_type_r_32f:
        return GL_FLOAT;
    default:
        assert(false);
        return -1;
    }
}

GLenum toGl(cgltf_primitive_type t) {
    switch (t)
    {
    case cgltf_primitive_type_points:
        return GL_POINTS;
    case cgltf_primitive_type_lines:
        return GL_LINES;
    case cgltf_primitive_type_line_loop:
        return GL_LINE_LOOP;
    case cgltf_primitive_type_line_strip:
        return GL_LINE_STRIP;
    case cgltf_primitive_type_triangles:
        return GL_TRIANGLES;
    case cgltf_primitive_type_triangle_strip:
        return GL_TRIANGLE_STRIP;
    case cgltf_primitive_type_triangle_fan:
        return GL_TRIANGLE_FAN;
    default:
        assert(false);
        return -1;
    }
}

u32 getAttribLocation(cgltf_attribute_type t, u32 i) {
    switch (t) {
    case cgltf_attribute_type_position:
    case cgltf_attribute_type_normal:
    case cgltf_attribute_type_tangent:
        assert(i == 0);
        return t - 1;
    case cgltf_attribute_type_texcoord:
    case cgltf_attribute_type_color:
    case cgltf_attribute_type_joints:
    case cgltf_attribute_type_weights:
        assert(i < 16);
        return 3 + (t - cgltf_attribute_type_texcoord) * 16 + i;
    default:
        assert(false);
    }
}

void uriToPath(std::span<char> buffer, const char* gltfFilePath, const char* uri)
{
    int lastSlash = -1;
    for (int i = 0; gltfFilePath[i]; i++)
        if (gltfFilePath[i] == '/' || gltfFilePath[i] == '\\')
            lastSlash = i;
    assert(buffer.size() >= lastSlash + 1 + strlen(uri));

    int i;
    for (i = 0; i <= lastSlash; i++)
        buffer[i] = gltfFilePath[i];

    for (int j = 0; uri[j]; j++)
        buffer[i++] = uri[j];
    buffer[i] = '\0';
}

glm::mat3 randRotMtx(vec3 u)
{
    const float theta = acosf(2 * u[0] - 1);
    const float phi = 2 * PI * u[1];
    const float ct = cosf(theta);
    const vec3 axis(ct * sin(phi), sinf(theta), ct * cos(phi));
    const float angle = 2 * PI * u[2];
    return glm::rotate(mat4(1), angle, axis);
}