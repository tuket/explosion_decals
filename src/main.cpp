#include "utils.hpp"
#include <GLFW/glfw3.h>
#include <cgltf.h>
#include <vector>
#include <span>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>

const float CAMERA_NEAR_DIST = 0.02f;
const float CAMERA_FAR_DIST  = 100.f;

GLFWwindow* window;
static const cgltf_data* cgltfData = nullptr;

struct Vert {
    vec3 pos, normal;
    vec2 tc;
};
struct InstancingData {
    mat4 modelMtx;
};

namespace shader_srcs
{

ConstStr scene_vert =
R"GLSL(
layout(location = 0)in vec3 a_pos;
layout(location = 1)in vec3 a_normal;
layout(location = 3)in vec2 a_tc;

out vec3 v_pos;
out vec3 v_normal;
out vec2 v_tc;

uniform mat4 u_modelMtx;
uniform mat4 u_modelView;
uniform mat4 u_modelViewProj;

void main()
{
	gl_Position = u_modelViewProj * vec4(a_pos, 1);
    v_pos = (u_modelMtx * vec4(a_pos, 1)).xyz;
    v_normal = mat3(u_modelMtx) * a_normal;
	v_tc = a_tc;
}
)GLSL";

ConstStr scene_frag =
R"GLSL(
layout(location = 0) out vec4 o_color;

in vec3 v_pos;
in vec3 v_normal;
in vec2 v_tc;

uniform sampler2D u_tex;

void main()
{
    vec3 albedo = texture(u_tex, v_tc).rgb;
    vec3 normal = normalize(v_normal);
    const vec3 lightPos = vec3(0, 2, 0.2);
    vec3 lightIntensity = 2.0*vec3(0.8, 0.8, 0.9);
    vec3 pl = lightPos - v_pos;
    float dist2 = dot(pl, pl);
    lightIntensity *= 1.0 / dist2;
    vec3 l = normalize(pl);
    o_color = vec4(albedo * (0.3 +  lightIntensity * dot(l, normal)), 1);
}
)GLSL";

ConstStr decal_vert =
R"GLSL(
layout(location = 0)in vec3 a_pos;
layout(location = 2)in mat4 a_modelMtx;

out vec3 v_pos;
flat out vec3 v_spherePos;
flat out mat3 v_envRotation;

uniform mat4 u_viewProj;

void main()
{
	gl_Position = u_viewProj * a_modelMtx * vec4(a_pos, 1);
    v_pos = a_pos;
    v_spherePos = vec3(a_modelMtx[3]);
    v_envRotation = mat3(a_modelMtx);
}
)GLSL";

ConstStr decal_frag =
R"GLSL(
// The following noise code is adapted from https://www.shadertoy.com/view/XsX3zB, which is MIT licensed
// vvv-----------------------------------------------------------------------------------------------------
vec3 random3(vec3 c) {
	float j = 4096.0 * sin(dot(c, vec3(17, 59.4, 15)));
	vec3 r;
	r.z = fract(512.0 * j);
	j *= 0.125;
	r.x = fract(512.0 * j);
	j *= 0.125;
	r.y = fract(512.0 * j);
	return r - 0.5;
}

float simplex3d(vec3 p)
{
    const float F3 =  0.3333333;
    const float G3 =  0.1666667;

	 /* 1. find current tetrahedron T and it's four vertices */
	 /* s, s+i1, s+i2, s+1.0 - absolute skewed (integer) coordinates of T vertices */
	 /* x, x1, x2, x3 - unskewed coordinates of p relative to each of T vertices*/
	 
	 /* calculate s and x */
	 vec3 s = floor(p + dot(p, vec3(F3)));
	 vec3 x = p - s + dot(s, vec3(G3));
	 
	 /* calculate i1 and i2 */
	 vec3 e = step(vec3(0.0), x - x.yzx);
	 vec3 i1 = e*(1.0 - e.zxy);
	 vec3 i2 = 1.0 - e.zxy*(1.0 - e);
	 	
	 /* x1, x2, x3 */
	 vec3 x1 = x - i1 + G3;
	 vec3 x2 = x - i2 + 2.0*G3;
	 vec3 x3 = x - 1.0 + 3.0*G3;
	 
	 /* 2. find four surflets and store them in d */
	 vec4 w, d;
	 
	 /* calculate surflet weights */
	 w.x = dot(x, x);
	 w.y = dot(x1, x1);
	 w.z = dot(x2, x2);
	 w.w = dot(x3, x3);
	 
	 /* w fades from 0.6 at the center of the surflet to 0.0 at the margin */
	 w = max(0.6 - w, 0.0);
	 
	 /* calculate surflet components */
	 d.x = dot(random3(s), x);
	 d.y = dot(random3(s + i1), x1);
	 d.z = dot(random3(s + i2), x2);
	 d.w = dot(random3(s + 1.0), x3);
	 
	 /* multiply d by w^4 */
	 w *= w;
	 w *= w;
	 d *= w;
	 
	 /* 3. return the sum of the four surflets */
	 return dot(d, vec4(52.0));
}

// directional artifacts can be reduced by rotating each octave
float simplex3d_fractal(vec3 m)
{
    const mat3 rot1 = mat3(-0.37, 0.36, 0.85,-0.14,-0.93, 0.34,0.92, 0.01,0.4);
    const mat3 rot2 = mat3(-0.55,-0.39, 0.74, 0.33,-0.91,-0.24,0.77, 0.12,0.63);
    const mat3 rot3 = mat3(-0.71, 0.52,-0.47,-0.08,-0.72,-0.68,-0.7,-0.45,0.56);

    return   0.5333333*simplex3d(m*rot1)
			+0.2666667*simplex3d(2.0*m*rot2)
			+0.1333333*simplex3d(4.0*m*rot3)
			+0.0666667*simplex3d(8.0*m);
}
// ^^^---------------------------------------------------------------------------------------------------

layout(location = 0) out vec4 o_color;

in vec3 v_pos;
flat in vec3 v_spherePos; // position of the sphere in world space
flat in mat3 v_envRotation; // rotate the direction we sample the noise environment so not all the decals look the same

uniform vec2 u_invScreenSize;
uniform mat4 u_invViewProj;
//uniform vec3 u_spherePos; // position of the sphere in world space
uniform float u_sphereRad; // radius of the sphere
//uniform mat3 u_envRotation; // rotate the direction we sample the noise environment so not all the decals look the same
uniform sampler2D u_depthTex; // the depth buffer of the scene
uniform float u_noiseFreq; // noise frequency
uniform float u_centerBase; // base darkness so the decal is not too dim, specially at the center
uniform float u_exponent; // exponent for the distance attenuation
uniform int u_displayMode; // display mode for debugging (see DISPLAY_MODE_ constants below)

const int DISPLAY_MODE_DEFAULT = 0;
const int DISPLAY_MODE_CIRCLE = 1;
const int DISPLAY_MODE_SPHERE = 2;
const int DISPLAY_MODE_SPHERE_NOISE = 3;

vec3 calcWorldPosFromDepth(vec3 fc_depth)
{
    vec4 clipSpacePos = vec4(2.0 * fc_depth - 1.0, 1);
    vec4 worldSpacePos = u_invViewProj * clipSpacePos;
    worldSpacePos /= worldSpacePos.w;
    return worldSpacePos.xyz;
}

void main()
{
    vec2 fc = gl_FragCoord.xy * u_invScreenSize;
    float bgDepth = texture(u_depthTex, fc).r;
    vec3 bgPos = calcWorldPosFromDepth(vec3(fc, bgDepth));
    float d = distance(bgPos, v_spherePos);

    if(u_displayMode != DISPLAY_MODE_SPHERE && u_displayMode != DISPLAY_MODE_SPHERE_NOISE && d > u_sphereRad)
        discard;

    float r1 = 1.0 - d / u_sphereRad;
    float noise = simplex3d_fractal(u_noiseFreq * v_envRotation * normalize(bgPos - v_spherePos));
    float a = mix(0.0, u_centerBase + noise, pow(r1, u_exponent));
    o_color = vec4(0, 0, 0, a);
    if(u_displayMode == DISPLAY_MODE_SPHERE_NOISE) {
        noise = simplex3d_fractal(u_noiseFreq * v_envRotation * normalize(v_pos - v_spherePos));
        o_color = vec4(vec3(noise), 1); 
    }
    else if(u_displayMode != DISPLAY_MODE_DEFAULT) {
        a = (u_displayMode == DISPLAY_MODE_SPHERE && d <= u_sphereRad) ? 0.35 : 0.2;
        o_color = vec4(1, 0, 0, a);
    }
}
)GLSL";

}

static u32 fbo;
static u32 fb_colorRbo, fb_depthTex;

struct GltfGpuResources {
    std::vector<u32> buffers;
    std::vector<u32> textures;
    std::vector<std::vector<u32>> vaos; //[meshInd][primitiveInd]
};

GltfGpuResources modelResources;

struct SceneShader {
    u32 prog;
    struct Locs {
        u32 modelMtx,
            modelView,
            modelViewProj,
            tex;
    } locs;
};
static SceneShader sceneShader;

struct DecalShader {
    u32 prog;
    struct Locs {
        u32 invScreenSize,
            invViewProj,
            viewProj,
            spherePos,
            sphereRad,
            envRotation,
            depthTex,
            noiseFreq,
            centerBase,
            exponent,
            displayMode;
    } locs;
};
static DecalShader decalShader;

struct Camera {
    vec3 pos;
    float heading, pitch;
};
static Camera camera = {
    .pos = {0.873904645, 1.38394725, 2.47491980},
    .heading = 0.277831912,
    .pitch = -0.481122792,
};

struct Sphere {
    u32 vao, vbo, ebo;
    u32 instancingVbo;
    u32 numInds;
};
static Sphere sphere;

enum EDisplayMode : int {
    DISPLAY_MODE_DEFAULT,
    DISPLAY_MODE_CIRCLE,
    DISPLAY_MODE_SPHERE,
    DISPLAY_MODE_SPHERE_NOISE,
};

struct Params {
    float sphereRad;
    float noiseFreq;
    float centerBase;
    float exponent;
    EDisplayMode displayMode;
};
static Params params = {
    .sphereRad = 0.5,
    .noiseFreq = 7,
    .centerBase = 1.5,
    .exponent = 1.5,
    .displayMode = DISPLAY_MODE_DEFAULT,
};

struct Decals {
    std::vector<vec3> positions;
    std::vector<mat3> rotations;
    //std::vector<float> radiuses;
};
Decals decals = {
    .positions = {vec3(0, 0.045, 0)},
    .rotations = {mat3(1)},
    //.radiuses = {0.5},
};

static void glfwErrorCallback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void resizeFbo(int w, int h)
{
    glBindRenderbuffer(GL_RENDERBUFFER, fb_colorRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, w, h);

    glBindTexture(GL_TEXTURE_2D, fb_depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

    glViewport(0, 0, w, h);
    glScissor(0, 0, w, h);
}

static void drawMesh(const cgltf_data& data, const cgltf_mesh& mesh,
    const mat4& viewMtx, const mat4& viewProjMtx, const mat4& modelMtx)
{
    const size_t meshInd = &mesh - data.meshes;
    const auto modelView = viewMtx * modelMtx;
    const auto modelViewProj = viewProjMtx * modelMtx;

    for (size_t primitiveInd = 0; primitiveInd < mesh.primitives_count; primitiveInd++) {
        const auto& primitive = mesh.primitives[primitiveInd];
        if(primitive.material->double_sided)
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);
        assert(primitive.material->has_pbr_metallic_roughness);
        const auto& mr = primitive.material->pbr_metallic_roughness;
        glUseProgram(sceneShader.prog);
        glUniformMatrix4fv(sceneShader.locs.modelMtx, 1, GL_FALSE, &modelMtx[0][0]);
        glUniformMatrix4fv(sceneShader.locs.modelView, 1, GL_FALSE, &modelView[0][0]);
        glUniformMatrix4fv(sceneShader.locs.modelViewProj, 1, GL_FALSE, &modelViewProj[0][0]);
        const size_t albedoTexInd = mr.base_color_texture.texture - data.textures;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, modelResources.textures[albedoTexInd]);
        const u32 vao = modelResources.vaos[meshInd][primitiveInd];
        glBindVertexArray(vao);
        const auto primitiveType = toGl(primitive.type);
        if (primitive.indices) {
            const auto& indices = *primitive.indices;
            const GLenum intType = toGl(indices.component_type);
            const size_t offset = indices.offset + indices.buffer_view->offset;
            glDrawElements(primitiveType, primitive.indices->count, intType, (void*)offset);
        }
        else {
            const size_t numVerts = primitive.attributes[0].data->count;
            glDrawArrays(primitiveType, 0, numVerts);
        }
    }
}

static void drawNodeRecursive(const cgltf_data& data, const cgltf_node& node,
    const mat4& viewMtx, const mat4& viewProjMtx, const mat4& parentModelMtx = mat4(1))
{
    mat4 modelMtx;
    cgltf_node_transform_local(&node, &modelMtx[0][0]);
    modelMtx = parentModelMtx * modelMtx;
    
    if (node.mesh) {
        auto& mesh = *node.mesh;
        drawMesh(data, mesh, viewMtx, viewProjMtx, modelMtx);
    }
    for (size_t childInd = 0; childInd < node.children_count; childInd++)
        drawNodeRecursive(data, *node.children[childInd], viewMtx, viewProjMtx, modelMtx);
}

int main()
{
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window = glfwCreateWindow(1280, 720, "pcv", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (gladLoadGL() == 0) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }
    glad_set_post_callback(glErrorCallback);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int w, int h) { resizeFbo(w, h); });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {} );
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y) {} );
    glfwSetScrollCallback(window, [](GLFWwindow* window, double dx, double dy) {} );

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    });

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    {
        glGenRenderbuffers(1, &fb_colorRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, fb_colorRbo);
        
        glGenTextures(1, &fb_depthTex);
        glBindTexture(GL_TEXTURE_2D, fb_depthTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fb_colorRbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fb_depthTex, 0);
    }

    sceneShader.prog = easyCreateShaderProg("pbr", shader_srcs::scene_vert, shader_srcs::scene_frag);
    sceneShader.locs.modelMtx = glGetUniformLocation(sceneShader.prog, "u_modelMtx");
    sceneShader.locs.modelView = glGetUniformLocation(sceneShader.prog, "u_modelView");
    sceneShader.locs.modelViewProj = glGetUniformLocation(sceneShader.prog, "u_modelViewProj");
    sceneShader.locs.tex = glGetUniformLocation(sceneShader.prog, "u_tex");
    glUseProgram(sceneShader.prog);
    glUniform1i(sceneShader.locs.tex, 0);

    decalShader.prog = easyCreateShaderProg("decal", shader_srcs::decal_vert, shader_srcs::decal_frag);
    decalShader.locs.invScreenSize = glGetUniformLocation(decalShader.prog, "u_invScreenSize");
    decalShader.locs.invViewProj = glGetUniformLocation(decalShader.prog, "u_invViewProj");
    decalShader.locs.viewProj = glGetUniformLocation(decalShader.prog, "u_viewProj");
    decalShader.locs.spherePos = glGetUniformLocation(decalShader.prog, "u_spherePos");
    decalShader.locs.sphereRad = glGetUniformLocation(decalShader.prog, "u_sphereRad");
    decalShader.locs.envRotation = glGetUniformLocation(decalShader.prog, "u_envRotation");
    decalShader.locs.depthTex = glGetUniformLocation(decalShader.prog, "u_depthTex");
    decalShader.locs.noiseFreq = glGetUniformLocation(decalShader.prog, "u_noiseFreq");
    decalShader.locs.centerBase = glGetUniformLocation(decalShader.prog, "u_centerBase");
    decalShader.locs.exponent = glGetUniformLocation(decalShader.prog, "u_exponent");
    decalShader.locs.displayMode = glGetUniformLocation(decalShader.prog, "u_displayMode");

    createIcoSphereMesh(sphere.vao, sphere.vbo, sphere.ebo, sphere.numInds, 2);
    {
        glGenBuffers(1, &sphere.instancingVbo);
        glBindBuffer(GL_ARRAY_BUFFER, sphere.instancingVbo);
        
        // modelMtx
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void*)(i * sizeof(vec4)) );
            glVertexAttribDivisor(2 + i, 1);
        }
    }


    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    {
        const char* fileName = "data/room.glb";
        cgltf_options options = {};
        cgltf_data* data = nullptr;
        cgltf_result res = cgltf_parse_file(&options, fileName, &data);
        if (res != cgltf_result_success) {
            printf("Error loading gltf file: %s\n", fileName);
        }
        res = cgltf_load_buffers(&options, data, fileName);
        if (res != cgltf_result_success) {
            printf("Error loading gltf buffers\n");
        }
        cgltfData = data;

        modelResources.buffers.resize(data->buffers_count);
        glGenBuffers(data->buffers_count, &modelResources.buffers[0]);
        for (size_t bufferInd = 0; bufferInd < data->buffers_count; bufferInd++) {
            const auto& buffer = data->buffers[bufferInd];
            u32& bo = modelResources.buffers[bufferInd];
            glBindBuffer(GL_COPY_WRITE_BUFFER, bo);
            glBufferData(GL_COPY_WRITE_BUFFER, buffer.size, buffer.data, GL_STATIC_DRAW);
        }

        modelResources.vaos.resize(data->meshes_count);
        for (size_t meshInd = 0; meshInd < data->meshes_count; meshInd++) {
            auto& mesh = data->meshes[meshInd];
            modelResources.vaos[meshInd].resize(mesh.primitives_count);
            glGenVertexArrays(mesh.primitives_count, &modelResources.vaos[meshInd][0]);
            for (size_t primitiveInd = 0; primitiveInd < mesh.primitives_count; primitiveInd++) {
                auto& vao = modelResources.vaos[meshInd][primitiveInd];
                glBindVertexArray(vao);
                auto& primitives = mesh.primitives[primitiveInd];
                if (primitives.indices) {
                    auto& indices = *primitives.indices;
                    assert(indices.type == cgltf_type_scalar);
                    auto bufferInd = indices.buffer_view->buffer - data->buffers;
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelResources.buffers[bufferInd]);
                }
                for (size_t attribInd = 0; attribInd < primitives.attributes_count; attribInd++) {
                    auto& attrib = primitives.attributes[attribInd];
                    const auto bufferInd = attrib.data->buffer_view->buffer - data->buffers;
                    const u32 attribLoc = getAttribLocation(attrib.type, attrib.index);
                    const u32 numComps = cgltf_num_components(attrib.data->type);
                    const GLenum compType = toGl(attrib.data->component_type);
                    const u32 normalized = attrib.data->normalized;
                    const size_t offset = attrib.data->offset + attrib.data->buffer_view->offset;
                    glEnableVertexAttribArray(attribLoc);
                    glBindBuffer(GL_ARRAY_BUFFER, modelResources.buffers[bufferInd]);
                    glVertexAttribPointer(attribLoc, numComps, compType, normalized, attrib.data->stride, (void*)offset);
                }
            }
        }

        modelResources.textures.resize(data->textures_count);
        glGenTextures(2, &modelResources.textures[0]);
        for (size_t texInd = 0; texInd < data->textures_count; texInd++) {
            u32& tex = modelResources.textures[texInd];
            auto& texInfo = data->textures[texInd];
            auto& image = *texInfo.image;
            u8* imgData = nullptr;
            int w, h, nc;
            if (image.buffer_view) {
                const auto* bufferView = image.buffer_view;
                const auto* data = (u8*)bufferView->buffer->data + bufferView->offset;
                const size_t size = bufferView->size;
                imgData = stbi_load_from_memory(data, size, &w, &h, &nc, 4);
            }
            else {
                char path[256];
                uriToPath(path, fileName, image.uri);
                imgData = stbi_load(path, &w, &h, &nc, 4);
            }
            glBindTexture(GL_TEXTURE_2D, modelResources.textures[texInd]);
            const GLenum internalFormat = GL_RGBA8;
            const GLenum format = GL_RGBA;
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, GL_UNSIGNED_BYTE, imgData);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    glClearColor(0.4, 0.4, 0.4, 0);

    bool firstFrame = true;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        int screenW, screenH;
        glfwGetFramebufferSize(window, &screenW, &screenH);
        if (screenW <= 0 || screenH <= 0)
            continue;
        const float aspectRatio = float(screenW) / screenH;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        ImGuizmo::SetRect(0, 0, screenW, screenH);

        if (firstFrame)
            resizeFbo(screenW, screenH);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
            static double prevMx = 0, prevMy = 0;
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            if (!ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
                const vec2 d = { mx - prevMx, my - prevMy };
                camera.heading -= d.x / screenH;
                camera.pitch -= d.y / screenH;
            }

            prevMx = mx;
            prevMy = my;
        }

        vec3 moveDir(0);
        if (glfwGetKey(window, GLFW_KEY_W))
            moveDir.z -= 1;
        if (glfwGetKey(window, GLFW_KEY_S))
            moveDir.z += 1;
        if (glfwGetKey(window, GLFW_KEY_A))
            moveDir.x -= 1;
        if (glfwGetKey(window, GLFW_KEY_D))
            moveDir.x += 1;
        if (moveDir.x != 0 && moveDir.z != 0)
            moveDir = normalize(moveDir);
        const auto camRot = glm::eulerAngleYX(camera.heading, camera.pitch);
        moveDir = mat3(camRot) * moveDir;
        const float speedBoost = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) ? 3.f : 1.f;;
        camera.pos += 0.02f * speedBoost * moveDir;

        mat4 viewMtx, projMtx;
        {
            viewMtx =
                glm::transpose(camRot) *
                glm::translate(mat4(1), -camera.pos);

            projMtx = glm::perspective(1.f, aspectRatio, CAMERA_NEAR_DIST, CAMERA_FAR_DIST);
        }
        const auto viewProjMtx = projMtx * viewMtx;

        // -- draw the room --
        assert(cgltfData->scenes_count);
        auto& scene = cgltfData->scenes[0];
        for (size_t nodeInd = 0; nodeInd < scene.nodes_count; nodeInd++)
            drawNodeRecursive(*cgltfData, *scene.nodes[nodeInd], viewMtx, viewProjMtx);

        // -- draw decals --
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glCullFace(GL_FRONT); // This is so the sphere doesn't get culled when the camera is inside it
        glDepthFunc(GL_GREATER); // Depth testing optimized for spheres that are usually above the surface
        glUseProgram(decalShader.prog);
        const auto invViewProj = inverse(viewProjMtx);
        glUniformMatrix4fv(decalShader.locs.viewProj, 1, GL_FALSE, &viewProjMtx[0][0]);
        glUniformMatrix4fv(decalShader.locs.invViewProj, 1, GL_FALSE, &invViewProj[0][0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, fb_depthTex);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(decalShader.locs.depthTex, 1);
        glUniform1f(decalShader.locs.sphereRad, params.sphereRad);
        glUniform1f(decalShader.locs.noiseFreq, params.noiseFreq);
        glUniform1f(decalShader.locs.centerBase, params.centerBase);
        glUniform1f(decalShader.locs.exponent, params.exponent);
        glUniform1i(decalShader.locs.displayMode, params.displayMode);
        glUniform2f(decalShader.locs.invScreenSize, 1.f / screenW, 1.f / screenH);
        std::vector<InstancingData> instancingData;
        for (size_t i = 0; i < decals.positions.size(); i++) {
            mat4 modelMtx(1);
            modelMtx = glm::translate(modelMtx, decals.positions[i]);
            modelMtx = modelMtx * mat4(decals.rotations[i]);
            instancingData.push_back({ modelMtx });
        }
        glBindVertexArray(sphere.vao);
        glBindBuffer(GL_ARRAY_BUFFER, sphere.instancingVbo);
        glBufferData(GL_ARRAY_BUFFER, instancingData.size() * sizeof(InstancingData), instancingData.data(), GL_STREAM_DRAW);
        glDrawElementsInstanced(GL_TRIANGLES, sphere.numInds, GL_UNSIGNED_INT, nullptr, instancingData.size());
        glDepthFunc(GL_LESS); // restore default depth testing
        glCullFace(GL_BACK); // restore normal culling

        // -- blit from the fbo to the window --
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(
            0, 0, screenW, screenH,
            0, 0, screenW, screenH,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);

        //ImGui::ShowDemoWindow(nullptr);

        ImGui::SetNextWindowPos({ 0, 0 }, ImGuiCond_Once);
        ImGui::SetNextWindowSize({ 400, 240 }, ImGuiCond_Once);
        if (ImGui::Begin("window"))
        {
            const char* displayModes[] = { "default", "circle", "sphere", "noise" };
            ImGui::Combo("display mode", (int*)&params.displayMode, displayModes, std::size(displayModes));
            ImGui::SliderFloat("radius", &params.sphereRad, 0, 3, "%.4f", ImGuiSliderFlags_Logarithmic);
            ImGui::DragFloat("noise frequency", &params.noiseFreq, 0.01, 0, FLT_MAX);
            ImGui::DragFloat("center base", &params.centerBase, 0.01, 0, FLT_MAX);
            ImGui::DragFloat("exponent", &params.exponent, 0.01, 0, FLT_MAX);

            for (size_t i = 0; i < decals.positions.size(); i++)
            {
                ImGuizmo::SetID(i);
                auto modelMtx = glm::translate(mat4(1), decals.positions[i]);
                if (ImGuizmo::Manipulate(&viewMtx[0][0], &projMtx[0][0],
                    ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::WORLD,
                    &modelMtx[0][0], NULL, NULL))
                {
                    decals.positions[i] = vec3(modelMtx[3]);
                }
            }
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        int toDelete = -1;
        if (ImGui::TreeNode("decals"))
        {
            for (size_t i = 0; i < decals.positions.size(); i++)
            {
                ImGui::PushID(i);
                ImGui::DragFloat3("pos", &decals.positions[i][0], 0.1);
                ImGui::SameLine();
                if (ImGui::Button("delete"))
                    toDelete = i;
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if (toDelete >= 0) {
            decals.positions.erase(decals.positions.begin() + toDelete);
            decals.rotations.erase(decals.rotations.begin() + toDelete);
        }

        if (ImGui::Button("Add Decal"))
        {
            auto randFloat = []() { return float(rand()) / RAND_MAX; };

            vec3 pos(glm::mix(-2.4f, +2.4f, randFloat()), 0.045, glm::mix(-2.4f, +2.4f, randFloat()));
            decals.positions.push_back(pos);

            const mat3 rot = randRotMtx({ randFloat(), randFloat(), randFloat() });
            decals.rotations.push_back(rot);
        }

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        firstFrame = false;
        glfwSwapBuffers(window);
    }
}